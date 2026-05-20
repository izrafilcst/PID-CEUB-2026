// src/sensors/Encoder.h
#pragma once
#include <cstdint>

/**
 * Leitor de encoder em quadratura 4× — máxima resolução por período.
 *
 * Hardware (ESP32): conectar CH-A e CH-B aos pinos passados no construtor.
 * Resistor 10 kΩ externo para VCC nos pinos de entrada (GPIO 36 e 39 são input-only).
 *
 * Native build: ISRs são suprimidas. Use _simulatePulse(a, b) nos testes para
 * injetar transições determinísticas.
 *
 * Quadratura 4×: cada período do encoder produz 4 transições (00 → 01 → 11 → 10 → 00).
 * Sentido de incremento é definido pela ordem das transições.
 */
class Encoder {
public:
    Encoder(uint8_t pinA, uint8_t pinB);

    void begin();

    int32_t getCount() const;     // total acumulado (com sinal)
    int32_t getDelta();           // pulsos desde a última chamada (zera delta interno)
    void    reset();              // zera count e delta
    int8_t  getDirection() const; // +1, 0 ou −1 (sinal da última transição válida)

#ifdef NATIVE_BUILD
    // Injeta uma transição síncrona — equivalente a uma ISR disparando com pinos
    // em estados (a, b). Tabela de quadratura aplica delta conforme transição.
    void _simulatePulse(bool a, bool b);
#endif

private:
    uint8_t  _pinA;
    uint8_t  _pinB;
    volatile int32_t _count;
    int32_t  _lastReadCount;
    volatile uint8_t _lastState;  // 2 bits: (A << 1) | B
    volatile int8_t  _lastDir;

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
