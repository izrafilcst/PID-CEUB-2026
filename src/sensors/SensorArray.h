#pragma once
#include <cstdint>
#include "sensors/Calibration.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <SPI.h>
#endif

class SensorArray {
public:
    SensorArray(uint8_t csPin, uint32_t spiFreq, int sensorCount, Calibration& cal);

    void begin();

    // Lê todos os sensores via SPI — deve completar em < 100µs
    void readAll(int* rawOut);

    // Lê, normaliza via Calibration e retorna posição ponderada
    float readPosition();

    // Executa N leituras movendo sobre a linha para calibrar
    void calibrateSweep(uint32_t durationMs);

    bool isLineLost() const;

    static constexpr int MAX_SENSORS = 8;

private:
    uint8_t _csPin;
    uint32_t _spiFreq;
    int _count;
    Calibration& _cal;
    int _raw[MAX_SENSORS];
    int _normalized[MAX_SENSORS];

#ifndef NATIVE_BUILD
    uint16_t _readChannel(uint8_t channel);
#endif
};
