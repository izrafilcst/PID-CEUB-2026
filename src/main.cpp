// orquestração principal LFR — Line Follower Racer
// Core 0: controle (sensor→PID→motor, < 2ms)
// Core 1: comunicação (BLE + telemetria JSON @ 30 Hz)
#ifndef NATIVE_BUILD

#include <Arduino.h>
#include <SPI.h>
#include <atomic>

#include "config.h"
#include "sensors/Calibration.h"
#include "sensors/SensorArray.h"
#include "control/PIDController.h"
#include "control/SpeedProfile.h"
#include "motors/MotorDriver.h"
#include "motors/DifferentialDrive.h"
#include "strategy/LineFollower.h"
#include "strategy/LapTimer.h"
#include "comm/BLETuner.h"
#include "comm/Logger.h"
#include "sensors/Encoder.h"
#include "sensors/VelocityEstimator.h"
#include "storage/NvsStore.h"
#include "control/CascadeController.h"

// ═══ Estado da máquina ═════════════════════════════════════════════════════
enum class RobotState : uint8_t { IDLE, CALIBRATING, READY, RUNNING, ERROR };

// ═══ Módulos — Dependency Injection no construtor ══════════════════════════
static Calibration calibration(SENSOR_COUNT);

static SensorArray sensors(
    PIN_SPI_CS,
    SPI_FREQ,
    SENSOR_COUNT,
    calibration
);

static PIDController pid(
    KP_DEFAULT, KI_DEFAULT, KD_DEFAULT,
    -static_cast<float>(PWM_MAX),
    +static_cast<float>(PWM_MAX)
);

static SpeedProfile speedProfile(
    MIN_SPEED, BASE_SPEED_DEFAULT, MAX_SPEED, ERROR_THRESHOLD
);

static DifferentialDrive drive(PWM_MAX);
static LineFollower lineFollower(calibration, pid, speedProfile, drive);
static LapTimer lapTimer;

static BLETuner ble;
static Logger   logger;

static MotorDriver::Config cfgA = {
    PIN_MOTOR_A_PWM, PIN_MOTOR_A_IN1, PIN_MOTOR_A_IN2, 0, PWM_FREQ, PWM_RESOLUTION
};
static MotorDriver::Config cfgB = {
    PIN_MOTOR_B_PWM, PIN_MOTOR_B_IN1, PIN_MOTOR_B_IN2, 1, PWM_FREQ, PWM_RESOLUTION
};
static MotorDriver motors(cfgA, cfgB, PIN_MOTOR_STBY);

// ═══ Cascade PID — Fase C ════════════════════════════════════════════════════
static Encoder encL(PIN_ENC_LEFT_A,  PIN_ENC_LEFT_B);
static Encoder encR(PIN_ENC_RIGHT_A, PIN_ENC_RIGHT_B);

static NvsStore nvs;
static VelocityEstimator velL(encL, nvs, "left");
static VelocityEstimator velR(encR, nvs, "right");

static PIDController pidVelL(
    KP_VEL_DEFAULT, KI_VEL_DEFAULT, KD_VEL_DEFAULT,
    -static_cast<float>(PWM_MAX), +static_cast<float>(PWM_MAX), 0.002f);
static PIDController pidVelR(
    KP_VEL_DEFAULT, KI_VEL_DEFAULT, KD_VEL_DEFAULT,
    -static_cast<float>(PWM_MAX), +static_cast<float>(PWM_MAX), 0.002f);

static CascadeController cascade(pidVelL, pidVelR, MAX_RPM_DEFAULT, PWM_MAX);

// ═══ Estado compartilhado Core 0 → Core 1 ══════════════════════════════════
struct TelemetrySnapshot {
    float    pos       = 0.0f;
    float    corr      = 0.0f;
    int      vL        = 0;
    int      vR        = 0;
    uint32_t dtUs      = 0;
    float    rpmL      = 0.0f;
    float    rpmR      = 0.0f;
    float    setpointL = 0.0f;
    float    setpointR = 0.0f;
    float    sensorsPct[SENSOR_COUNT] = {};  // [0–100]
    bool     lineLost  = false;
    bool     crossing  = false;
};

