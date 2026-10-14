// main.cpp — LFR v2: fiação dos módulos e tasks. Nenhuma regra de negócio aqui.
// Core 1: task "control" a 500 Hz (sensor → PID → motor), prioridade alta.
// Core 0: task "comm" a 100 Hz (BLE, comandos, telemetria), junto da pilha NimBLE.
#ifndef NATIVE_BUILD

#include <Arduino.h>
#include <SPI.h>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "config.h"
#include "comm/Annunciator.h"
#include "comm/BLETuner.h"
#include "comm/CommandHandler.h"
#include "comm/CommandProtocol.h"
#include "comm/Mailbox.h"
#include "comm/PeriodicTimer.h"
#include "comm/Telemetry.h"
#include "config/ConfigStore.h"
#include "config/RobotConfig.h"
#include "motors/MotorDriver.h"
#include "sensors/BatteryMonitor.h"
#include "sensors/ButtonInput.h"
#include "sensors/Calibration.h"
#include "sensors/Encoder.h"
#include "sensors/SensorArray.h"
#include "sensors/VelocityEstimator.h"
#include "storage/NvsStore.h"
#include "strategy/ControlLoop.h"
#include "strategy/RobotStateMachine.h"

static constexpr const char* DEVICE_NAME = "LFR-RACER-01";
static constexpr const char* FW_VERSION  = "4.0.0";
static constexpr int      OVERRUN_LIMIT  = 5;     // ciclos seguidos acima do período
static constexpr int      BATTERY_EVERY  = 50;    // 50 × 2 ms = 100 ms
static constexpr int      STATUS_EVERY   = 250;   // 250 × 2 ms = 500 ms
static constexpr uint32_t TELEMETRY_MS   = 33;    // ~30 Hz

// ── Hardware ───────────────────────────────────────────────────────────────
static Calibration calibration(SENSOR_COUNT);
static SensorArray sensors(PIN_SPI_CS, SPI_FREQ, SENSOR_COUNT, calibration);
static MotorDriver::Config cfgA = {
    PIN_MOTOR_A_PWM, PIN_MOTOR_A_IN1, PIN_MOTOR_A_IN2, 0, PWM_FREQ, PWM_RESOLUTION
};
static MotorDriver::Config cfgB = {
    PIN_MOTOR_B_PWM, PIN_MOTOR_B_IN1, PIN_MOTOR_B_IN2, 1, PWM_FREQ, PWM_RESOLUTION
};
static MotorDriver motors(cfgA, cfgB, PIN_MOTOR_STBY);
static Encoder encL(PIN_ENC_LEFT_A, PIN_ENC_LEFT_B);
static Encoder encR(PIN_ENC_RIGHT_A, PIN_ENC_RIGHT_B);
static NvsStore nvs;
static VelocityEstimator velL(encL, nvs, "left");
static VelocityEstimator velR(encR, nvs, "right");

// ── Lógica ─────────────────────────────────────────────────────────────────
static ConfigStore    configStore(nvs);
static ControlLoop    control(calibration, PWM_MAX);
static BatteryMonitor battery(VBAT_DIVIDER_RATIO);
static ButtonInput    button;
static Annunciator    annunciator;
static BLETuner       ble;
static RobotConfig    g_liveCfg;          // dono: task comm (depois do setup)

// ── Canais entre núcleos ───────────────────────────────────────────────────
static Mailbox<RobotConfig>    g_cfgBox;  // comm → control
static Mailbox<TelemetryFrame> g_telBox;  // control → comm
static Mailbox<StatusFrame>    g_staBox;  // control → comm
static Mailbox<CalReport>      g_calBox;  // control → comm
static QueueHandle_t           g_ctlQueue = nullptr;  // comm → control (ControlMsg)
static std::atomic<uint8_t>    g_state{0};
static bool g_cfgErr = false;
static bool g_calValidAtBoot = false;
// Handles para medir a folga de pilha (info "stk", E-2). Gravados por xTaskCreatePinnedToCore
// antes de a task comm existir; depois só lidos.
static TaskHandle_t g_controlTask = nullptr;
static TaskHandle_t g_commTask = nullptr;

