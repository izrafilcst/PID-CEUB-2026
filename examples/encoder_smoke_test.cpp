// examples/encoder_smoke_test.cpp
// Lê os 2 encoders e imprime count + delta a cada 100ms via Serial.
// Gire as rodas com a mão e veja os valores mudando — sinal de vida.
#ifndef NATIVE_BUILD
#include <Arduino.h>
#include "config.h"
#include "sensors/Encoder.h"

static Encoder encL(PIN_ENC_LEFT_A,  PIN_ENC_LEFT_B);
static Encoder encR(PIN_ENC_RIGHT_A, PIN_ENC_RIGHT_B);

void setup() {
    Serial.begin(115200);
    delay(200);
    encL.begin();
    encR.begin();
    Serial.println("countL,deltaL,dirL,countR,deltaR,dirR");
}

void loop() {
    Serial.printf("%ld,%ld,%d,%ld,%ld,%d\n",
        (long)encL.getCount(), (long)encL.getDelta(), (int)encL.getDirection(),
        (long)encR.getCount(), (long)encR.getDelta(), (int)encR.getDirection());
    delay(100);
}
#endif
