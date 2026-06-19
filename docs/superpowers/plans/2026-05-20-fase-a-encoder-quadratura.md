# Fase A — Encoder Quadratura 4× Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implementar leitura de encoder com quadratura 4× (4 transições por período de pulso) para os 2 motores N20, totalmente testável no ambiente nativo via injeção de pulsos simulados.

**Architecture:** Classe `Encoder` encapsula 2 pinos (CH-A e CH-B) e mantém contagem com `volatile int32_t`. No ESP32 usa `attachInterruptArg()` com handlers em IRAM em ambos os pinos (RISING+FALLING). No ambiente nativo expõe método `_simulatePulse(a,b)` que permite testes determinísticos sem ISR. Tabela de transição de quadratura traduz pares (estado anterior, estado atual) em incremento ±1 ou 0.

**Tech Stack:** C++17, PlatformIO Unity Test Framework, ESP32 Arduino `attachInterruptArg`, `IRAM_ATTR`, `portENTER_CRITICAL_ISR`.

---

## File Map

| File | Action | Purpose |
|------|--------|---------|
| `src/sensors/Encoder.h` | Create | Interface pública do encoder (RAII, DI, native-friendly) |
| `src/sensors/Encoder.cpp` | Create | Implementação dupla: ESP32 (ISR) + native (simulação) |
| `src/config.h` | Modify | Adicionar comentário explicando encoder + `ENCODER_GLITCH_FILTER_US` |
| `test/test_encoder/test_encoder.cpp` | Create | 9 testes Unity nativos (quadratura, direção, glitches, overflow) |
| `platformio.ini` | No change | `test_build_src = yes` já permite compilar src/ nos testes |

---

## Task A1: Criar arquivo de teste RED com 9 stubs

**Files:**
- Create: `test/test_encoder/test_encoder.cpp`

- [ ] **Step 1: Criar diretório e arquivo de teste com TODOS os 9 testes em RED**

```cpp
// test/test_encoder/test_encoder.cpp
#include <unity.h>
#include "sensors/Encoder.h"

static Encoder* enc = nullptr;

void setUp() {
    enc = new Encoder(18, 19);  // pinos arbitrários — em native não importam
    enc->begin();
}

void tearDown() {
    delete enc;
    enc = nullptr;
}

// ─── 1. Estado inicial ──────────────────────────────────────────────────────
void test_encoder_starts_at_zero() {
    TEST_ASSERT_EQUAL_INT32(0, enc->getCount());
}

// ─── 2. Sequência completa para frente: 00 → 01 → 11 → 10 → 00 = +4 ────────
void test_encoder_forward_full_period_counts_four() {
    enc->_simulatePulse(false, true);   // 00 → 01
    enc->_simulatePulse(true,  true);   // 01 → 11
    enc->_simulatePulse(true,  false);  // 11 → 10
    enc->_simulatePulse(false, false);  // 10 → 00
    TEST_ASSERT_EQUAL_INT32(4, enc->getCount());
}

// ─── 3. Sequência completa para trás: 00 → 10 → 11 → 01 → 00 = −4 ──────────
void test_encoder_reverse_full_period_counts_minus_four() {
    enc->_simulatePulse(true,  false);  // 00 → 10
    enc->_simulatePulse(true,  true);   // 10 → 11
    enc->_simulatePulse(false, true);   // 11 → 01
    enc->_simulatePulse(false, false);  // 01 → 00
    TEST_ASSERT_EQUAL_INT32(-4, enc->getCount());
}

// ─── 4. Transições inválidas (glitch / ISR perdida) são ignoradas ──────────
void test_encoder_invalid_transition_ignored() {
    enc->_simulatePulse(true, true);  // 00 → 11 inválido (pula estado)
    TEST_ASSERT_EQUAL_INT32(0, enc->getCount());
}

// ─── 5. reset() zera contagem ──────────────────────────────────────────────
void test_encoder_reset_zeroes_count() {
    enc->_simulatePulse(false, true);   // 00 → 01 → +1
    enc->_simulatePulse(true,  true);   // 01 → 11 → +1
    TEST_ASSERT_EQUAL_INT32(2, enc->getCount());
    enc->reset();
    TEST_ASSERT_EQUAL_INT32(0, enc->getCount());
}

// ─── 6. getDelta() devolve mudança desde última chamada e zera delta ───────
void test_encoder_getDelta_returns_change_since_last_call() {
    enc->_simulatePulse(false, true);  // +1
    enc->_simulatePulse(true,  true);  // +1
    TEST_ASSERT_EQUAL_INT32(2, enc->getDelta());
    TEST_ASSERT_EQUAL_INT32(0, enc->getDelta());  // 2ª chamada: nada novo
    enc->_simulatePulse(true,  false); // +1
    TEST_ASSERT_EQUAL_INT32(1, enc->getDelta());
}

// ─── 7. getDelta() NÃO zera count() ────────────────────────────────────────
void test_encoder_getDelta_preserves_total_count() {
    enc->_simulatePulse(false, true);
    enc->_simulatePulse(true,  true);
    enc->getDelta();
    TEST_ASSERT_EQUAL_INT32(2, enc->getCount());
}

// ─── 8. getDirection() reflete última transição ────────────────────────────
void test_encoder_direction_matches_last_pulse() {
    enc->_simulatePulse(false, true);
    TEST_ASSERT_EQUAL_INT8(+1, enc->getDirection());
    enc->_simulatePulse(false, false);
    TEST_ASSERT_EQUAL_INT8(-1, enc->getDirection());
}

// ─── 9. Symmetric: avançar 100 + recuar 100 = 0 ────────────────────────────
void test_encoder_symmetric_forward_reverse() {
    // 25 períodos completos para frente (4 transições cada)
    for (int i = 0; i < 25; i++) {
        enc->_simulatePulse(false, true);
        enc->_simulatePulse(true,  true);
        enc->_simulatePulse(true,  false);
        enc->_simulatePulse(false, false);
    }
    TEST_ASSERT_EQUAL_INT32(100, enc->getCount());
    // 25 períodos completos para trás
    for (int i = 0; i < 25; i++) {
        enc->_simulatePulse(true,  false);
        enc->_simulatePulse(true,  true);
        enc->_simulatePulse(false, true);
        enc->_simulatePulse(false, false);
    }
    TEST_ASSERT_EQUAL_INT32(0, enc->getCount());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_encoder_starts_at_zero);
    RUN_TEST(test_encoder_forward_full_period_counts_four);
    RUN_TEST(test_encoder_reverse_full_period_counts_minus_four);
    RUN_TEST(test_encoder_invalid_transition_ignored);
    RUN_TEST(test_encoder_reset_zeroes_count);
    RUN_TEST(test_encoder_getDelta_returns_change_since_last_call);
    RUN_TEST(test_encoder_getDelta_preserves_total_count);
    RUN_TEST(test_encoder_direction_matches_last_pulse);
    RUN_TEST(test_encoder_symmetric_forward_reverse);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar testes — TODOS devem falhar com erro de link**

```bash
pio test -e native -f test_encoder
```

Expected: erro de compilação ou link reclamando que `sensors/Encoder.h` ou métodos da classe `Encoder` não existem.

- [ ] **Step 3: Commit (RED state)**

```bash
git add test/test_encoder/test_encoder.cpp
git commit -m "test(encoder): add 9 failing tests for quadrature 4x decoder"
```

---

## Task A2: Criar Encoder.h com interface pública

**Files:**
- Create: `src/sensors/Encoder.h`

- [ ] **Step 1: Escrever Encoder.h**

```cpp
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
    static void IRAM_ATTR _isrTrampoline(void* ctx);