static void applyRequest(const ControlMsg& msg, RobotStateMachine& sm, const SmInputs& in,
                         uint32_t nowMs) {
    switch (msg.req) {
        case ControlRequest::START:      sm.handle(SmEvent::CMD_START, in, nowMs); break;
        case ControlRequest::STOP:       sm.handle(SmEvent::CMD_STOP, in, nowMs); break;
        case ControlRequest::RESET:      sm.handle(SmEvent::CMD_RESET, in, nowMs); break;
        case ControlRequest::SENSOR_CAL: sm.handle(SmEvent::CMD_SENSOR_CAL, in, nowMs); break;
        case ControlRequest::ENC_CAL_START:
            if (!sm.isActive()) {
                (msg.leftSide ? velL : velR).startCalibration();
                annunciator.beep(1, 50, 0, nowMs);
            }
            break;
        case ControlRequest::ENC_CAL_FINISH:
            if (!sm.isActive()) {
                VelocityEstimator& v = msg.leftSide ? velL : velR;
                CalReport r;
                r.kind = CalReport::Kind::ENCODER;
                r.ok = v.finishCalibration(msg.rotations);
                r.left = msg.leftSide;
                r.ppr = v.getEffectivePPR();
                g_calBox.post(r);
                annunciator.beep(r.ok ? 1 : 3, r.ok ? 200 : 120, 120, nowMs);
            }
            break;
        case ControlRequest::NONE:
            break;
    }
}

static TelemetryFrame buildTelemetry(const int* raw, const ControlOutputs& out,
                                     uint32_t execUs, RobotState st, int pwmMax) {
    TelemetryFrame f;
    int norm[SENSOR_COUNT];
    calibration.normalize(raw, norm);
    for (int i = 0; i < SENSOR_COUNT; i++) f.s[i] = static_cast<uint8_t>(norm[i] / 10);
    f.pos  = out.error;
    f.corr = out.u;
    f.vL   = static_cast<int16_t>(std::lround(out.uL * pwmMax));
    f.vR   = static_cast<int16_t>(std::lround(out.uR * pwmMax));
    f.dtUs = static_cast<uint16_t>(std::min<uint32_t>(execUs, 65535));
    f.rpmL = static_cast<int16_t>(std::lround(velL.getRPM()));
    f.rpmR = static_cast<int16_t>(std::lround(velR.getRPM()));
    f.spL  = static_cast<int16_t>(std::lround(out.spL));
    f.spR  = static_cast<int16_t>(std::lround(out.spR));
    f.st   = static_cast<uint8_t>(st);
    f.lost = out.lost ? 1 : 0;
    return f;
}

