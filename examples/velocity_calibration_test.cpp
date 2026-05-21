// examples/velocity_calibration_test.cpp
// Procedimento:
//   1. Flash neste arquivo (descomentar no platformio se aplicável)
//   2. Abrir Serial Monitor a 115200
//   3. Quando aparecer "START CAL — gire a roda esquerda 10 voltas e digite f":
//      gire a roda 10 voltas manualmente, depois envie 'f' no Serial.
//   4. Repita para a roda direita.
//   5. Tente girar lentamente e veja RPM no Serial.
#ifndef NATIVE_BUILD
#include <Arduino.h>
#include "config.h"
#include "sensors/Encoder.h"
#include "sensors/VelocityEstimator.h"
#include "storage/NvsStore.h"

static Encoder encL(PIN_ENC_LEFT_A,  PIN_ENC_LEFT_B);
static Encoder encR(PIN_ENC_RIGHT_A, PIN_ENC_RIGHT_B);
static NvsStore nvs;
static VelocityEstimator velL(encL, nvs, "left");
static VelocityEstimator velR(encR, nvs, "right");

static const int ROTATIONS = 10;

static void calibrateAxis(const char* label, VelocityEstimator& ve) {
    Serial.printf("START CAL %s — gire %d voltas e envie 'f'\n", label, ROTATIONS);
    ve.startCalibration();
    while (true) {
        if (Serial.available()) {
            int c = Serial.read();
            if (c == 'f' || c == 'F') break;
        }
        delay(50);
    }
    bool ok = ve.finishCalibration(ROTATIONS);
    Serial.printf("CAL %s: ok=%d, PPR_x4=%.2f\n", label, (int)ok, ve.getEffectivePPR());
}

void setup() {
    Serial.begin(115200);
    delay(500);
    encL.begin();
    encR.begin();
    nvs.begin("lfr");
    velL.begin();
    velR.begin();
    Serial.printf("Loaded PPR L=%.2f R=%.2f\n",
        velL.getEffectivePPR(), velR.getEffectivePPR());
    calibrateAxis("LEFT",  velL);
    calibrateAxis("RIGHT", velR);
    Serial.println("RUN — gire as rodas, leitura a cada 200ms");
}

void loop() {
    static uint32_t last = 0;
    uint32_t now = millis();
    uint32_t dt = now - last;
    last = now;
    velL.update(dt);
    velR.update(dt);
    Serial.printf("rpmL=%.1f  rpmR=%.1f\n", velL.getRPM(), velR.getRPM());
    delay(200);
}
#endif
