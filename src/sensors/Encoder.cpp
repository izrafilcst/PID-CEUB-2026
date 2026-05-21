// src/sensors/Encoder.cpp
#include "sensors/Encoder.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#endif

// Out-of-class definition required for GCC < 9 when the static constexpr
// array is odr-used (address taken / passed to linker) — C++17 §9.4.2.
constexpr int8_t Encoder::QUAD_TABLE[16];

Encoder::Encoder(uint8_t pinA, uint8_t pinB)
    : _pinA(pinA), _pinB(pinB),
      _count(0), _lastReadCount(0), _lastState(0), _lastDir(0) {}

int32_t Encoder::getCount() const {
#ifndef NATIVE_BUILD
    portENTER_CRITICAL(&_mux);
#endif
    uint32_t snap = _count;
#ifndef NATIVE_BUILD
    portEXIT_CRITICAL(&_mux);
#endif
    // Cast unsigned→signed preserva o padrão de bits; -4 ↔ 0xFFFFFFFC.
    return static_cast<int32_t>(snap);
}

int32_t Encoder::getDelta() {
#ifndef NATIVE_BUILD
    portENTER_CRITICAL(&_mux);
#endif
    uint32_t curr = _count;
    // Subtração unsigned é well-defined (wrap modular). O cast para signed
    // dá o delta correto enquanto |delta| < 2^31 — milhões de pulsos.
    int32_t delta = static_cast<int32_t>(curr - _lastReadCount);
    _lastReadCount = curr;
#ifndef NATIVE_BUILD
    portEXIT_CRITICAL(&_mux);
#endif
    return delta;
}

void Encoder::reset() {
#ifndef NATIVE_BUILD
    portENTER_CRITICAL(&_mux);
#endif
    _count = 0;
    _lastReadCount = 0;
    _lastState = 0;
    _lastDir = 0;
#ifndef NATIVE_BUILD
    portEXIT_CRITICAL(&_mux);
#endif
}

int8_t Encoder::getDirection() const {
    return _lastDir;
}

#ifndef NATIVE_BUILD
// IRAM_ATTR previne flash-cache stalls (~20–200 µs) se o cache miss
// acontecer durante a ISR — crítico no loop de 2 ms do PID.
void IRAM_ATTR Encoder::_applyTransition(uint8_t newState) {
#else
void Encoder::_applyTransition(uint8_t newState) {
#endif
    uint8_t prev = _lastState & 0x3;
    uint8_t curr = newState   & 0x3;
    int8_t delta = QUAD_TABLE[(curr << 2) | prev];
    if (delta != 0) {
        // _count é uint32_t — unsigned overflow é well-defined (wrap).
        _count = static_cast<uint32_t>(static_cast<int32_t>(_count) + delta);
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
    // Idempotência: detach any previous attachment before re-attaching
    if (_started) {
        detachInterrupt(digitalPinToInterrupt(_pinA));
        detachInterrupt(digitalPinToInterrupt(_pinB));
    }
    // GPIO 36/39 são input-only e NÃO têm pull-up interno; usar INPUT explícito
    // documenta que o pull-up externo de 10 kΩ é obrigatório no hardware.
    pinMode(_pinA, INPUT);
    pinMode(_pinB, INPUT);
    _lastState  = (digitalRead(_pinA) ? 0x2 : 0x0)
                | (digitalRead(_pinB) ? 0x1 : 0x0);
    _count = 0;
    _lastReadCount = 0;
    _lastDir = 0;
    // Uma única ISR de trampolim faz pollread dos 2 pinos em ambas as bordas
    attachInterruptArg(digitalPinToInterrupt(_pinA), _isrTrampoline, this, CHANGE);
    attachInterruptArg(digitalPinToInterrupt(_pinB), _isrTrampoline, this, CHANGE);
    _started = true;
}

void IRAM_ATTR Encoder::_isrTrampoline(void* ctx) {
    Encoder* self = static_cast<Encoder*>(ctx);
    uint8_t newState = (digitalRead(self->_pinA) ? 0x2 : 0x0)
                     | (digitalRead(self->_pinB) ? 0x1 : 0x0);
    self->_applyTransition(newState);
}

#endif  // NATIVE_BUILD
