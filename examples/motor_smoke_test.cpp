// Teste manual de fumaça dos motores — flash no ESP32 e observe
// Sequência: A frente → A ré → B frente → B ré → ambos frente → stop
#ifndef NATIVE_BUILD
#include <Arduino.h>
#include "motors/MotorDriver.h"
#include "config.h"

static MotorDriver::Config cfgA = {
    PIN_MOTOR_A_PWM, PIN_MOTOR_A_IN1, PIN_MOTOR_A_IN2, 0, PWM_FREQ, PWM_RESOLUTION
};
static MotorDriver::Config cfgB = {
    PIN_MOTOR_B_PWM, PIN_MOTOR_B_IN1, PIN_MOTOR_B_IN2, 1, PWM_FREQ, PWM_RESOLUTION
};
static MotorDriver motors(cfgA, cfgB, PIN_MOTOR_STBY);

static uint32_t stepStart = 0;
static int step = 0;
static constexpr uint32_t STEP_MS = 1500;

void setup() {
    Serial.begin(115200);
    motors.begin();
    stepStart = millis();
    Serial.println("Motor smoke test started");
}

void loop() {
    if (millis() - stepStart < STEP_MS) return;
    stepStart = millis();

    switch (step % 6) {
        case 0: motors.setSpeed(MotorId::A,  700); motors.brake(MotorId::B);
                Serial.println("A FORWARD"); break;
        case 1: motors.setSpeed(MotorId::A, -700); motors.brake(MotorId::B);
                Serial.println("A REVERSE"); break;
        case 2: motors.brake(MotorId::A);  motors.setSpeed(MotorId::B,  700);
                Serial.println("B FORWARD"); break;
        case 3: motors.brake(MotorId::A);  motors.setSpeed(MotorId::B, -700);
                Serial.println("B REVERSE"); break;
        case 4: motors.setSpeed(MotorId::A, 700); motors.setSpeed(MotorId::B, 700);
                Serial.println("BOTH FORWARD"); break;
        case 5: motors.stop();
                Serial.println("STOP"); break;
    }
    step++;
}
#endif
