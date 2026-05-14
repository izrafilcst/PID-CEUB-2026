// Teste de movimentos diferenciais — valida MotorDriver + DifferentialDrive no ESP32
// Sequência: reto → curva dir → curva esq → spin CW → spin CCW → stop
// Observar: cada movimento deve ser visualmente distinto
#ifndef NATIVE_BUILD
#include <Arduino.h>
#include "motors/MotorDriver.h"
#include "motors/DifferentialDrive.h"
#include "config.h"

static MotorDriver::Config cfgA = {
    PIN_MOTOR_A_PWM, PIN_MOTOR_A_IN1, PIN_MOTOR_A_IN2, 0, PWM_FREQ, PWM_RESOLUTION
};
static MotorDriver::Config cfgB = {
    PIN_MOTOR_B_PWM, PIN_MOTOR_B_IN1, PIN_MOTOR_B_IN2, 1, PWM_FREQ, PWM_RESOLUTION
};
static MotorDriver motors(cfgA, cfgB, PIN_MOTOR_STBY);
static DifferentialDrive drive(BASE_SPEED_DEFAULT, MIN_SPEED, PWM_MAX);

struct Movement {
    const char* name;
    float correction;
    int baseSpeed;
    uint32_t durationMs;
};

static const Movement SEQUENCE[] = {
    { "RETO",       0.0f,   700, 1500 },
    { "CURVA DIR", 300.0f,  500, 1200 },
    { "CURVA ESQ", -300.0f, 500, 1200 },
    { "SPIN CW",   400.0f,    0, 1000 },
    { "SPIN CCW", -400.0f,    0, 1000 },
    { "STOP",        0.0f,    0, 2000 },
};
static constexpr int SEQ_LEN = sizeof(SEQUENCE) / sizeof(SEQUENCE[0]);

static int step = 0;
static uint32_t stepStart = 0;
static bool running = false;

static void applyStep(int idx) {
    const Movement& mv = SEQUENCE[idx];
    int lp, rp;
    drive.compute(mv.correction, mv.baseSpeed, lp, rp);
    motors.setSpeed(MotorId::A, lp);
    motors.setSpeed(MotorId::B, rp);
    Serial.print("  → ");
    Serial.print(mv.name);
    Serial.print("  L="); Serial.print(lp);
    Serial.print("  R="); Serial.println(rp);
}

void setup() {
    Serial.begin(115200);
    motors.begin();
    pinMode(PIN_BTN_START, INPUT_PULLUP);
    pinMode(PIN_LED_STATUS, OUTPUT);
    Serial.println("Pressione BTN_START para iniciar sequencia de movimentos");
}

void loop() {
    // Aguarda botão
    if (!running && digitalRead(PIN_BTN_START) == LOW) {
        running = true;
        step = 0;
        stepStart = millis();
        Serial.println("Iniciando sequencia...");
        digitalWrite(PIN_LED_STATUS, HIGH);
        applyStep(step);
        return;
    }

    if (!running) return;

    // Fim da sequência
    if (step >= SEQ_LEN) {
        motors.stop();
        running = false;
        digitalWrite(PIN_LED_STATUS, LOW);
        Serial.println("Sequencia completa. Pressione BTN_START para repetir.");
        return;
    }

    // Avança step quando duração expirar
    if (millis() - stepStart >= SEQUENCE[step].durationMs) {
        stepStart = millis();
        step++;
        if (step < SEQ_LEN) {
            applyStep(step);
        }
    }
}
#endif
