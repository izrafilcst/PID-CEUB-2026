# Timing (dt real) + Tratamento de Cruzamento — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Corrigir o bug de timing do loop de controle (dt truncado + perda de pulsos do encoder), plumbar o dt real (variável, em µs/s) para o estimador de velocidade e para as duas camadas de PID, fechar duas corridas de concorrência entre cores, tornar o `beep()` não-bloqueante, e adicionar tratamento de cruzamento (interseção) usando um novo sinal de detecção + o `isLineLost()` já existente.

**Architecture:** Mantém o loop de controle **livre** (não travado em taxa fixa), mas passa o intervalo real medido (`micros()`) por toda a cadeia. O `VelocityEstimator` passa a **acumular** pulsos e tempo por uma janela mínima real (variável) antes de estimar RPM — isso, ao mesmo tempo, elimina o descarte silencioso de pulsos e o problema de resolução dos 28 PPR em alta taxa. As camadas de PID passam a receber o `dt` real por chamada. As corridas são fechadas estendendo a seção crítica (`lineFollowerMux`) sobre o `cascade.compute()` e restringindo comandos de calibração ao estado não-`RUNNING`.

**Tech Stack:** C++17, PlatformIO (env `native` = Unity, env `esp32dev` = Arduino/NimBLE). Testes puros rodam em `native`; `main.cpp` é firmware-only (`#ifndef NATIVE_BUILD`) e é validado por compilação + bancada.

## Global Constraints

- Estilo: `PascalCase` classes, `camelCase` métodos/vars, `SCREAMING_SNAKE` constantes; 4 espaços; linha ≤ 100 col; `#pragma once` nos headers.
- Sem warnings sob `-Wall -Wextra -std=gnu++17` (env `native`).
- Guards de plataforma: código Arduino/NimBLE fica sob `#ifndef NATIVE_BUILD`.
- DI no construtor; sem globals mutáveis novos fora do `main.cpp`; sem `delay()`.
- Toda lógica nova de controle/sensor precisa de teste Unity no ambiente `native`.
- Gate de verificação por tarefa: `pio test -e native` (esperado: todas as suítes `succeeded`). O `main.cpp` também deve compilar em `esp32dev` quando o toolchain estiver disponível.
- Decisões travadas com o dono do projeto (2026-07-12): **dt variável real em µs** (não taxa fixa); **escopo = timing + corridas + cruzamento**; **`isLineLost()` deve dirigir comportamento**, não só telemetria.

---

### Task 1: VelocityEstimator — dt real em µs + acumulação por janela mínima

**Files:**
- Modify: `src/config.h` (adicionar `VELOCITY_MIN_WINDOW_US`)
- Modify: `src/sensors/VelocityEstimator.h` (unidade µs, `setMinWindowUs`, acumuladores)
- Modify: `src/sensors/VelocityEstimator.cpp` (nova lógica de `update`)
- Test: `test/test_velocity_estimator/test_velocity_estimator.cpp`

**Interfaces:**
- Consumes: `Encoder::getDelta()` (int32_t, drena delta interno), `Encoder::getCount()`, `IPersistentStore`.
- Produces:
  - `void VelocityEstimator::update(uint32_t dtUs)` — `dtUs` agora em **microssegundos**. `dtUs==0` reseta a janela (drena e zera acumuladores, não toca IIR).
  - `void VelocityEstimator::setMinWindowUs(uint32_t us)` — janela mínima real antes de estimar (default `VELOCITY_MIN_WINDOW_US`).
  - `float VelocityEstimator::getRPM() const` — inalterado.

- [ ] **Step 1: Escrever os testes que falham (acumulação e janela mínima)**

Editar `test/test_velocity_estimator/test_velocity_estimator.cpp`. Adicionar helper e dois testes novos após o helper `simulateRotations` (linha ~37):

```cpp
// Injeta N períodos de quadratura para frente (4 contagens por período).
static void injectPeriods(Encoder& e, int periods) {
    for (int i = 0; i < periods; i++) {
        e._simulatePulse(false, true);
        e._simulatePulse(true,  true);
        e._simulatePulse(true,  false);
        e._simulatePulse(false, false);
    }
}

// ─── Pulsos NÃO são perdidos em ticks menores que a janela mínima ─────────
void test_velocity_accumulates_pulses_across_subwindow_ticks() {
    ve->setFilterAlpha(1.0f);   // sem IIR — resultado determinístico
    ve->setMinWindowUs(6000);   // janela mínima = 6 ms
    injectPeriods(*enc, 4);     // 16 contagens
    ve->update(4000);           // 4 ms < 6 ms → acumula, NÃO estima ainda
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ve->getRPM());
    injectPeriods(*enc, 3);     // +12 contagens = 28 no total
    ve->update(4000);           // acumUs=8 ms ≥ 6 ms → estima sobre 8 ms, 28 contagens
    // 28 * 60e6 / (8000 µs * 28 PPR) = 7500 RPM
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 7500.0f, ve->getRPM());
}

// ─── Abaixo da janela mínima, mantém o RPM anterior ───────────────────────
void test_velocity_holds_rpm_below_min_window() {
    ve->setFilterAlpha(1.0f);
    ve->setMinWindowUs(6000);
    injectPeriods(*enc, 7);     // 28 contagens
    ve->update(2000);           // 2 ms < 6 ms → não estima
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ve->getRPM());
}
```

