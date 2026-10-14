#include "sensors/ButtonInput.h"

ButtonInput::ButtonInput(uint32_t debounceMs, uint32_t shortMaxMs, uint32_t longMs)
    : _debounceMs(debounceMs), _shortMaxMs(shortMaxMs), _longMs(longMs) {}

ButtonEvent ButtonInput::update(bool pressed, uint32_t nowMs) {
    if (pressed != _raw) {
        _raw = pressed;
        _rawSince = nowMs;
    }
    if (_raw != _stable && nowMs - _rawSince >= _debounceMs) {
        _stable = _raw;
        if (_stable) {
            _pressedAt = nowMs;
            _longFired = false;
            return ButtonEvent::PRESS;
        } else {
            const bool wasLong = _longFired;
            _longFired = false;
            if (!wasLong && nowMs - _pressedAt < _shortMaxMs) return ButtonEvent::SHORT;
            return ButtonEvent::NONE;
        }
    }
    if (_stable && !_longFired && nowMs - _pressedAt >= _longMs) {
        _longFired = true;
        return ButtonEvent::LONG;
    }
    return ButtonEvent::NONE;
}
