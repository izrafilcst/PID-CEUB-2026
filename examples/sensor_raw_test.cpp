// Leitura bruta dos 8 sensores — saída CSV no Serial para análise
// Flash no ESP32 e abra Serial Monitor a 115200
#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <SPI.h>
#include "sensors/SensorArray.h"
#include "sensors/Calibration.h"
#include "config.h"

static Calibration cal(SENSOR_COUNT);
static SensorArray sensors(PIN_SPI_CS, SPI_FREQ, SENSOR_COUNT, cal);
static int raw[SENSOR_COUNT];
static uint32_t lastPrint = 0;

void setup() {
    Serial.begin(115200);
    SPI.begin(PIN_SPI_CLK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SPI_CS);
    sensors.begin();
    Serial.println("s0,s1,s2,s3,s4,s5,s6,s7");
}

void loop() {
    sensors.readAll(raw);
    if (millis() - lastPrint >= 50) {
        lastPrint = millis();
        for (int i = 0; i < SENSOR_COUNT; i++) {
            Serial.print(raw[i]);
            if (i < SENSOR_COUNT - 1) Serial.print(',');
        }
        Serial.println();
    }
}
#endif