Converter TODAS as chamadas `update(ms)` existentes para microssegundos (× 1000). Edições exatas neste arquivo:
- L41 `ve->update(10);` → `ve->update(10000);`
- L50 `ve->update(100);` → `ve->update(100000);`
- L60 `ve->update(100);` → `ve->update(100000);`
- L66 `ve->update(100);` → `ve->update(100000);`
- L76 `ve->update(0);` → **manter** `ve->update(0);` (semântica de reset)
- L90 `ve->update(100);` → `ve->update(100000);`
- L126 `ve->update(100);` → `ve->update(100000);`

Registrar os dois testes novos no `main()` (após `RUN_TEST(test_velocity_low_pass_smooths_spikes);`):

```cpp
    RUN_TEST(test_velocity_accumulates_pulses_across_subwindow_ticks);
    RUN_TEST(test_velocity_holds_rpm_below_min_window);
```

- [ ] **Step 2: Rodar para confirmar que falha**

Run: `pio test -e native -f test_velocity_estimator`
Expected: FAIL — `setMinWindowUs` não existe (erro de compilação) e/ou os asserts de acumulação falham.

- [ ] **Step 3: Adicionar a constante de janela em `config.h`**

Em `src/config.h`, logo após a linha `#define VELOCITY_FILTER_ALPHA ...` (bloco ENCODER / VELOCITY):

```cpp
// Janela mínima (µs) de tempo real acumulado antes de estimar RPM. Resolve a
// baixa resolução do encoder (28 PPR) em loops sub-ms: pulsos são acumulados
// até a janela fechar, então o RPM é calculado sobre o dt REAL da janela.
#define VELOCITY_MIN_WINDOW_US   4000
```

- [ ] **Step 4: Atualizar o header `VelocityEstimator.h`**

Em `src/sensors/VelocityEstimator.h`, trocar a declaração de `update` e adicionar o setter (após `setFilterAlpha`):

```cpp
    // Atualiza RPM dado o intervalo REAL desde a última chamada (microssegundos).
    // dtUs==0 reseta a janela de acumulação (drena delta, não toca no IIR).
    void update(uint32_t dtUs);

    // Janela mínima (µs) de tempo real acumulado antes de estimar RPM.
    void setMinWindowUs(uint32_t us);
```

Adicionar aos membros privados (após `bool _calibrating;`):

```cpp
    int32_t  _accumCounts;   // pulsos acumulados na janela corrente
    uint32_t _accumUs;       // tempo real acumulado na janela corrente (µs)
    uint32_t _minWindowUs;   // janela mínima antes de estimar
```

- [ ] **Step 5: Reimplementar `update` no `.cpp`**

Em `src/sensors/VelocityEstimator.cpp`:

Trocar a constante de dt máximo (topo do arquivo):

```cpp
// dtUs acumulado máximo antes de descartar a janela (stall / scheduler hiccup).
static constexpr uint32_t DT_MAX_US = 500000;  // 0,5 s
```
(remover a antiga `static constexpr uint32_t DT_MAX_MS = 500;`)

Inicializar os novos membros no construtor (na lista de inicialização, após `_calibrating(false)`):

```cpp
      , _accumCounts(0),
      _accumUs(0),
      _minWindowUs(VELOCITY_MIN_WINDOW_US)
```
(atenção à vírgula: a lista atual termina em `_calibrating(false)` — adicionar as três acima antes do `{`)

No `begin()`, após `_calibrating = false;`, zerar a janela:

```cpp
    _accumCounts = 0;
    _accumUs     = 0;
```

Substituir o corpo INTEIRO de `update(...)` por:

```cpp
void VelocityEstimator::update(uint32_t dtUs) {
    // Sempre drena o delta do encoder para manter os contadores vivos.
    int32_t delta = _enc.getDelta();

    // Durante calibração o usuário gira a roda à mão; não alimentar o IIR.
    if (_calibrating) {
        _accumCounts = 0;
        _accumUs     = 0;
        return;
    }

    // dtUs==0 = sinal de reset (1ª chamada / re-entry): zera a janela.
    if (dtUs == 0) {
        _accumCounts = 0;
        _accumUs     = 0;
        return;
    }

    // Acumula este tick na janela corrente — pulsos NUNCA são descartados.
    _accumCounts += delta;
    _accumUs     += dtUs;

    // Stall: a janela cresceu demais (loop travou) → descarta, mantém RPM.
    if (_accumUs > DT_MAX_US) {
        _accumCounts = 0;
        _accumUs     = 0;
        return;
    }

    // Tempo real insuficiente para estimativa confiável → segue acumulando.
    if (_accumUs < _minWindowUs) return;

    // Estima sobre o dt REAL da janela (variável), então reinicia a janela.
    if (_pprX4 > 0.0f) {
        float rpmRaw = (static_cast<float>(_accumCounts) * 60000000.0f)
                     / (static_cast<float>(_accumUs) * _pprX4);
        _rpmFiltered = _alpha * rpmRaw + (1.0f - _alpha) * _rpmFiltered;
    }
    _accumCounts = 0;
    _accumUs     = 0;
}

void VelocityEstimator::setMinWindowUs(uint32_t us) {
    _minWindowUs = us;
}
```

Em `finishCalibration(...)` e `startCalibration(...)`, após zerar `_rpmFiltered`, zerar também os acumuladores (para não vazar contagem da calibração):

```cpp
    _accumCounts = 0;
    _accumUs     = 0;
```
(adicionar em ambos, junto de onde já fazem `_rpmFiltered = 0.0f;`)

- [ ] **Step 6: Rodar os testes**

Run: `pio test -e native -f test_velocity_estimator`
Expected: PASS (todas as suítes de velocity, incl. as 2 novas).

- [ ] **Step 7: Commit**

```bash
git add src/config.h src/sensors/VelocityEstimator.h src/sensors/VelocityEstimator.cpp test/test_velocity_estimator/test_velocity_estimator.cpp
git commit -m "fix(velocity): dt real em µs + acumulação por janela; elimina perda de pulsos

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 2: Plumbar dt real para as duas camadas de PID

**Files:**
- Modify: `src/strategy/LineFollower.h` (param `dtSec` + acessores de flag)
- Modify: `src/strategy/LineFollower.cpp` (passar `dtSec` ao PID externo)
- Modify: `src/control/CascadeController.h` (param `dtSec`)
- Modify: `src/control/CascadeController.cpp` (passar `dtSec` aos PIDs internos)
- Test: `test/test_cascade/test_cascade.cpp`, `test/test_line_follower/test_line_follower.cpp`

**Interfaces:**
- Consumes: `PIDController::compute(float setpoint, float measurement, float dt)` (já existe — sobrecarga de 3 args).
- Produces:
  - `void LineFollower::update(const int* rawSensors, int& leftPwm, int& rightPwm, float* normalizedError = nullptr, float dtSec = 0.01f)`
  - `void CascadeController::compute(float correction, float targetBaseRpm, float actualRpmL, float actualRpmR, int& pwmL, int& pwmR, float dtSec = 0.002f)`

> Os defaults preservam o comportamento atual dos testes existentes; só o `main.cpp` (Task 4) passará o dt real.

- [ ] **Step 1: Escrever o teste que falha (cascade usa o dt fornecido)**

Em `test/test_cascade/test_cascade.cpp`, adicionar após `test_cascade_set_inner_gains_updates_both_pids`:

```cpp
// ─── 9. compute() usa o dt fornecido no termo derivativo ─────────────────
void test_cascade_uses_provided_dt_for_derivative() {
    // PIDs internos só com Kd (derivada no processo). Kp=Ki=0.
    PIDController dL(0.0f, 0.0f, 0.001f, -1023.0f, 1023.0f, 0.01f);
    PIDController dR(0.0f, 0.0f, 0.001f, -1023.0f, 1023.0f, 0.01f);
    CascadeController dcc(dL, dR, 1000.0f, 1023);
    int pwmL = 0, pwmR = 0;

    // 1º compute estabelece _prevMeasurement (=0). dt grande.
    dcc.compute(0.0f, 0.0f, 0.0f, 0.0f, pwmL, pwmR, 0.01f);
    // measurement salta p/ 100: derivada = -(100-0)/dt; Kd=0.001
    dcc.compute(0.0f, 0.0f, 100.0f, 100.0f, pwmL, pwmR, 0.01f);
    int atDtBig = pwmL;   // -0.001 * 100/0.01 = -10

    dL.reset(); dR.reset();
    dcc.compute(0.0f, 0.0f, 0.0f, 0.0f, pwmL, pwmR, 0.002f);
    dcc.compute(0.0f, 0.0f, 100.0f, 100.0f, pwmL, pwmR, 0.002f);
    int atDtSmall = pwmL; // -0.001 * 100/0.002 = -50

    TEST_ASSERT_EQUAL_INT(-10, atDtBig);
    TEST_ASSERT_EQUAL_INT(-50, atDtSmall);
}
```

Registrar no `main()`:

```cpp
    RUN_TEST(test_cascade_uses_provided_dt_for_derivative);