static TelemetrySnapshot   g_snap;
static portMUX_TYPE        g_snapMux        = portMUX_INITIALIZER_UNLOCKED;
static volatile float      g_baseRpm        = BASE_RPM_DEFAULT;  // mutável via BLE
static std::atomic<RobotState> robotState{RobotState::IDLE};
static portMUX_TYPE        lineFollowerMux  = portMUX_INITIALIZER_UNLOCKED;
static uint32_t            calStart         = 0;
static constexpr uint32_t  CAL_DURATION_MS  = 5000;

// Estado do cascade — escopo de arquivo p/ permitir reset em transições READY→RUNNING.
// M-1 (Reviewer) + MS-1 (Security): stale prevLoopStart causa dt fora de escala
// no 1º tick após re-entry; encoders acumulam pulsos durante IDLE/READY.
static uint32_t g_prevLoopStart = 0;

static void resetCascadeState() {
    g_prevLoopStart = 0;
    velL.update(0);   // Fase B: update(0) drena delta sem atualizar IIR
    velR.update(0);
}

// ═══ Bateria ════════════════════════════════════════════════════════════════
// Divisor: 100 kΩ / 47 kΩ → Vout = Vin × 47/147. LiPo 2S: 6.4–8.4 V.
static float readBatteryPct() {
    const int   raw  = analogRead(PIN_VBAT_ADC);
    const float vout = (raw / 4095.0f) * 3.3f;
    const float vin  = vout * (100.0f + 47.0f) / 47.0f;
    const float pct  = (vin - 6.4f) / (8.4f - 6.4f) * 100.0f;
    return std::max(0.0f, std::min(100.0f, pct));
}

// ═══ Helpers de hardware ════════════════════════════════════════════════════
// Buzzer não-bloqueante: beepStart() liga e agenda o desligamento; serviceBuzzer()
// (chamado nos dois loops) desliga quando vence o prazo. Evita busy-wait no loop
// de controle (Core 0) e na task de BLE (Core 1).
static volatile uint32_t g_buzzerOffMs = 0;

static void beepStart(uint32_t ms) {
    digitalWrite(PIN_BUZZER, HIGH);
    g_buzzerOffMs = millis() + ms;
    if (g_buzzerOffMs == 0) g_buzzerOffMs = 1;  // 0 é sentinela de "desligado"
}

static void serviceBuzzer() {
    uint32_t off = g_buzzerOffMs;
    if (off != 0 && static_cast<int32_t>(millis() - off) >= 0) {
        digitalWrite(PIN_BUZZER, LOW);
        g_buzzerOffMs = 0;
    }
}

static inline bool btnPressed() {
    return digitalRead(PIN_BTN_START) == LOW;
}

