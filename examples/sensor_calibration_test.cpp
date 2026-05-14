// Calibração automática + exibição de posição normalizada
// Uso: posicione sobre linha, pressione BTN_START para iniciar calibração (3s),
// depois observe posição e sensores normalizados
#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <SPI.h>
#include "sensors/SensorArray.h"
#include "sensors/Calibration.h"
#include "config.h"

enum class State { WAITING, CALIBRATING, RUNNING };

static Calibration cal(SENSOR_COUNT);
static SensorArray sensors(PIN_SPI_CS, SPI_FREQ, SENSOR_COUNT, cal);
static State state = State::WAITING;
static uint32_t calStart = 0;
static int normalized[SENSOR_COUNT];

void setup() {
    Serial.begin(115200);
    SPI.begin(PIN_SPI_CLK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SPI_CS);
    sensors.begin();
    pinMode(PIN_BTN_START, INPUT_PULLUP);
    Serial.println("Pressione BTN_START para calibrar (3s sweep sobre a linha)");
}

void loop() {
    switch (state) {
        case State::WAITING:
            if (digitalRead(PIN_BTN_START) == LOW) {
                state = State::CALIBRATING;
                calStart = millis();
                Serial.println("Calibrando... mova o robo sobre a linha");
            }
            break;

        case State::CALIBRATING:
            sensors.calibrateSweep(3000);
            Serial.println("Calibracao completa. Posicionando...");
            state = State::RUNNING;
            break;

        case State::RUNNING: {
            float pos = sensors.readPosition();
            Serial.print("pos=");
            Serial.print(pos, 1);
            Serial.print("  lost=");
            Serial.print(sensors.isLineLost() ? "SIM" : "NAO");
            Serial.println();
            delay(50);
            break;
        }
    }
}
#endif