```

- [ ] **Step 2: Rodar para confirmar que falha**

Run: `pio test -e native -f test_cascade`
Expected: FAIL — `compute(...)` não aceita 7º argumento (erro de compilação).

- [ ] **Step 3: Adicionar `dtSec` ao CascadeController**

Em `src/control/CascadeController.h`, trocar a assinatura de `compute`:

```cpp
    void compute(float correction, float targetBaseRpm,
                 float actualRpmL, float actualRpmR,
                 int& pwmL, int& pwmR, float dtSec = 0.002f);
```

Em `src/control/CascadeController.cpp`, ajustar a definição e as duas chamadas internas:

```cpp
void CascadeController::compute(float correction, float targetBaseRpm,
                                float actualRpmL, float actualRpmR,
                                int& pwmL, int& pwmR, float dtSec) {
```
e trocar (passo 3 do corpo):

```cpp
    float outL = _pidL.compute(setpointL, actualRpmL, dtSec);
    float outR = _pidR.compute(setpointR, actualRpmR, dtSec);
```

- [ ] **Step 4: Rodar teste do cascade**

Run: `pio test -e native -f test_cascade`
Expected: PASS (as 8 existentes + a nova).

- [ ] **Step 5: Escrever o teste que falha (line follower usa o dt fornecido)**

Em `test/test_line_follower/test_line_follower.cpp`, adicionar após `test_sequential_derivative_smooths`:

```cpp
void test_line_follower_uses_provided_dt() {
    delete pid; pid = new PIDController(0.0f, 0.0f, 0.0001f, -255.0f, 255.0f);
    delete lf;  lf  = new LineFollower(*cal, *pid, *speed, *drive);

    int raw_center[8] = {0, 0, 0, 700, 700, 0, 0, 0};   // posição ≈ 0
    int raw_right[8]  = {0, 0, 0, 0, 700, 700, 0, 0};    // posição ≈ +1000
    int l, r;

    lf->update(raw_center, l, r, nullptr, 0.01f);        // estabelece prevMeas
    lf->update(raw_right,  l, r, nullptr, 0.01f);
    float corrBig = lf->getLastCorrection();             // ~ +10

    pid->reset();
    lf->update(raw_center, l, r, nullptr, 0.002f);
    lf->update(raw_right,  l, r, nullptr, 0.002f);
    float corrSmall = lf->getLastCorrection();           // ~ +50

    // dt menor → derivada maior → |correção| maior
    TEST_ASSERT_TRUE(fabsf(corrSmall) > fabsf(corrBig));
}
```

Adicionar `#include <cmath>` no topo (para `fabsf`) se ainda não houver, e registrar no `main()`:

```cpp
    RUN_TEST(test_line_follower_uses_provided_dt);
```

- [ ] **Step 6: Rodar para confirmar que falha**

Run: `pio test -e native -f test_line_follower`
Expected: FAIL — `update(...)` não aceita o 5º argumento `dtSec`.

- [ ] **Step 7: Adicionar `dtSec` ao LineFollower**

Em `src/strategy/LineFollower.h`, trocar a assinatura de `update`:

```cpp
    void update(const int* rawSensors, int& leftPwm, int& rightPwm,
                float* normalizedError = nullptr, float dtSec = 0.01f);
```

Em `src/strategy/LineFollower.cpp`, ajustar a definição e a chamada ao PID externo:

```cpp
void LineFollower::update(const int* rawSensors, int& leftPwm, int& rightPwm,
                          float* normalizedError, float dtSec) {
```
e trocar a linha da correção:

```cpp
    float correction = -_pid.compute(0.0f, position, dtSec);
```

- [ ] **Step 8: Rodar todos os testes de controle**

Run: `pio test -e native -f test_line_follower -f test_cascade -f test_pid`
Expected: PASS.

- [ ] **Step 9: Commit**

```bash
git add src/strategy/LineFollower.h src/strategy/LineFollower.cpp src/control/CascadeController.h src/control/CascadeController.cpp test/test_cascade/test_cascade.cpp test/test_line_follower/test_line_follower.cpp
git commit -m "feat(control): plumbar dt real (variável) para PID externo e cascade interno

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 3: Detecção e tratamento de cruzamento + uso do isLineLost

**Files:**
- Modify: `src/sensors/Calibration.h` (`isCrossing()`, membro/constantes)
- Modify: `src/sensors/Calibration.cpp` (contagem de sensores ativos em `weightedPosition`)
- Modify: `src/strategy/LineFollower.h` (acessores `isLineLost()`/`isCrossing()`)
- Modify: `src/strategy/LineFollower.cpp` (override em cruzamento, hold em linha perdida)
- Test: `test/test_calibration/test_calibration.cpp`, `test/test_line_follower/test_line_follower.cpp`

**Interfaces:**
- Produces:
  - `bool Calibration::isCrossing() const` — true quando ≥ `CROSSING_MIN_ACTIVE` sensores normalizados ≥ `CROSSING_ACTIVE_LEVEL`.
  - `bool LineFollower::isLineLost() const`, `bool LineFollower::isCrossing() const`.
  - Comportamento: em cruzamento a correção do último ciclo é forçada a `0` (segue reto); em linha perdida a correção anterior é **mantida** (recuperação de heading).

- [ ] **Step 1: Escrever os testes que falham (Calibration)**

Em `test/test_calibration/test_calibration.cpp`, adicionar após `test_line_lost_returns_last_known`:

```cpp
void test_crossing_detected_when_many_sensors_active() {
    Calibration cal(8);
    int rawMin[8] = {0,0,0,0,0,0,0,0};
    int rawMax[8] = {1000,1000,1000,1000,1000,1000,1000,1000};
    cal.update(rawMin);
    cal.update(rawMax);
    int readings[8] = {800,800,800,800,800,800,800,800};  // linha perpendicular
    int normalized[8];
    cal.normalize(readings, normalized);
    cal.weightedPosition(normalized);
    TEST_ASSERT_TRUE(cal.isCrossing());
    TEST_ASSERT_FALSE(cal.isLineLost());
}

void test_no_crossing_on_single_line() {
    Calibration cal(8);
    int rawMin[8] = {0,0,0,0,0,0,0,0};
    int rawMax[8] = {1000,1000,1000,1000,1000,1000,1000,1000};
    cal.update(rawMin);
    cal.update(rawMax);
    int readings[8] = {0,0,0,700,700,0,0,0};  // linha normal (2 sensores)
    int normalized[8];
    cal.normalize(readings, normalized);
    cal.weightedPosition(normalized);
    TEST_ASSERT_FALSE(cal.isCrossing());
}

void test_no_crossing_when_line_lost() {
    Calibration cal(8);
    int rawMin[8] = {0,0,0,0,0,0,0,0};
    int rawMax[8] = {1000,1000,1000,1000,1000,1000,1000,1000};
    cal.update(rawMin);
    cal.update(rawMax);
    int zeros[8] = {0,0,0,0,0,0,0,0};
    int normalized[8];
    cal.normalize(zeros, normalized);
    cal.weightedPosition(normalized);
    TEST_ASSERT_FALSE(cal.isCrossing());
    TEST_ASSERT_TRUE(cal.isLineLost());
}
```

Registrar no `main()`:

```cpp
    RUN_TEST(test_crossing_detected_when_many_sensors_active);
    RUN_TEST(test_no_crossing_on_single_line);
    RUN_TEST(test_no_crossing_when_line_lost);
```

- [ ] **Step 2: Rodar para confirmar que falha**

Run: `pio test -e native -f test_calibration`
Expected: FAIL — `isCrossing()` não existe.

- [ ] **Step 3: Adicionar `isCrossing` ao header da Calibration**

Em `src/sensors/Calibration.h`, após `bool isLineLost() const { return _lineLost; }`:

```cpp
    bool isCrossing() const { return _crossing; }
```

Adicionar membro (após `mutable bool _lineLost;`):

```cpp
    mutable bool _crossing;
```

Adicionar constantes (após `static constexpr int LINE_LOST_THRESHOLD = 200;`):

```cpp
    // Cruzamento: linha perpendicular acende muitos sensores ao mesmo tempo.
    static constexpr int CROSSING_ACTIVE_LEVEL = 700;  // normalizado [0..1000]
    static constexpr int CROSSING_MIN_ACTIVE   = 6;    // nº mínimo de sensores
```

- [ ] **Step 4: Implementar detecção em `Calibration.cpp`**

Em `src/sensors/Calibration.cpp`:

No construtor, inicializar `_crossing`:

```cpp
Calibration::Calibration(int sensorCount)
    : _count(sensorCount), _lastPosition(0.0f), _lineLost(false), _crossing(false) {
    assert(sensorCount > 0 && sensorCount <= MAX_SENSORS);
    reset();
}
```

No `reset()`, adicionar após `_lineLost = false;`:

```cpp
    _crossing = false;
```

Reescrever `weightedPosition` para contar sensores ativos:

```cpp
float Calibration::weightedPosition(const int* normalized) const {
    long sum = 0;
    long weightedSum = 0;
    int  activeCount = 0;
    const int halfRange = (_count - 1) * static_cast<int>(POSITION_SCALE) / 2;

    for (int i = 0; i < _count; i++) {
        int pos = i * static_cast<int>(POSITION_SCALE) - halfRange;
        sum += normalized[i];
        weightedSum += static_cast<long>(pos) * normalized[i];
        if (normalized[i] >= CROSSING_ACTIVE_LEVEL) activeCount++;
    }

    if (sum < LINE_LOST_THRESHOLD) {
        _lineLost = true;
        _crossing = false;
        return _lastPosition;
    }

    _lineLost = false;
    _crossing = (activeCount >= CROSSING_MIN_ACTIVE);
    _lastPosition = static_cast<float>(weightedSum) / static_cast<float>(sum);
    return _lastPosition;
}
```

- [ ] **Step 5: Rodar teste da calibração**

Run: `pio test -e native -f test_calibration`
Expected: PASS.

- [ ] **Step 6: Escrever os testes que falham (LineFollower)**

Em `test/test_line_follower/test_line_follower.cpp`, adicionar após `test_line_follower_uses_provided_dt`:

```cpp
void test_line_follower_goes_straight_on_crossing() {
    int raw_cross[8] = {800,800,800,800,800,800,800,800};  // cruzamento
    int l, r;
    lf->update(raw_cross, l, r);
    TEST_ASSERT_TRUE(lf->isCrossing());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, lf->getLastCorrection());  // reto
    TEST_ASSERT_EQUAL(r, l);
}