// ═══ Task Core 1 — BLE + Telemetria JSON ════════════════════════════════════
static void taskComm(void* /*pvParameters*/) {
    ble.begin("LFR-RACER-01");

    int   telTick = 0;
    int   batTick = 0;
    float batPct  = 100.0f;
    float bestLap = 0.0f;
    bool  hasLap  = false;
    float laps[5] = {};
    int   nLaps   = 0;

    uint32_t lastLapMs = 0;  // detecta quando LapTimer registra uma nova volta

    for (;;) {
        serviceBuzzer();
        ble.update();

        // Envia frame de identificação ao cliente recém-conectado
        if (ble.consumeNewConnection()) {
            ble.notifyInfo("LFR-RACER-01", "v3.1.0", "RUN");
        }

        // Aplica parâmetros PID recebidos via BLE
        if (ble.hasNewPID()) {
            PIDParams p = ble.getPID();
            taskENTER_CRITICAL(&lineFollowerMux);
            lineFollower.setPID(p.kp, p.ki, p.kd);
            taskEXIT_CRITICAL(&lineFollowerMux);
        }

        // Aplica parâmetros de velocidade recebidos via BLE
        if (ble.hasNewSpeed()) {
            SpeedParams s = ble.getSpeed();
            taskENTER_CRITICAL(&lineFollowerMux);
            lineFollower.setSpeed(s.minSpeed, s.baseSpeed, s.maxSpeed, s.threshold);
            taskEXIT_CRITICAL(&lineFollowerMux);
        }

        // Aplica ganhos do PID interno de velocidade
        if (ble.hasNewVelocityPID()) {
            VelocityPIDParams p = ble.getVelocityPID();
            // Hardening (Wave 3): ganho negativo inverte o sinal da malha
            // (realimentação positiva → descontrole). NaN/Inf → 0. Piso em 0.
            float kp = (std::isfinite(p.kp) && p.kp >= 0.0f) ? p.kp : 0.0f;
            float ki = (std::isfinite(p.ki) && p.ki >= 0.0f) ? p.ki : 0.0f;
            float kd = (std::isfinite(p.kd) && p.kd >= 0.0f) ? p.kd : 0.0f;
            taskENTER_CRITICAL(&lineFollowerMux);
            cascade.setInnerGains(kp, ki, kd);
            taskEXIT_CRITICAL(&lineFollowerMux);
        }

        // Aplica novo MAX_RPM (clamp do cascade)
        if (ble.hasNewRpm()) {
            RpmParams r = ble.getRpm();
            // Hardening (Wave 3): clampa comandos BLE à faixa física antes de
            // tocar a malha. max inválido/≤0 → default; base confinada a [0,max].
            float maxRpm  = (std::isfinite(r.maxRpm) && r.maxRpm > 0.0f)
                          ? (r.maxRpm < RPM_HARD_CEILING ? r.maxRpm : RPM_HARD_CEILING)
                          : MAX_RPM_DEFAULT;
            float baseRpm = (std::isfinite(r.baseRpm) && r.baseRpm >= 0.0f)
                          ? (r.baseRpm < maxRpm ? r.baseRpm : maxRpm)
                          : 0.0f;
            taskENTER_CRITICAL(&lineFollowerMux);
            cascade.setMaxRpm(maxRpm);
            g_baseRpm = baseRpm;
            taskEXIT_CRITICAL(&lineFollowerMux);
        }

        // Comando de calibração de encoder
        if (ble.hasNewCalibration() && robotState.load() != RobotState::RUNNING) {
            CalibrationCmd c = ble.getCalibration();
            VelocityEstimator& target = c.leftSide ? velL : velR;
            if (c.op == CalibrationCmd::Op::START) {
                target.startCalibration();
                beepStart(50);
            } else if (c.op == CalibrationCmd::Op::FINISH) {
                bool ok = target.finishCalibration(c.rotations);
                beepStart(ok ? 200 : 500);
                char buf[40];
                snprintf(buf, sizeof(buf), "%.2f", target.getEffectivePPR());
                // NOTE: notifyInfo(name, fw, mode) — fw/mode fields are repurposed
                // as status/value for this confirmation message. Tracked as known
                // deviation; proper fix (new overload) is out of Fase D scope.
                ble.notifyInfo(c.leftSide ? "CAL_L" : "CAL_R",
                               ok ? "OK" : "FAIL", buf);
            }
        }

        // Comandos de controle remoto via BLE
        if (ble.consumeStart() && robotState.load() == RobotState::READY) {
            pid.reset();
            lineFollower.reset();
            resetCascadeState();
            lapTimer.start(millis());
            logger.setEnabled(true);
            robotState.store(RobotState::RUNNING, std::memory_order_release);
            beepStart(50);
        }
        if (ble.consumeStop() && robotState.load() == RobotState::RUNNING) {
            motors.stop();
            logger.setEnabled(false);
            robotState.store(RobotState::READY, std::memory_order_release);
            beepStart(300);
        }
        if (ble.consumeReset()) {
            motors.stop();
            logger.setEnabled(false);
            lapTimer.reset();
            hasLap = false;
            nLaps  = 0;
            robotState.store(RobotState::IDLE, std::memory_order_release);
            beepStart(100);
        }

        // Bateria: leitura a cada ~1 s (100 × 10 ms)
        if (++batTick >= 100) {
            batTick = 0;
            batPct  = readBatteryPct();
        }

        // Telemetria JSON @ 30 Hz (cada 3 ticks de 10 ms)
        if (++telTick >= 3) {
            telTick = 0;

            TelemetrySnapshot snap;
            taskENTER_CRITICAL(&g_snapMux);
            snap = g_snap;
            taskEXIT_CRITICAL(&g_snapMux);

            // Sincroniza voltas com LapTimer
            uint32_t currLapMs = lapTimer.getLastLapMs();
            if (currLapMs > 0 && currLapMs != lastLapMs) {
                lastLapMs = currLapMs;
                // Insere no início do histórico (shift right, máx 5 entradas)
                int shift = (nLaps < 5) ? nLaps : 4;
                for (int i = shift; i > 0; i--) laps[i] = laps[i - 1];
                laps[0] = currLapMs / 1000.0f;
                if (nLaps < 5) nLaps++;
            }
            uint32_t bestMs = lapTimer.getBestLapMs();
            if (bestMs > 0) {
                hasLap  = true;
                bestLap = bestMs / 1000.0f;
            }

            ble.notifyTelemetry(
                snap.pos, snap.corr, snap.vL, snap.vR, snap.dtUs,
                snap.rpmL, snap.rpmR, snap.setpointL, snap.setpointR,
                snap.sensorsPct, SENSOR_COUNT,
                batPct, bestLap, hasLap, laps, nLaps
            );

            // Buffer CSV de debug (opcional — pode ser redirecionado p/ SPIFFS)
            logger.drain([](const char* /*csv*/) {});
        }

        vTaskDelay(pdMS_TO_TICKS(10));  // 100 Hz poll
    }
}

