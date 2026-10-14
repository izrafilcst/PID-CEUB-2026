#pragma once
#include <cstdint>

enum class ButtonEvent : uint8_t { NONE, SHORT, LONG, PRESS };

/**
 * Botão com debounce e toque curto/longo (spec §4.6, F-04).
 * PRESS: uma vez por aperto, quando ele passa do debounce (E-3: em ARMED/RUNNING o carro para
 * aí). SHORT: soltar antes de shortMaxMs. LONG: dispara ao completar longMs segurando, sem
 * esperar soltar. Entre os dois, nada. Depois de um LONG, soltar não gera SHORT.
 */
class ButtonInput {
public:
    explicit ButtonInput(uint32_t debounceMs = 30, uint32_t shortMaxMs = 800,
                         uint32_t longMs = 1500);

    ButtonEvent update(bool pressed, uint32_t nowMs);
    bool isPressed() const { return _stable; }

private:
    uint32_t _debounceMs;
    uint32_t _shortMaxMs;
    uint32_t _longMs;
    bool     _raw = false;
    bool     _stable = false;
    bool     _longFired = false;
    uint32_t _rawSince = 0;
    uint32_t _pressedAt = 0;
};