#endif
};
```

- [ ] **Step 2: Validar que header compila isoladamente**

```bash
pio test -e native -f test_encoder 2>&1 | head -30
```

Expected: ainda erros — `Encoder::Encoder` e métodos não implementados (definidos apenas no header).

- [ ] **Step 3: Commit**

```bash
git add src/sensors/Encoder.h
git commit -m "feat(encoder): add public interface with quadrature 4x lookup table"
```

---

## Task A3: Implementar Encoder.cpp (native path)

**Files:**
- Create: `src/sensors/Encoder.cpp`

- [ ] **Step 1: Criar Encoder.cpp com apenas o caminho NATIVE_BUILD primeiro**

```cpp
// src/sensors/Encoder.cpp
#include "sensors/Encoder.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#endif

constexpr int8_t Encoder::QUAD_TABLE[16];

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

#endif  // NATIVE_BUILD
```

- [ ] **Step 2: Rodar testes nativos — TODOS devem passar**

```bash
pio test -e native -f test_encoder
```

Expected:
```
test_encoder_starts_at_zero                          [PASSED]
test_encoder_forward_full_period_counts_four         [PASSED]
test_encoder_reverse_full_period_counts_minus_four   [PASSED]
test_encoder_invalid_transition_ignored              [PASSED]
test_encoder_reset_zeroes_count                      [PASSED]
test_encoder_getDelta_returns_change_since_last_call [PASSED]
test_encoder_getDelta_preserves_total_count          [PASSED]
test_encoder_direction_matches_last_pulse            [PASSED]
test_encoder_symmetric_forward_reverse               [PASSED]
9 Tests 0 Failures 0 Ignored
OK
```

- [ ] **Step 3: Commit (GREEN state — native path)**

```bash
git add src/sensors/Encoder.cpp
git commit -m "feat(encoder): implement native simulation path with quadrature decoding"
```

---

## Task A4: Adicionar caminho ESP32 (ISR) em Encoder.cpp

**Files:**
- Modify: `src/sensors/Encoder.cpp`

- [ ] **Step 1: Adicionar bloco ESP32 ao final do arquivo (antes do último `#endif`)**

Adicionar ao final de `src/sensors/Encoder.cpp`, **antes** da linha `#endif  // NATIVE_BUILD`, fechar o `#ifdef NATIVE_BUILD` e abrir o `#else`:

```cpp
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
```

Estrutura final do arquivo deve ser:

```cpp
#include "sensors/Encoder.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#endif

// ... constructor, getters, _applyTransition (compartilhados) ...

#ifdef NATIVE_BUILD
// ... begin() native + _simulatePulse() ...
#else
// ... begin() ESP32 + _isrTrampoline() ...
#endif
```

- [ ] **Step 2: Rodar testes nativos — devem continuar passando (não regrediu)**

```bash
pio test -e native -f test_encoder
```

Expected: `9 Tests 0 Failures 0 Ignored`

- [ ] **Step 3: Compilar para esp32dev — verifica zero warnings**

```bash
pio run -e esp32dev 2>&1 | tail -20
```

Expected: `SUCCESS` ou ao menos sem erros. Warnings de variáveis não usadas em outros lugares são pré-existentes; o Encoder.cpp não deve introduzir nenhum.

- [ ] **Step 4: Commit**

```bash
git add src/sensors/Encoder.cpp
git commit -m "feat(encoder): add ESP32 ISR path with attachInterruptArg trampoline"
```

---

## Task A5: Atualizar config.h com comentário e constantes auxiliares

**Files:**
- Modify: `src/config.h`

- [ ] **Step 1: Localizar bloco de pinos do encoder (linhas 17-20)**

```bash
grep -n "PIN_ENC" src/config.h
```

Expected output:
```
17:#define PIN_ENC_LEFT_A      18
18:#define PIN_ENC_LEFT_B      19
19:#define PIN_ENC_RIGHT_A     36
20:#define PIN_ENC_RIGHT_B     39
```

- [ ] **Step 2: Substituir as 4 linhas adicionando comentário explicativo acima**

Editar `src/config.h` substituindo o bloco existente por:

```cpp
// ═══ ENCODERS Hall (Fase A — leitura quadratura 4×) ════════════════════════
// GPIO 36 e 39 são input-only no ESP32 e não têm pull-up interno: use 10kΩ externo.
// Resolução: 4 transições por período × PPR efetivo (medido na Fase B).
#define PIN_ENC_LEFT_A      18
#define PIN_ENC_LEFT_B      19
#define PIN_ENC_RIGHT_A     36
#define PIN_ENC_RIGHT_B     39
```

- [ ] **Step 3: Verificar que main.cpp ainda compila (não usa esses pinos ainda)**

```bash
pio run -e esp32dev 2>&1 | tail -5
```

Expected: build successful.

- [ ] **Step 4: Commit**

```bash
git add src/config.h
git commit -m "docs(config): annotate encoder pins with quadrature 4x context"
```

---

## Task A6: Smoke test ESP32 (opcional — bench)

**Files:**
- Create: `examples/encoder_smoke_test.cpp`

- [ ] **Step 1: Criar exemplo de bench test**

```cpp
// examples/encoder_smoke_test.cpp
// Lê os 2 encoders e imprime count + delta a cada 100ms via Serial.
// Gire as rodas com a mão e veja os valores mudando — sinal de vida.
#ifndef NATIVE_BUILD
#include <Arduino.h>
#include "config.h"
#include "sensors/Encoder.h"

static Encoder encL(PIN_ENC_LEFT_A,  PIN_ENC_LEFT_B);
static Encoder encR(PIN_ENC_RIGHT_A, PIN_ENC_RIGHT_B);

void setup() {
    Serial.begin(115200);
    delay(200);
    encL.begin();
    encR.begin();
    Serial.println("countL,deltaL,dirL,countR,deltaR,dirR");
}

void loop() {
    Serial.printf("%ld,%ld,%d,%ld,%ld,%d\n",
        (long)encL.getCount(), (long)encL.getDelta(), (int)encL.getDirection(),
        (long)encR.getCount(), (long)encR.getDelta(), (int)encR.getDirection());
    delay(100);
}
#endif
```

- [ ] **Step 2: Compilar (sem upload) — confirma que não quebra build**

```bash
pio run -e esp32dev 2>&1 | tail -5
```

Expected: build successful.

- [ ] **Step 3: Commit**

```bash
git add examples/encoder_smoke_test.cpp
git commit -m "test(encoder): add bench smoke test for ESP32 quadrature reads"
```

---

## Critério de "Done" — Fase A

- [ ] `pio test -e native -f test_encoder` → 9/9 PASSED
- [ ] `pio run -e esp32dev` → SUCCESS sem warnings novos
- [ ] Encoder.h documentado no topo (Doxygen-style já presente)
- [ ] Pinos de encoder anotados em config.h com explicação de quadratura 4×
- [ ] Encoder não é ainda usado em main.cpp (integração fica para Fase C — CascadeController)
- [ ] Sem regressão em testes existentes:

```bash
pio test -e native 2>&1 | tail -3
```

Expected: todos os outros suites (test_pid, test_speed_profile, test_calibration, test_motors, test_line_follower, test_velocity_profile, test_lap_timer, test_smoke) continuam passando.