void test_line_follower_holds_correction_when_line_lost() {
    int raw_right[8] = {0,0,0,0,0,0,100,900};  // curva à direita
    int l, r;
    lf->update(raw_right, l, r);
    float held = lf->getLastCorrection();
    TEST_ASSERT_TRUE(fabsf(held) > 0.0f);

    int raw_lost[8] = {0,0,0,0,0,0,0,0};        // linha perdida
    lf->update(raw_lost, l, r);
    TEST_ASSERT_TRUE(lf->isLineLost());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, held, lf->getLastCorrection());  // mantém
}
```

Registrar no `main()`:

```cpp
    RUN_TEST(test_line_follower_goes_straight_on_crossing);
    RUN_TEST(test_line_follower_holds_correction_when_line_lost);
```

- [ ] **Step 7: Rodar para confirmar que falha**

Run: `pio test -e native -f test_line_follower`
Expected: FAIL — `isCrossing()`/`isLineLost()` não existem no LineFollower e a correção não é sobrescrita.

- [ ] **Step 8: Adicionar acessores ao LineFollower.h**

Em `src/strategy/LineFollower.h`, após `float getLastCorrection() const { return _lastCorrection; }`:

```cpp
    /** Estado do último update(): linha perdida (todos sensores no preto). */
    bool isLineLost() const { return _cal.isLineLost(); }
    /** Estado do último update(): cruzamento (muitos sensores no branco). */
    bool isCrossing() const { return _cal.isCrossing(); }
