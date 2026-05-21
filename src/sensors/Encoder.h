// src/sensors/Encoder.h
#pragma once
#include <cstdint>

#ifndef NATIVE_BUILD
#include <freertos/FreeRTOS.h>  // portMUX_TYPE
#endif

/**
 * Leitor de encoder em quadratura 4× — máxima resolução por período.
 *
 * Hardware (ESP32): conectar CH-A e CH-B aos pinos passados no construtor.
 * Resistor 10 kΩ externo para VCC nos pinos de entrada
 * (GPIO 36 e 39 são input-only e NÃO têm pull-up interno — pinMode INPUT_PULLUP
 * é silenciosamente ignorado, então usamos INPUT explícito).
 *
 * Native build: ISRs são suprimidas. Use _simulatePulse(a, b) nos testes para
 * injetar transições determinísticas.
 *
 * Quadratura 4×: cada período do encoder produz 4 transições (00 → 01 → 11 → 10 → 00).
 * Sentido de incremento é definido pela ordem das transições.
 *
 * Thread safety (ESP32 dual-core):
 *   - ISR é o único produtor de _count/_lastState/_lastDir
 *   - Consumers (getCount/getDelta/reset) entram em critical section (portMUX)
 *   - Overflow é seguro: _count é uint32_t (unsigned wrap é well-defined em C++)
 *     e o delta é calculado como (int32_t)(curr − last) — válido enquanto
 *     |delta| < 2^31 (~2 bilhões de pulsos entre chamadas — ~3 dias @ 600k/s).
 */
class Encoder {
public:
    Encoder(uint8_t pinA, uint8_t pinB);

    void begin();

    int32_t getCount() const;     // total acumulado (com sinal, derivado de uint32_t)
    int32_t getDelta();           // pulsos desde a última chamada (zera delta interno)
    void    reset();              // zera count, delta e estado
    int8_t  getDirection() const; // +1, 0 ou −1 (sinal da última transição válida)

#ifdef NATIVE_BUILD
    // Injeta uma transição síncrona — equivalente a uma ISR disparando com pinos
    // em estados (a, b). Tabela de quadratura aplica delta conforme transição.
    void _simulatePulse(bool a, bool b);
#endif

private:
    uint8_t  _pinA;
    uint8_t  _pinB;
    volatile uint32_t _count;             // unsigned: wrap é well-defined em C++
    volatile uint32_t _lastReadCount;     // volatile p/ consistência com _count
    volatile uint8_t  _lastState;         // 2 bits: (A << 1) | B
    volatile int8_t   _lastDir;
#ifndef NATIVE_BUILD
    mutable portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
    bool _started = false;                // begin() idempotente
#endif

    static constexpr int8_t QUAD_TABLE[16] = {
        //   prev=00 prev=01 prev=10 prev=11
        /* curr=00 */  0, -1, +1,  0,
        /* curr=01 */ +1,  0,  0, -1,
        /* curr=10 */ -1,  0,  0, +1,
        /* curr=11 */  0, +1, -1,  0
    };

    void _applyTransition(uint8_t newState);

#ifndef NATIVE_BUILD
    // IRAM_ATTR appears only on the definition (Encoder.cpp). On Arduino-ESP32
    // GCC, the section attribute on a class-member declaration is parsed as
    // a stray identifier ("variable or field 'IRAM_ATTR' declared void").
    static void _isrTrampoline(void* ctx);
#endif
};
