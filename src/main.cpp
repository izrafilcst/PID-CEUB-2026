// orquestração principal LFR — Line Follower Racer
// Core 0: controle (sensor→PID→motor, < 2ms)
// Core 1: comunicação (BLE + telemetria)
#ifndef NATIVE_BUILD

#include <Arduino.h>
#include <SPI.h>

#include "config.h"
#include "sensors/Calibration.h"
#include "sensors/SensorArray.h"
#include "control/PIDController.h"
#include "control/SpeedProfile.h"
#include "motors/MotorDriver.h"
#include "motors/DifferentialDrive.h"
#include "strategy/LineFollower.h"
#include "comm/BLETuner.h"
#include "comm/Logger.h"

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

static DifferentialDrive drive(BASE_SPEED_DEFAULT, MIN_SPEED, PWM_MAX);

static LineFollower lineFollower(calibration, pid, speedProfile, drive);

static BLETuner ble;
static Logger   logger;

static MotorDriver::Config cfgA = {
    PIN_MOTOR_A_PWM, PIN_MOTOR_A_IN1, PIN_MOTOR_A_IN2,
    0,                          // ledc channel
    PWM_FREQ, PWM_RESOLUTION
};
static MotorDriver::Config cfgB = {
    PIN_MOTOR_B_PWM, PIN_MOTOR_B_IN1, PIN_MOTOR_B_IN2,
    1,                          // ledc channel
    PWM_FREQ, PWM_RESOLUTION
};
static MotorDriver motors(cfgA, cfgB, PIN_MOTOR_STBY);

// ═══ Estado global (volátil apenas o que cruza cores) ══════════════════════
static volatile RobotState robotState = RobotState::IDLE;
static uint32_t calStart              = 0;
static constexpr uint32_t CAL_DURATION_MS = 5000;

// ═══ Helpers de hardware ════════════════════════════════════════════════════

/**
 * @brief Gera bipe no buzzer bloqueante (apenas em IDLE/READY, fora do loop RT).
 *        Nunca chamar durante RUNNING — quebraria o timing do Core 0.
 */
static void beep(uint32_t ms) {
    digitalWrite(PIN_BUZZER, HIGH);
    uint32_t t = millis();
    while (millis() - t < ms) { /* aguarda sem delay() */ }
    digitalWrite(PIN_BUZZER, LOW);
}

static inline bool btnPressed() {
    return digitalRead(PIN_BTN_START) == LOW;
}

// ═══ Task Core 1 — BLE + Telemetria ════════════════════════════════════════
static void taskComm(void* /*pvParameters*/) {
    ble.begin("LFR-Tuner");

    for (;;) {
        ble.update();   // processa callbacks internos do NimBLE

        // Aplica novos parâmetros PID recebidos via BLE
        if (ble.hasNewPID()) {
            PIDParams p = ble.getPID();
            lineFollower.setPID(p.kp, p.ki, p.kd);
        }

        // Aplica novos parâmetros de velocidade recebidos via BLE
        if (ble.hasNewSpeed()) {
            SpeedParams s = ble.getSpeed();
            lineFollower.setSpeed(s.minSpeed, s.baseSpeed, s.maxSpeed, s.threshold);
        }

        // Drena buffer de telemetria e encaminha via BLE
        logger.drain([](const char* csv) {
            ble.notifyStatus(csv);
        });

        vTaskDelay(pdMS_TO_TICKS(10));  // 100 Hz — adequado para BLE
    }
}

// ═══ Setup (Core 1, antes de criar tasks) ══════════════════════════════════
void setup() {
    // Periféricos
    SPI.begin(PIN_SPI_CLK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SPI_CS);
    sensors.begin();
    motors.begin();

    pinMode(PIN_BTN_START,  INPUT_PULLUP);
    pinMode(PIN_BUZZER,     OUTPUT);
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_BUZZER,     LOW);
    digitalWrite(PIN_LED_STATUS, LOW);

    // Core 1: BLE + Logger (prioridade 1, stack 8 kB)
    xTaskCreatePinnedToCore(
        taskComm, "Comm",
        8192, nullptr, 1, nullptr,
        1   // core 1
    );

    robotState = RobotState::IDLE;
    digitalWrite(PIN_LED_STATUS, HIGH);
    beep(100);
}