// ═══ Task control — core 1, 500 Hz ═════════════════════════════════════════
static void taskControl(void*) {
    RobotConfig cfg;
    uint32_t cfgSeen = 0;
    g_cfgBox.fetch(cfg, cfgSeen);
    control.applyConfig(cfg);
    velL.setPprX4(cfg.hw.pprL);
    velR.setPprX4(cfg.hw.pprR);

    RobotStateMachine sm(g_calValidAtBoot, millis());
    annunciator.onState(sm.state(), sm.error(), millis());
    if (g_cfgErr) annunciator.beep(3, 60, 60, millis());

    TickType_t lastWake = xTaskGetTickCount();
    uint32_t prevUs = micros();
    RobotState prevState = sm.state();
    ErrorCode prevError = sm.error();
    int batteryTick = 0;
    int statusTick = 0;
    int overruns = 0;
    uint16_t ovrMax = 0;
    int raw[SENSOR_COUNT] = {};
    ControlOutputs out{};

    for (;;) {
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(LOOP_PERIOD_US / 1000));
        const uint32_t t0 = micros();
        const uint32_t dtUs = t0 - prevUs;
        prevUs = t0;
        const uint32_t nowMs = millis();

        if (g_cfgBox.fetch(cfg, cfgSeen)) {
            control.applyConfig(cfg);
            velL.setPprX4(cfg.hw.pprL);
            velR.setPprX4(cfg.hw.pprR);
        }

        if (++batteryTick >= BATTERY_EVERY) {
            batteryTick = 0;
            battery.update(analogReadMilliVolts(PIN_VBAT_ADC), 0.1f, nowMs, cfg.hw);
        }

        SmInputs in;
        in.calibrationValid = calibration.isValid();
        in.batteryOk = battery.hasReading() && battery.volts() >= cfg.hw.vcut;

        ControlMsg msg;
        while (xQueueReceive(g_ctlQueue, &msg, 0) == pdTRUE) applyRequest(msg, sm, in, nowMs);

        const ButtonEvent be = button.update(digitalRead(PIN_BTN_START) == LOW, nowMs);
        if (be == ButtonEvent::SHORT) sm.handle(SmEvent::BTN_SHORT, in, nowMs);
        if (be == ButtonEvent::LONG) sm.handle(SmEvent::BTN_LONG, in, nowMs);

        sensors.readAll(raw);
        velL.update(dtUs);  // sempre: drena o delta mesmo parado
        velR.update(dtUs);

        if (sm.state() == RobotState::CALIBRATING) {
            if (prevState != RobotState::CALIBRATING) calibration.reset();
            calibration.update(raw);
            if (sm.calibrationWindowOver(nowMs)) {
                CalReport r;
                int nBad = 0;
                r.ok = calibration.isValid(r.bad, &nBad);
                r.nBad = static_cast<uint8_t>(nBad);
                // motores parados: gravar na NVS aqui não atrapalha o controle
                if (r.ok) configStore.saveCalibration(calibration.exportData());
                sm.handle(r.ok ? SmEvent::CAL_DONE_OK : SmEvent::CAL_DONE_FAIL, in, nowMs);
                g_calBox.post(r);
            }
        }

        if (sm.tick(nowMs, cfg.tune.startMs)) {
            control.start(raw, velL.getRPM(), velR.getRPM());
        }

        if (sm.state() == RobotState::RUNNING) {
            ControlInputs ci;
            ci.raw = raw;
            ci.rpmL = velL.getRPM();
            ci.rpmR = velR.getRPM();
            ci.vbat = battery.volts();
            ci.dtS = static_cast<float>(dtUs) * 1e-6f;
            out = control.step(ci);
            motors.setSpeed(MotorId::A, out.duties.a);
            motors.setSpeed(MotorId::B, out.duties.b);
            if (out.lostTimeout) sm.handle(SmEvent::LINE_LOST_TIMEOUT, in, nowMs);
            if (battery.isLow()) sm.handle(SmEvent::BATTERY_LOW, in, nowMs);
        } else {
            out = ControlOutputs{};
        }

        const uint32_t execUs = micros() - t0;
        const uint16_t execClamped = static_cast<uint16_t>(std::min<uint32_t>(execUs, 65535));
        ovrMax = std::max(ovrMax, execClamped);
        overruns = (execUs > LOOP_PERIOD_US) ? overruns + 1 : 0;
        if (overruns >= OVERRUN_LIMIT) {
            sm.handle(SmEvent::LOOP_OVERRUN, in, nowMs);
            overruns = 0;
        }
        if (!sm.motorsEnabled()) motors.stop();

        if (sm.state() != prevState || sm.error() != prevError) {
            annunciator.onState(sm.state(), sm.error(), nowMs);
        }
        const Indication ind = annunciator.update(nowMs);
        digitalWrite(PIN_LED_STATUS, ind.led ? HIGH : LOW);
        ledcWrite(BUZZER_LEDC_CHANNEL, ind.buzzer ? 128 : 0);

        g_state.store(static_cast<uint8_t>(sm.state()));
        g_telBox.post(buildTelemetry(raw, out, execUs, sm.state(), PWM_MAX));
        if (++statusTick >= STATUS_EVERY) {
            statusTick = 0;
            StatusFrame s;
            s.bat = battery.percent(cfg.hw.chem);
            s.vb = battery.volts();
            s.err = static_cast<uint8_t>(sm.error());
            s.ovr = ovrMax;
            ovrMax = 0;
            g_staBox.post(s);
        }
        prevState = sm.state();
        prevError = sm.error();
    }
}

// ═══ Task comm — core 0, 100 Hz ════════════════════════════════════════════
static DeviceInfo currentInfo() {
    DeviceInfo d;
    d.name = DEVICE_NAME;
    d.cfgErr = g_cfgErr;
    // No ESP-IDF a folga de pilha já sai em bytes
    d.stkControl = g_controlTask ? uxTaskGetStackHighWaterMark(g_controlTask) : 0;
    d.stkComm = uxTaskGetStackHighWaterMark(g_commTask);
    return d;
}

class BleSink : public ResponseSink {
public:
    void send(const char* json) override { ble.notify(json); }
};