```

- [ ] **Step 9: Aplicar o comportamento no LineFollower.cpp**

Em `src/strategy/LineFollower.cpp`, substituir o corpo de `update` (do cálculo de `correction` até o `_drive.compute`) por:

```cpp
    float correction;
    if (_cal.isLineLost()) {
        // Recuperação: mantém a última correção conhecida (segue virando
        // para o lado onde a linha estava). Não recalcula o PID.
        correction = _lastCorrection;
    } else {
        correction = -_pid.compute(0.0f, position, dtSec);
        if (_cal.isCrossing()) {
            correction = 0.0f;  // cruzamento → segue reto através da interseção
        }
        _lastCorrection = correction;
    }

    int baseSpeed = _speed.compute(std::abs(normErr));
    _drive.compute(correction, baseSpeed, leftPwm, rightPwm);
```

> Observação: `_cal.isLineLost()`/`_cal.isCrossing()` já refletem o `weightedPosition(...)` chamado logo acima, na mesma passagem.

- [ ] **Step 10: Rodar toda a suíte de controle e sensores**

Run: `pio test -e native -f test_line_follower -f test_calibration`
Expected: PASS (incluindo `test_line_lost_maintains_last_direction`, que continua verde: em linha perdida a correção de curva anterior é mantida → `lp > rp`).

- [ ] **Step 11: Commit**

```bash
git add src/sensors/Calibration.h src/sensors/Calibration.cpp src/strategy/LineFollower.h src/strategy/LineFollower.cpp test/test_calibration/test_calibration.cpp test/test_line_follower/test_line_follower.cpp
git commit -m "feat(strategy): detecção de cruzamento + hold em linha perdida (usa isLineLost)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 4: Integração no main.cpp — dt real, corridas, beep, telemetria

> `main.cpp` é firmware-only (`#ifndef NATIVE_BUILD`) e não tem teste unitário. Verificação: (a) `pio test -e native` continua verde (as interfaces batem), (b) `pio run -e esp32dev` compila quando o toolchain está disponível, (c) checagem de bancada.

