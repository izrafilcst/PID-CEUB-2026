#include "sensors/SensorArray.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <SPI.h>

SensorArray::SensorArray(uint8_t csPin, uint32_t spiFreq, int count, Calibration& cal)
    : _csPin(csPin), _spiFreq(spiFreq), _count(count), _cal(cal) {}

void SensorArray::begin() {
    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);
    SPI.begin();
}

uint16_t SensorArray::_readChannel(uint8_t channel) {
    // MCP3008: modo SGL, canal 0-7
    // Protocolo: START bit + SGL/DIFF + D2 D1 D0 + 12 clocks de resultado
    SPI.beginTransaction(SPISettings(_spiFreq, MSBFIRST, SPI_MODE0));
    digitalWrite(_csPin, LOW);

    uint8_t cmd1 = 0x01;                          // start bit
    uint8_t cmd2 = (0x08 | channel) << 4;         // SGL=1, canal, nibble alto
    SPI.transfer(cmd1);
    uint8_t hi = SPI.transfer(cmd2);
    uint8_t lo = SPI.transfer(0x00);

    digitalWrite(_csPin, HIGH);
    SPI.endTransaction();

    return ((hi & 0x03) << 8) | lo;
}

void SensorArray::readAll(int* rawOut) {
    for (int i = 0; i < _count; i++) {
        rawOut[i] = static_cast<int>(_readChannel(static_cast<uint8_t>(i)));
        _raw[i] = rawOut[i];
    }
}

float SensorArray::readPosition() {
    readAll(_raw);
    _cal.normalize(_raw, _normalized);
    return _cal.weightedPosition(_normalized);
}

void SensorArray::calibrateSweep(uint32_t durationMs) {
    uint32_t start = millis();
    while (millis() - start < durationMs) {
        readAll(_raw);
        _cal.update(_raw);
    }
}

bool SensorArray::isLineLost() const {
    return _cal.isLineLost();
}

#else
// Stubs para compilação native
SensorArray::SensorArray(uint8_t cs, uint32_t freq, int count, Calibration& cal)
    : _csPin(cs), _spiFreq(freq), _count(count), _cal(cal) {}
void SensorArray::begin() {}
void SensorArray::readAll(int* rawOut) {
    for (int i = 0; i < _count; i++) rawOut[i] = 0;
}
float SensorArray::readPosition() { return 0.0f; }
void SensorArray::calibrateSweep(uint32_t) {}
bool SensorArray::isLineLost() const { return false; }
#endif