// ═══ Setup ══════════════════════════════════════════════════════════════════
void setup() {
    SPI.begin(PIN_SPI_CLK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SPI_CS);
    sensors.begin();
    motors.begin();
    encL.begin();
    encR.begin();
    nvs.begin("lfr");
    velL.begin();
    velR.begin();
    analogReadResolution(12);

    pinMode(PIN_BTN_START,  INPUT_PULLUP);
    pinMode(PIN_BUZZER,     OUTPUT);
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_BUZZER,     LOW);
    digitalWrite(PIN_LED_STATUS, LOW);

    xTaskCreatePinnedToCore(taskComm, "Comm", 8192, nullptr, 1, nullptr, 1);

    robotState.store(RobotState::IDLE, std::memory_order_release);
    digitalWrite(PIN_LED_STATUS, HIGH);
    beepStart(100);
}

// ═══ Loop — Core 0 (controle tempo-real) ════════════════════════════════════
void loop() {
    serviceBuzzer();
    switch (robotState.load(std::memory_order_acquire)) {

        case RobotState::IDLE:
            if (btnPressed()) {
                robotState.store(RobotState::CALIBRATING, std::memory_order_release);
                calStart = millis();
                calibration.reset();
                sensors.begin();
                beepStart(100);
                digitalWrite(PIN_LED_STATUS, LOW);
            }
            break;

        case RobotState::CALIBRATING: {
            int raw[SENSOR_COUNT];
            sensors.readAll(raw);
            calibration.update(raw);
            digitalWrite(PIN_LED_STATUS, (millis() / 200) % 2);
            if (millis() - calStart >= CAL_DURATION_MS) {
                robotState.store(RobotState::READY, std::memory_order_release);
                digitalWrite(PIN_LED_STATUS, LOW);
                beepStart(200);
            }
            break;
        }

        case RobotState::READY:
            digitalWrite(PIN_LED_STATUS, HIGH);
            if (btnPressed()) {
                pid.reset();
                lineFollower.reset();
                resetCascadeState();
                lapTimer.start(millis());
                logger.setEnabled(true);
                robotState.store(RobotState::RUNNING, std::memory_order_release);
                beepStart(50);
            }
            break;

        case RobotState::RUNNING: {
            uint32_t loopStart = micros();
            // MED-1: dt real; M-1: g_prevLoopStart é resetado em resetCascadeState()
            // a cada re-entry no estado, evitando dt stale após STOP/START.
            uint32_t dtUs = (g_prevLoopStart == 0) ? 0 : (loopStart - g_prevLoopStart);
            g_prevLoopStart = loopStart;
            // dtUs==0 no 1º tick pós-reset → usa período nominal p/ o dt dos PIDs.
            float dtSec = (dtUs == 0) ? (LOOP_PERIOD_US * 1e-6f) : (dtUs * 1e-6f);

            // 1. Atualiza estimadores de velocidade com dt REAL do loop (µs)
            velL.update(dtUs);
            velR.update(dtUs);

            // 2. Lê sensores de linha
            int raw[SENSOR_COUNT];
            sensors.readAll(raw);

            // 3. Valida RPM antes de entrar na seção crítica (leitura só-Core0).
            float rpmLActual = velL.getRPM();
            float rpmRActual = velR.getRPM();
            if (!std::isfinite(rpmLActual)) rpmLActual = 0.0f;
            if (!std::isfinite(rpmRActual)) rpmRActual = 0.0f;

            int   discardL = 0;
            int   discardR = 0;
            float normErr  = 0.0f;
            int   leftPwm  = 0;
            int   rightPwm = 0;
            bool  lineLost = false;
            bool  crossing = false;

            // Seção crítica cobre PID externo + cascade + leitura de g_baseRpm.
            // Fecha a corrida com setInnerGains()/setMaxRpm()/g_baseRpm (Core 1),
            // que também rodam sob lineFollowerMux.
            taskENTER_CRITICAL(&lineFollowerMux);
            lineFollower.update(raw, discardL, discardR, &normErr, dtSec);
            float correctionPwm = lineFollower.getLastCorrection();
            lineLost = lineFollower.isLineLost();
            crossing = lineFollower.isCrossing();
            float correctionRpm =
                correctionPwm * (MAX_RPM_DEFAULT / static_cast<float>(PWM_MAX));
            cascade.compute(correctionRpm, g_baseRpm,
                            rpmLActual, rpmRActual,
                            leftPwm, rightPwm, dtSec);
            taskEXIT_CRITICAL(&lineFollowerMux);

            // 7. Aplica PWM nos motores (fora da seção crítica)
            motors.setSpeed(MotorId::A, leftPwm);
            motors.setSpeed(MotorId::B, rightPwm);

            uint32_t loopDurationUs = micros() - loopStart;

            // Atualiza snapshot de telemetria para o Core 1
            {
                int norm[SENSOR_COUNT];
                calibration.normalize(raw, norm);
                taskENTER_CRITICAL(&g_snapMux);
                g_snap.pos       = normErr * 3500.0f;
                // M-2 (Reviewer): semântica de `corr` agora é diff dos PWMs internos do
                // cascade (não mais saída do PID externo). Renomear o campo é Fase D scope;
                // por ora documentamos. Para correção pura da malha externa, usar telemetry
                // de getLastCorrection() (a expor) em Fase D.
                g_snap.corr      = static_cast<float>(leftPwm - rightPwm);
                g_snap.vL        = leftPwm;
                g_snap.vR        = rightPwm;
                g_snap.dtUs      = loopDurationUs;
                // C-1 (Reviewer): reusa rpmLActual/rpmRActual já validados via isfinite
                // em vez de chamar getRPM() de novo (TOCTOU defensivo).
                g_snap.rpmL      = rpmLActual;
                g_snap.rpmR      = rpmRActual;
                g_snap.setpointL = g_baseRpm + correctionRpm;
                g_snap.setpointR = g_baseRpm - correctionRpm;
                for (int i = 0; i < SENSOR_COUNT; i++)
                    g_snap.sensorsPct[i] = norm[i] / 10.0f;  // [0–1000] → [0–100]
                g_snap.lineLost = lineLost;
                g_snap.crossing = crossing;
                taskEXIT_CRITICAL(&g_snapMux);
            }

#ifdef DEBUG_LOG
            if (logger.isEnabled()) {
                LogEntry entry;
                entry.ts         = millis();
                entry.position   = normErr * 3500.0f;
                entry.error      = normErr;
                entry.termP      = normErr * pid.getKp();
                entry.termI      = 0.0f;
                entry.termD      = 0.0f;
                entry.correction = static_cast<float>(leftPwm - rightPwm);
                entry.vLeft      = leftPwm;
                entry.vRight     = rightPwm;
                entry.dtUs       = loopDurationUs;
                logger.push(entry);
            }
#else
            (void)loopDurationUs;
#endif

            if (btnPressed()) {
                motors.stop();
                logger.setEnabled(false);
                robotState.store(RobotState::READY, std::memory_order_release);
                beepStart(300);
            }

            if (loopDurationUs > 4000) {
                motors.stop();
                robotState.store(RobotState::ERROR, std::memory_order_release);
            }
            break;
        }

        case RobotState::ERROR:
            motors.stop();
            logger.setEnabled(false);
            digitalWrite(PIN_LED_STATUS, (millis() / 200) % 2);
            if (btnPressed()) {
                robotState.store(RobotState::IDLE, std::memory_order_release);
                beepStart(100);
            }
            break;
    }
}

#endif  // NATIVE_BUILD
