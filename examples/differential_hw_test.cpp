// Teste de movimentos diferenciais — valida MotorDriver + mixSteering + OutputStage no ESP32
// Sequência: reto → curva dir → curva esq → giro horário → giro anti-horário → parado
// Cada movimento deve ser visualmente distinto. Se algum lado girar ao contrário, ajustar
// invL/invR (ou swap) pelo BLE — não mexer na fiação.
#ifndef NATIVE_BUILD
#include <Arduino.h>
#include "config.h"
#include "config/RobotConfig.h"
#include "control/Mixer.h"
#include "motors/MotorDriver.h"
#include "motors/OutputStage.h"

static MotorDriver::Config cfgA = {
    PIN_MOTOR_A_PWM, PIN_MOTOR_A_IN1, PIN_MOTOR_A_IN2, 0, PWM_FREQ, PWM_RESOLUTION
};
static MotorDriver::Config cfgB = {
    PIN_MOTOR_B_PWM, PIN_MOTOR_B_IN1, PIN_MOTOR_B_IN2, 1, PWM_FREQ, PWM_RESOLUTION
};
static MotorDriver motors(cfgA, cfgB, PIN_MOTOR_STBY);
static const OutputStage output(PWM_MAX);
static RobotConfig cfg = robotcfg::factoryConfig();

struct Movement {
    const char* name;
    float base;
    float u;
    uint32_t durationMs;
};

static const Movement SEQUENCE[] = {
    {"RETO",          0.40f,  0.00f, 1500},
    {"CURVA DIR",     0.30f,  0.20f, 1200},
    {"CURVA ESQ",     0.30f, -0.20f, 1200},
    {"GIRO HORARIO",  0.00f,  0.40f, 1000},
    {"GIRO ANTI-HOR", 0.00f, -0.40f, 1000},
    {"PARADO",        0.00f,  0.00f, 2000},
};
static constexpr int SEQ_LEN = sizeof(SEQUENCE) / sizeof(SEQUENCE[0]);

static int step = 0;
static uint32_t stepStart = 0;
static bool running = false;

static void applyStep(int idx) {
    const Movement& mv = SEQUENCE[idx];
    float l = 0.0f;
    float r = 0.0f;
    mixSteering(mv.base, mv.u, l, r);
    cfg.tune.vcomp = 0;  // bancada: sem leitura de bateria
    const MotorDuties d = output.compute(l, r, 0.0f, cfg.hw, cfg.tune);
    motors.setSpeed(MotorId::A, d.a);
    motors.setSpeed(MotorId::B, d.b);
    Serial.printf("  -> %-14s A=%5d  B=%5d\n", mv.name, d.a, d.b);
}

void setup() {
    Serial.begin(115200);
    motors.begin();
    pinMode(PIN_BTN_START, INPUT_PULLUP);
    pinMode(PIN_LED_STATUS, OUTPUT);
    Serial.println("Pressione o botao BOOT para iniciar a sequencia (rodas fora do chao)");
}

void loop() {
    if (!running && digitalRead(PIN_BTN_START) == LOW) {
        running = true;
        step = 0;
        stepStart = millis();
        digitalWrite(PIN_LED_STATUS, HIGH);
        applyStep(step);
        return;
    }
    if (!running) return;
    if (step >= SEQ_LEN) {
        motors.stop();
        running = false;
        digitalWrite(PIN_LED_STATUS, LOW);
        Serial.println("Sequencia completa. Pressione BOOT para repetir.");
        return;
    }
    if (millis() - stepStart >= SEQUENCE[step].durationMs) {
        stepStart = millis();
        step++;
        if (step < SEQ_LEN) applyStep(step);
    }
}
#endif
