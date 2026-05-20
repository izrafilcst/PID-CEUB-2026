// src/sensors/Encoder.cpp
#include "sensors/Encoder.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#endif

Encoder::Encoder(uint8_t pinA, uint8_t pinB)
    : _pinA(pinA), _pinB(pinB),
      _count(0), _lastReadCount(0), _lastState(0), _lastDir(0) {}

int32_t Encoder::getCount() const {
    return _count;
}

int32_t Encoder::getDelta() {
    int32_t curr = _count;
    int32_t delta = curr - _lastReadCount;
    _lastReadCount = curr;
    return delta;
}

void Encoder::reset() {
    _count = 0;
    _lastReadCount = 0;
    _lastDir = 0;
}

int8_t Encoder::getDirection() const {
    return _lastDir;
}

void Encoder::_applyTransition(uint8_t newState) {
    uint8_t prev = _lastState & 0x3;
    uint8_t curr = newState   & 0x3;
    int8_t delta = QUAD_TABLE[(curr << 2) | prev];
    if (delta != 0) {
        _count += delta;
        _lastDir = delta;
    }
    _lastState = curr;
}

#ifdef NATIVE_BUILD

void Encoder::begin() {
    _count = 0;
    _lastReadCount = 0;
    _lastDir = 0;
    _lastState = 0;  // assume estado inicial 00 (ambos baixos)
}

void Encoder::_simulatePulse(bool a, bool b) {
    uint8_t newState = (a ? 0x2 : 0x0) | (b ? 0x1 : 0x0);
    _applyTransition(newState);
}

#else  // ESP32 build

void Encoder::begin() {
    pinMode(_pinA, INPUT_PULLUP);
    pinMode(_pinB, INPUT_PULLUP);
    _lastState  = (digitalRead(_pinA) ? 0x2 : 0x0)
                | (digitalRead(_pinB) ? 0x1 : 0x0);
    _count = 0;
    _lastReadCount = 0;
    _lastDir = 0;
    // Uma única ISR de trampolim faz pollread dos 2 pinos em ambas as bordas
    attachInterruptArg(digitalPinToInterrupt(_pinA), _isrTrampoline, this, CHANGE);
    attachInterruptArg(digitalPinToInterrupt(_pinB), _isrTrampoline, this, CHANGE);
}

void IRAM_ATTR Encoder::_isrTrampoline(void* ctx) {
    Encoder* self = static_cast<Encoder*>(ctx);
    uint8_t newState = (digitalRead(self->_pinA) ? 0x2 : 0x0)
                     | (digitalRead(self->_pinB) ? 0x1 : 0x0);
    self->_applyTransition(newState);
}

#endif  // NATIVE_BUILD