static void taskComm(void*) {
    ble.begin(DEVICE_NAME);
    BleSink sink;
    CommandHandler handler(g_liveCfg, configStore, FW_VERSION);
    handler.setInfoSource(currentInfo);
    char in[CommandQueue::MAX_LEN];
    char outBuf[192];
    uint32_t telSeen = 0;
    uint32_t staSeen = 0;
    uint32_t calSeen = 0;
    PeriodicTimer telTimer(TELEMETRY_MS);  // ~30 Hz sem deriva (A-WP1-06)

    for (;;) {
        const uint32_t nowMs = millis();

        // No onConnect o cliente ainda não se inscreveu: info + cfg saem na inscrição
        // (A-WP1-01) e de novo a cada get
        ble.consumeNewConnection();
        if (ble.consumeNewSubscription()) handler.sendInfoAndConfig(sink);

        while (ble.popCommand(in, sizeof in)) {
            Command cmd;
            if (!parseCommand(in, cmd)) {
                if (formatAckSimple(outBuf, sizeof outBuf, "?", false, "parse")) {
                    ble.notify(outBuf);
                }
                continue;
            }
            ControlMsg msg;
            handler.handle(cmd, isActiveState(g_state.load()), sink, msg);
            if (msg.req != ControlRequest::NONE) xQueueSend(g_ctlQueue, &msg, 0);
            if (handler.consumeConfigChanged()) g_cfgBox.post(handler.live());
        }

        CalReport cr;
        if (g_calBox.fetch(cr, calSeen)) {
            const bool ok = (cr.kind == CalReport::Kind::SENSOR)
                ? formatCalResult(outBuf, sizeof outBuf, cr)
                : formatEncCal(outBuf, sizeof outBuf, cr);
            if (ok) ble.notify(outBuf);
            if (cr.kind == CalReport::Kind::ENCODER && cr.ok) {
                // PPR já persistido pelo VelocityEstimator; espelha na config viva e publica
                handler.applyEncoderCalibration(cr.left, cr.ppr);
            }
            if (handler.consumeConfigChanged()) g_cfgBox.post(handler.live());
        }

        if (telTimer.due(nowMs)) {
            TelemetryFrame tf;
            if (g_telBox.fetch(tf, telSeen) && formatTelemetry(outBuf, sizeof outBuf, tf)) {
                ble.notify(outBuf);
            }
        }

        StatusFrame sf;
        if (g_staBox.fetch(sf, staSeen)) {
            sf.prof = handler.live().profile;
            sf.dirty = handler.dirty() ? 1 : 0;
            if (formatStatus(outBuf, sizeof outBuf, sf)) ble.notify(outBuf);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ═══ Setup ═════════════════════════════════════════════════════════════════
void setup() {
    pinMode(PIN_BTN_START, INPUT_PULLUP);
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);
    ledcSetup(BUZZER_LEDC_CHANNEL, BUZZER_FREQ_HZ, 8);
    ledcAttachPin(PIN_BUZZER, BUZZER_LEDC_CHANNEL);
    ledcWrite(BUZZER_LEDC_CHANNEL, 0);

    SPI.begin(PIN_SPI_CLK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SPI_CS);
    sensors.begin();
    motors.begin();
    encL.begin();
    encR.begin();
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_VBAT_ADC, ADC_11db);

    nvs.begin("lfr");
    velL.begin();
    velR.begin();

    RobotConfig cfg = robotcfg::factoryConfig();
    const ConfigStore::LoadReport rep = configStore.load(cfg);
    g_cfgErr = !rep.hardwareOk || !rep.profileOk;
    g_liveCfg = cfg;

    CalibrationData cal{};
    if (configStore.loadCalibration(cal)) calibration.importData(cal);
    g_calValidAtBoot = calibration.isValid();

    g_cfgBox.post(cfg);
    g_ctlQueue = xQueueCreate(8, sizeof(ControlMsg));

    // control antes de comm: o handle do control já existe quando a comm o lê (E-2)
    xTaskCreatePinnedToCore(taskControl, "control", 8192, nullptr,
                            configMAX_PRIORITIES - 2, &g_controlTask, 1);
    xTaskCreatePinnedToCore(taskComm, "comm", 8192, nullptr, 2, &g_commTask, 0);
}

void loop() {
    vTaskDelete(nullptr);  // o trabalho está nas tasks
}

#endif  // NATIVE_BUILD