**Files:**
- Modify: `src/main.cpp`

**Interfaces (consumidas das Tasks 1–3):**
- `velL.update(uint32_t dtUs)`, `velR.update(uint32_t dtUs)`
- `lineFollower.update(raw, l, r, &normErr, dtSec)`, `lineFollower.isLineLost()`, `lineFollower.isCrossing()`
- `cascade.compute(correctionRpm, base, rpmL, rpmR, l, r, dtSec)`

- [ ] **Step 1: Trocar o cálculo de dt (µs real) no estado RUNNING**

Em `src/main.cpp`, no `case RobotState::RUNNING`, substituir:

```cpp
            uint32_t dtMs = (g_prevLoopStart == 0) ? 2 : ((loopStart - g_prevLoopStart) / 1000);
            g_prevLoopStart = loopStart;

            // 1. Atualiza estimadores de velocidade com dt real do loop
            velL.update(dtMs);
            velR.update(dtMs);
```
por:

```cpp
            uint32_t dtUs = (g_prevLoopStart == 0) ? 0 : (loopStart - g_prevLoopStart);
            g_prevLoopStart = loopStart;
            // dtUs==0 no 1º tick pós-reset → usa período nominal p/ o dt dos PIDs.
            float dtSec = (dtUs == 0) ? (LOOP_PERIOD_US * 1e-6f) : (dtUs * 1e-6f);

            // 1. Atualiza estimadores de velocidade com dt REAL do loop (µs)
            velL.update(dtUs);
            velR.update(dtUs);
```

- [ ] **Step 2: Reestruturar a seção crítica p/ englobar o cascade (fecha corrida do retune)**

Substituir o bloco atual (do `taskENTER_CRITICAL(&lineFollowerMux);` da leitura de sensores até a chamada `cascade.compute(...)`) por:

```cpp
            // 3. Valida RPM antes de entrar na seção crítica (leitura só-Core0).
            float rpmLActual = velL.getRPM();
            float rpmRActual = velR.getRPM();
            if (!std::isfinite(rpmLActual)) rpmLActual = 0.0f;
            if (!std::isfinite(rpmRActual)) rpmRActual = 0.0f;

            int   discardL = 0;
            int   discardR = 0;
            float normErr  = 0.0f;
            int   leftPwm  = 0;
            int   rightPwm = 0;
            bool  lineLost = false;
            bool  crossing = false;

            // Seção crítica cobre PID externo + cascade + leitura de g_baseRpm.
            // Fecha a corrida com setInnerGains()/setMaxRpm()/g_baseRpm (Core 1),
            // que também rodam sob lineFollowerMux.
            taskENTER_CRITICAL(&lineFollowerMux);
            lineFollower.update(raw, discardL, discardR, &normErr, dtSec);
            float correctionPwm = lineFollower.getLastCorrection();
            lineLost = lineFollower.isLineLost();
            crossing = lineFollower.isCrossing();
            float correctionRpm =
                correctionPwm * (MAX_RPM_DEFAULT / static_cast<float>(PWM_MAX));
            cascade.compute(correctionRpm, g_baseRpm,
                            rpmLActual, rpmRActual,
                            leftPwm, rightPwm, dtSec);
            taskEXIT_CRITICAL(&lineFollowerMux);

            // 7. Aplica PWM nos motores (fora da seção crítica)
            motors.setSpeed(MotorId::A, leftPwm);
            motors.setSpeed(MotorId::B, rightPwm);
```

> Remover os trechos antigos que ficaram redundantes: a leitura/validação de `rpmLActual/rpmRActual` que estava **depois** do `taskEXIT_CRITICAL`, o cálculo antigo de `correctionRpm`, e a chamada antiga de `cascade.compute(...)` fora da seção crítica. O `int raw[SENSOR_COUNT]; sensors.readAll(raw);` continua **antes** da seção crítica (SPI não deve rodar sob portMUX).

- [ ] **Step 3: Registrar lineLost/crossing na telemetria**

Na struct `TelemetrySnapshot` (topo do arquivo), adicionar após `float sensorsPct[SENSOR_COUNT] = {};`:

```cpp
    bool     lineLost  = false;
    bool     crossing  = false;
```

No bloco de atualização do snapshot (dentro do `taskENTER_CRITICAL(&g_snapMux)`), adicionar antes do `taskEXIT_CRITICAL(&g_snapMux);`:

```cpp
                g_snap.lineLost = lineLost;
                g_snap.crossing = crossing;
```

- [ ] **Step 4: Substituir `beep()` bloqueante por buzzer não-bloqueante**

Remover a função `beep(...)` (linhas ~125-130) e adicionar, no lugar:

```cpp
// Buzzer não-bloqueante: beepStart() liga e agenda o desligamento; serviceBuzzer()
// (chamado nos dois loops) desliga quando vence o prazo. Evita busy-wait no loop
// de controle (Core 0) e na task de BLE (Core 1).
static volatile uint32_t g_buzzerOffMs = 0;

static void beepStart(uint32_t ms) {
    digitalWrite(PIN_BUZZER, HIGH);
    g_buzzerOffMs = millis() + ms;
    if (g_buzzerOffMs == 0) g_buzzerOffMs = 1;  // 0 é sentinela de "desligado"
}

static void serviceBuzzer() {
    uint32_t off = g_buzzerOffMs;
    if (off != 0 && static_cast<int32_t>(millis() - off) >= 0) {
        digitalWrite(PIN_BUZZER, LOW);
        g_buzzerOffMs = 0;
    }
}
```

Trocar TODAS as chamadas `beep(x)` por `beepStart(x)` em `main.cpp` (setup, taskComm e loop — busca simples por `beep(`).

Chamar `serviceBuzzer();` no início do `for(;;)` de `taskComm` (antes de `ble.update();`) e no início do `loop()` (primeira linha, antes do `switch`).

- [ ] **Step 5: Restringir comandos de calibração a estado não-RUNNING (fecha 2ª corrida)**

No `taskComm`, envolver o bloco `if (ble.hasNewCalibration()) { ... }` com uma checagem de estado:

```cpp
        if (ble.hasNewCalibration() && robotState.load() != RobotState::RUNNING) {
            // ... corpo existente do tratamento de calibração ...
        }
```

> Durante `RUNNING` o Core 0 chama `velL/velR.update()`; a calibração (Core 1) mexe em `_pprX4/_calibrating`. Restringir a calibração ao robô parado remove a corrida sem custo no caminho crítico. (A calibração de encoder é sempre feita na bancada, com o robô parado.)

- [ ] **Step 6: Verificar que a suíte nativa continua verde**

Run: `pio test -e native`
Expected: PASS — todas as suítes (as originais + as novas das Tasks 1–3).

- [ ] **Step 7: Compilar o firmware (se o toolchain esp32 estiver disponível)**

Run: `pio run -e esp32dev`
Expected: SUCCESS, sem warnings novos. (Se o toolchain não estiver instalado neste ambiente, registrar como pendência de bancada.)

- [ ] **Step 8: Commit**

```bash
git add src/main.cpp
git commit -m "fix(main): dt real (µs/s) na cadeia, fecha corridas retune/calibração, buzzer não-bloqueante

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Riscos e verificação de bancada (pós-plano)

- **Derivada com dt pequeno e variável (escolha do dono):** com o loop livre, `dtSec` pode ficar muito curto e amplificar ruído no termo D (`Kd=12` externo é dominante). Se em bancada o robô tremer/oscilar em reta, reduzir `Kd` externo via BLE **ou** aumentar `VELOCITY_MIN_WINDOW_US`/decimar o loop externo. Está documentado no guia de leitura.
- **`VELOCITY_MIN_WINDOW_US`:** default 4 ms. Se o RPM ler ruidoso a baixa velocidade, aumentar (janela maior = mais suave, mais atraso). Se responder devagar demais, diminuir.
- **Cruzamento — thresholds:** `CROSSING_MIN_ACTIVE=6`, `CROSSING_ACTIVE_LEVEL=700`. Validar contra a largura real da linha e o array; se cruzamentos não forem detectados, baixar `CROSSING_MIN_ACTIVE`; se falsos positivos em curva fechada, subir.
- **Surface BLE dos flags lineLost/crossing:** ficou **fora de escopo** (exigiria mudar a assinatura de `BLETuner::notifyTelemetry`). Já estão no `TelemetrySnapshot` — expor no dashboard é um follow-up pequeno.

## Self-Review (executado)

1. **Cobertura do escopo travado:** dt real em µs (Task 1+2+4) ✔; não-perda de pulsos (Task 1) ✔; corrida retune vs cascade (Task 4/Step 2) ✔; corrida calibração cross-core (Task 4/Step 5) ✔; beep não-bloqueante (Task 4/Step 4) ✔; cruzamento + `isLineLost` usado (Task 3) ✔.
2. **Placeholders:** nenhum — todo passo mostra o código/edição exata e o comando com resultado esperado.
3. **Consistência de tipos:** `update(uint32_t dtUs)`, `update(..., float dtSec)`, `compute(..., float dtSec)`, `isCrossing()/isLineLost()` usados de forma idêntica entre header, cpp, testes e `main.cpp`. Defaults (`dtSec=0.01f`/`0.002f`) preservam as suítes existentes; só `main.cpp` passa o dt real.
