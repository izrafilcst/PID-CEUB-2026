#pragma once
#include <cstdint>

/**
 * Disparo periódico sem deriva (A-WP1-06): a fase avança de `period` em `period`, então quem
 * consulta a cada 10 ms com período de 33 ms dispara, em média, a cada 33,3 ms (~30 Hz) e não a
 * cada 40 ms. Depois de um atraso de mais de um período, dispara uma vez só e ressincroniza (sem
 * rajada). Comparações por diferença com sinal: atravessa o estouro do millis().
 */
class PeriodicTimer {
public:
    explicit PeriodicTimer(uint32_t periodMs) : _period(periodMs > 0 ? periodMs : 1) {}

    /** True when the next period started; the first call always fires. */
    bool due(uint32_t nowMs) {
        if (_started && static_cast<int32_t>(nowMs - _next) < 0) return false;
        _next = _started ? _next + _period : nowMs + _period;
        _started = true;
        if (static_cast<int32_t>(nowMs - _next) >= 0) _next = nowMs + _period;  // atraso longo
        return true;
    }

private:
    uint32_t _period;
    uint32_t _next = 0;
    bool     _started = false;
};