// ═══ Loop — Core 0 (controle tempo-real) ════════════════════════════════════
void loop() {
    switch (robotState) {

        // ── IDLE: aguarda botão para iniciar calibração ──────────────────────
        case RobotState::IDLE:
            if (btnPressed()) {
                robotState = RobotState::CALIBRATING;
                calStart   = millis();
                calibration.reset();
                sensors.begin();    // reinit SPI/chip select
                beep(100);
                digitalWrite(PIN_LED_STATUS, LOW);
            }
            break;

        // ── CALIBRATING: varre a pista por CAL_DURATION_MS ms ───────────────
        case RobotState::CALIBRATING: {
            // Lê sensores e alimenta Calibration com min/max contínuos
            int raw[SENSOR_COUNT];
            sensors.readAll(raw);
            calibration.update(raw);

            // Pisca LED durante calibração
            digitalWrite(PIN_LED_STATUS, (millis() / 200) % 2);

            if (millis() - calStart >= CAL_DURATION_MS) {
                robotState = RobotState::READY;
                digitalWrite(PIN_LED_STATUS, LOW);
                beep(200);
            }
            break;
        }

        // ── READY: calibrado, aguarda botão para correr ─────────────────────
        case RobotState::READY:
            digitalWrite(PIN_LED_STATUS, HIGH);
            if (btnPressed()) {
                pid.reset();
                lineFollower.reset();
                logger.setEnabled(true);
                robotState = RobotState::RUNNING;
                beep(50);
            }
            break;

        // ── RUNNING: loop de controle tempo-real < 2 ms ─────────────────────
        case RobotState::RUNNING: {
            uint32_t loopStart = micros();

            // Leitura dos sensores via SensorArray (< 100 µs SPI)
            int raw[SENSOR_COUNT];
            sensors.readAll(raw);

            // Ciclo de controle: calibração → PID → perfil velocidade → PWM
            int   leftPwm  = 0;
            int   rightPwm = 0;
            float normErr  = 0.0f;

            lineFollower.update(raw, leftPwm, rightPwm, &normErr);

            // Aplica PWM nos motores
            motors.setSpeed(MotorId::A, leftPwm);
            motors.setSpeed(MotorId::B, rightPwm);

            uint32_t dtUs = micros() - loopStart;

            // Telemetria — somente com DEBUG_LOG para não pesar no Core 0
#ifdef DEBUG_LOG
            if (logger.isEnabled()) {
                LogEntry entry;
                entry.ts         = millis();
                entry.position   = normErr * 3500.0f;  // POSITION_MAX from LineFollower
                entry.error      = normErr;
                entry.termP      = normErr * pid.getKp();   // aproximação para log
                entry.termI      = 0.0f;                    // PID não expõe integral
                entry.termD      = 0.0f;
                entry.correction = static_cast<float>(leftPwm - rightPwm);
                entry.vLeft      = leftPwm;
                entry.vRight     = rightPwm;
                entry.dtUs       = dtUs;
                logger.push(entry);
            }
#else
            (void)dtUs;
#endif

            // Para se botão pressionado
            if (btnPressed()) {
                motors.stop();
                logger.setEnabled(false);
                robotState = RobotState::READY;
                beep(300);
            }

            // Watchdog: se loop > 4 ms, algo travou — vai para ERROR
            if (dtUs > 4000) {
                motors.stop();
                robotState = RobotState::ERROR;
            }
            break;
        }

        // ── ERROR: motores parados, pisca LED, aguarda botão ────────────────
        case RobotState::ERROR:
            motors.stop();
            logger.setEnabled(false);
            digitalWrite(PIN_LED_STATUS, (millis() / 200) % 2);
            if (btnPressed()) {
                robotState = RobotState::IDLE;
                beep(100);
            }
            break;
    }
}

#endif // NATIVE_BUILD
