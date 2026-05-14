# Fase 0 + Fase 1 — Fundações LFR Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove devcontainer, scaffold PlatformIO, and implement all pure-logic modules (PIDController, SpeedProfile, Calibration, DifferentialDrive) with full TDD — all tests passing in native environment, no hardware required.

**Architecture:** Bottom-up TDD: each module is tested in isolation on the host (native env) before integration. Tests define the interface; implementation follows. Modules are injected via constructor (no globals).

**Tech Stack:** C++17, PlatformIO 6.x, Unity Test Framework, `pio test -e native`

---

## File Map

| File | Action | Purpose |
|------|--------|---------|
| `.devcontainer/` | Delete | Remove devcontainer setup |
| `platformio.ini` | Create | Build environments (native + esp32dev) |
| `src/config.h` | Create | All constants from CLAUDE.md |
| `src/main.cpp` | Create | Empty Arduino skeleton |
| `src/control/PIDController.h` | Create | PID interface |
| `src/control/PIDController.cpp` | Create | PID implementation |
| `src/control/SpeedProfile.h` | Create | Speed profile interface |
| `src/control/SpeedProfile.cpp` | Create | Speed profile implementation |
| `src/sensors/Calibration.h` | Create | Calibration + position interface |
| `src/sensors/Calibration.cpp` | Create | Calibration + position implementation |
| `src/motors/DifferentialDrive.h` | Create | Differential drive interface |
| `src/motors/DifferentialDrive.cpp` | Create | Differential drive implementation |
| `test/test_smoke/test_smoke.cpp` | Create | Framework smoke test |
| `test/test_pid/test_pid.cpp` | Create | PID unit tests |
| `test/test_speed_profile/test_speed_profile.cpp` | Create | SpeedProfile unit tests |
| `test/test_calibration/test_calibration.cpp` | Create | Calibration unit tests |
| `test/test_motors/test_differential.cpp` | Create | DifferentialDrive unit tests |
| `.gitignore` | Create | Ignore .pio/, build artifacts |

---

## Task 0: Limpar devcontainer e verificar ambiente

**Files:**
- Delete: `.devcontainer/`

- [ ] **Step 1: Remover a pasta devcontainer**

```bash
rm -rf .devcontainer
```

- [ ] **Step 2: Verificar PlatformIO disponível**

```bash
pio --version
```

Expected: `PlatformIO Core, version 6.x.x`

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "chore: remove devcontainer — local PlatformIO setup"
```

---

## Task 1: Scaffold PlatformIO

**Files:**
- Create: `platformio.ini`
- Create: `src/config.h`
- Create: `src/main.cpp`
- Create: `.gitignore`
- Create: `test/test_smoke/test_smoke.cpp`
- Create (dirs): `src/sensors/`, `src/control/`, `src/motors/`, `src/strategy/`, `src/comm/`, `examples/`

- [ ] **Step 1: Criar platformio.ini**

```ini
[env:native]
platform = native
build_flags =
    -Wall
    -Wextra
    -std=gnu++17
    -DUNITY_INCLUDE_FLOAT
lib_deps =
    throwtheswitch/Unity @ ^2.5.2
test_build_src = true

[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
build_flags =
    -Wall
    -Wextra
    -std=gnu++17
    -DUNITY_INCLUDE_FLOAT
lib_deps =
    throwtheswitch/Unity @ ^2.5.2
test_build_src = true
```

- [ ] **Step 2: Criar src/config.h**

```cpp
#pragma once

// PID — valores iniciais, ajustáveis via BLE
#define KP_DEFAULT          3.0f
#define KI_DEFAULT          0.0f
#define KD_DEFAULT          12.0f

// Velocidade (PWM 0–255)
#define BASE_SPEED_DEFAULT  160
#define MAX_SPEED           230
#define MIN_SPEED           60
#define ERROR_THRESHOLD     0.6f    // normalizado 0..1; acima reduz velocidade

// Sensor array
#define SENSOR_COUNT        8
#define SENSOR_SPACING_MM   10
#define SENSOR_HEIGHT_MM    4
#define LOOKAHEAD_MM        70

// Posição: escala -4000..+4000 (sensor mais à esq = -3500, mais à dir = +3500)
#define POSITION_MAX        4000
#define POSITION_MIN        (-4000)
```

- [ ] **Step 3: Criar src/main.cpp**

```cpp
#pragma once
#include <Arduino.h>
#include "config.h"

void setup() {}
void loop() {}
```

- [ ] **Step 4: Criar .gitignore**

```
.pio/
.vscode/
__pycache__/
*.pyc
```

- [ ] **Step 5: Criar diretórios de módulos**

```bash
mkdir -p src/sensors src/control src/motors src/strategy src/comm examples
mkdir -p test/test_smoke test/test_pid test/test_speed_profile test/test_calibration test/test_motors test/test_line_follower
touch src/sensors/.gitkeep src/strategy/.gitkeep src/comm/.gitkeep examples/.gitkeep
```

- [ ] **Step 6: Criar test/test_smoke/test_smoke.cpp**

```cpp
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

void test_framework_works(void) {
    TEST_ASSERT_TRUE(1);
    TEST_ASSERT_EQUAL_INT(2, 1 + 1);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_framework_works);
    return UNITY_END();
}
```

- [ ] **Step 7: Verificar compilação nativa**

```bash
pio run -e native
```

Expected: `SUCCESS` (pode demorar na primeira vez — baixa dependências)

- [ ] **Step 8: Rodar smoke test**

```bash
pio test -e native -f test_smoke
```

Expected:
```
test/test_smoke/test_smoke.cpp:14:test_framework_works:PASS
-----------------------
1 Tests 0 Failures 0 Ignored
OK
```

- [ ] **Step 9: Commit**

```bash
git add platformio.ini src/config.h src/main.cpp .gitignore test/test_smoke/
git add src/sensors/ src/control/ src/motors/ src/strategy/ src/comm/ examples/
git commit -m "feat: scaffold PlatformIO — native + esp32dev envs, smoke test passing"
```

---

## Task 2: PIDController — RED

**Files:**
- Create: `test/test_pid/test_pid.cpp`

- [ ] **Step 1: Criar test/test_pid/test_pid.cpp**

```cpp
#include <unity.h>
#include "control/PIDController.h"

void setUp(void) {}
void tearDown(void) {}

void test_pid_zero_error_zero_output(void) {
    PIDController pid(3.0f, 0.0f, 0.0f, -255.0f, 255.0f);
    float output = pid.compute(0.0f, 0.0f, 0.01f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, output);
}

void test_pid_proportional_scales_linearly(void) {
    PIDController pid(2.0f, 0.0f, 0.0f, -10000.0f, 10000.0f);
    float out1 = pid.compute(0.0f, -1.0f, 0.01f);  // error = +1
    pid.reset();
    float out2 = pid.compute(0.0f, -2.0f, 0.01f);  // error = +2
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.0f * out1, out2);
}

void test_pid_derivative_on_measurement_not_error(void) {
    // Derivada no PROCESSO: mudar setpoint não gera spike
    PIDController pid(0.0f, 0.0f, 100.0f, -10000.0f, 10000.0f);
    pid.compute(0.0f, 0.0f, 0.01f);  // seed: measurement=0, prev=0
    // Setpoint muda bruscamente; measurement permanece 0
    float output = pid.compute(1.0f, 0.0f, 0.01f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, output);  // sem spike
}

void test_pid_derivative_reduces_overshoot(void) {
    // Com D alto, overshooting deve ser menor que só P
    PIDController pidP(3.0f, 0.0f, 0.0f, -1000.0f, 1000.0f);
    PIDController pidPD(3.0f, 0.0f, 20.0f, -1000.0f, 1000.0f);
    float measurement = -1.0f;  // setpoint=0, erro inicial=1
    float outP = pidP.compute(0.0f, measurement, 0.01f);
    float outPD = pidPD.compute(0.0f, measurement, 0.01f);
    // Na primeira iteração (sem histórico), D=0 → ambos iguais
    TEST_ASSERT_FLOAT_WITHIN(0.001f, outP, outPD);
    // Segunda iteração: measurement se move em direção ao setpoint
    measurement = -0.5f;
    outP = pidP.compute(0.0f, measurement, 0.01f);
    outPD = pidPD.compute(0.0f, measurement, 0.01f);
    // PD deve produzir output MENOR (freia antes do overshoot)
    TEST_ASSERT_TRUE(outPD < outP);
}

void test_pid_integral_accumulates_over_time(void) {
    PIDController pid(0.0f, 10.0f, 0.0f, -10000.0f, 10000.0f);
    // 3 chamadas com erro=1, dt=0.1 → integral = 0.3 → output = 3.0
    pid.compute(1.0f, 0.0f, 0.1f);
    pid.compute(1.0f, 0.0f, 0.1f);
    float output = pid.compute(1.0f, 0.0f, 0.1f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.0f, output);
}

void test_pid_integral_windup_clamp(void) {
    PIDController pid(0.0f, 1.0f, 0.0f, -255.0f, 255.0f);
    // Acumular por 1000 steps com erro alto — integral deve ser limitada pelo clamping
    for (int i = 0; i < 1000; i++) {
        pid.compute(10000.0f, 0.0f, 0.01f);
    }
    float output = pid.compute(10000.0f, 0.0f, 0.01f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 255.0f, output);  // clamped, não explodiu
}

void test_pid_output_clamped(void) {
    PIDController pid(1000.0f, 0.0f, 0.0f, -255.0f, 255.0f);
    float output = pid.compute(0.0f, -1.0f, 0.01f);  // P = 1000 → clampado
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 255.0f, output);
    pid.reset();
    output = pid.compute(0.0f, 1.0f, 0.01f);  // P = -1000 → clampado
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -255.0f, output);
}

void test_pid_reset_clears_state(void) {
    PIDController pid(0.0f, 10.0f, 0.0f, -10000.0f, 10000.0f);
    pid.compute(1.0f, 0.0f, 0.1f);  // acumula integral
    pid.compute(1.0f, 0.0f, 0.1f);
    pid.reset();
    float output = pid.compute(1.0f, 0.0f, 0.1f);  // apenas 1 step após reset
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, output);
}

void test_pid_symmetry(void) {
    PIDController pid(3.0f, 0.0f, 0.0f, -10000.0f, 10000.0f);
    float pos = pid.compute(0.0f, -1.0f, 0.01f);  // erro = +1
    pid.reset();
    float neg = pid.compute(0.0f, 1.0f, 0.01f);   // erro = -1
    TEST_ASSERT_FLOAT_WITHIN(0.001f, pos, -neg);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pid_zero_error_zero_output);
    RUN_TEST(test_pid_proportional_scales_linearly);
    RUN_TEST(test_pid_derivative_on_measurement_not_error);
    RUN_TEST(test_pid_derivative_reduces_overshoot);
    RUN_TEST(test_pid_integral_accumulates_over_time);
    RUN_TEST(test_pid_integral_windup_clamp);
    RUN_TEST(test_pid_output_clamped);
    RUN_TEST(test_pid_reset_clears_state);
    RUN_TEST(test_pid_symmetry);
    return UNITY_END();
}
```

- [ ] **Step 2: Confirmar RED — testes devem FALHAR**

```bash
pio test -e native -f test_pid
```

Expected: `error: 'PIDController' was not declared` (arquivo não existe ainda)

---

## Task 3: PIDController — GREEN

**Files:**
- Create: `src/control/PIDController.h`
- Create: `src/control/PIDController.cpp`

- [ ] **Step 1: Criar src/control/PIDController.h**

```cpp
#pragma once

/**
 * Generic PID controller with derivative-on-measurement and anti-windup.
 * All state is encapsulated — no globals, safe to instantiate multiple times.
 */
class PIDController {
public:
    /**
     * @param kp         Proportional gain
     * @param ki         Integral gain
     * @param kd         Derivative gain (applied to measurement, not error)
     * @param outMin     Minimum output value (clamping)
     * @param outMax     Maximum output value (clamping)
     */
    PIDController(float kp, float ki, float kd, float outMin, float outMax);

    /**
     * Compute one PID step.
     * @param setpoint    Desired value
     * @param measurement Current measured value
     * @param dt          Time delta in seconds since last call
     * @return            Clamped control output
     */
    float compute(float setpoint, float measurement, float dt);

    /** Reset integral and derivative history. Call before re-use after pause. */
    void reset();

    /** Update gains at runtime (e.g., from BLE tuner). */
    void setGains(float kp, float ki, float kd);

private:
    float _kp, _ki, _kd;
    float _outMin, _outMax;
    float _integral;
    float _prevMeasurement;
    bool  _firstRun;
};
```

- [ ] **Step 2: Criar src/control/PIDController.cpp**

```cpp
#include "PIDController.h"
#include <algorithm>

PIDController::PIDController(float kp, float ki, float kd, float outMin, float outMax)
    : _kp(kp), _ki(ki), _kd(kd),
      _outMin(outMin), _outMax(outMax),
      _integral(0.0f), _prevMeasurement(0.0f), _firstRun(true) {}

float PIDController::compute(float setpoint, float measurement, float dt) {
    float error = setpoint - measurement;

    // Derivada no PROCESSO (não no erro) — evita derivative kick ao mudar setpoint
    float derivative = 0.0f;
    if (!_firstRun && dt > 0.0f) {
        derivative = -(measurement - _prevMeasurement) / dt;
    }
    _firstRun = false;
    _prevMeasurement = measurement;

    float p = _kp * error;
    float d = _kd * derivative;

    // Acumular integral antes do clamping para anti-windup condicional
    _integral += _ki * error * dt;

    float output = p + _integral + d;

    // Clamp output
    float clamped = std::max(_outMin, std::min(_outMax, output));

    // Anti-windup: se o output foi saturado, desfaz a acumulação do step atual
    if (clamped != output) {
        _integral -= _ki * error * dt;
    }

    return clamped;
}

void PIDController::reset() {
    _integral = 0.0f;
    _prevMeasurement = 0.0f;
    _firstRun = true;
}

void PIDController::setGains(float kp, float ki, float kd) {
    _kp = kp;
    _ki = ki;
    _kd = kd;
}
```

- [ ] **Step 3: Rodar testes — todos devem PASSAR**

```bash
pio test -e native -f test_pid
```

Expected:
```
test_pid_zero_error_zero_output:PASS
test_pid_proportional_scales_linearly:PASS
test_pid_derivative_on_measurement_not_error:PASS
test_pid_derivative_reduces_overshoot:PASS
test_pid_integral_accumulates_over_time:PASS
test_pid_integral_windup_clamp:PASS
test_pid_output_clamped:PASS
test_pid_reset_clears_state:PASS
test_pid_symmetry:PASS
9 Tests 0 Failures 0 Ignored
OK
```

- [ ] **Step 4: Commit**

```bash
git add test/test_pid/ src/control/PIDController.h src/control/PIDController.cpp
git commit -m "feat: PIDController TDD — 9 testes passando (derivada no processo, anti-windup)"
```

---

## Task 4: SpeedProfile — RED

**Files:**
- Create: `test/test_speed_profile/test_speed_profile.cpp`

- [ ] **Step 1: Criar test/test_speed_profile/test_speed_profile.cpp**

```cpp
#include <unity.h>
#include "control/SpeedProfile.h"

void setUp(void) {}
void tearDown(void) {}

void test_zero_error_max_speed(void) {
    SpeedProfile sp(230.0f, 60.0f, 0.6f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 230.0f, sp.compute(0.0f));
}

void test_small_error_high_speed(void) {
    SpeedProfile sp(230.0f, 60.0f, 0.6f);
    float speed = sp.compute(0.2f);  // 33% do threshold
    TEST_ASSERT_TRUE(speed > 150.0f);
    TEST_ASSERT_TRUE(speed <= 230.0f);
}

void test_large_error_min_speed(void) {
    SpeedProfile sp(230.0f, 60.0f, 0.6f);
    float speed = sp.compute(1.0f);  // acima do threshold
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 60.0f, speed);
}

void test_linear_interpolation(void) {
    SpeedProfile sp(200.0f, 100.0f, 1.0f);
    // erro=0.5 → 50% do threshold → metade do range → 150
    float speed = sp.compute(0.5f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 150.0f, speed);
}

void test_symmetry_positive_negative(void) {
    SpeedProfile sp(230.0f, 60.0f, 0.6f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, sp.compute(0.3f), sp.compute(-0.3f));
}

void test_clamps_to_bounds(void) {
    SpeedProfile sp(230.0f, 60.0f, 0.6f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 60.0f, sp.compute(999.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 230.0f, sp.compute(-999.0f));
}

void test_params_updateable_runtime(void) {
    SpeedProfile sp(230.0f, 60.0f, 0.6f);
    sp.setParams(180.0f, 80.0f, 0.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 180.0f, sp.compute(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 80.0f, sp.compute(1.0f));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_zero_error_max_speed);
    RUN_TEST(test_small_error_high_speed);
    RUN_TEST(test_large_error_min_speed);
    RUN_TEST(test_linear_interpolation);
    RUN_TEST(test_symmetry_positive_negative);
    RUN_TEST(test_clamps_to_bounds);
    RUN_TEST(test_params_updateable_runtime);
    return UNITY_END();
}
```

- [ ] **Step 2: Confirmar RED**

```bash
pio test -e native -f test_speed_profile
```

Expected: `error: 'SpeedProfile' was not declared`

---

## Task 5: SpeedProfile — GREEN

**Files:**
- Create: `src/control/SpeedProfile.h`
- Create: `src/control/SpeedProfile.cpp`

- [ ] **Step 1: Criar src/control/SpeedProfile.h**

```cpp
#pragma once
#include <cmath>

/**
 * Maps absolute PID error to base speed.
 * Error 0 → maxSpeed; error >= threshold → minSpeed; linear between.
 */
class SpeedProfile {
public:
    /**
     * @param maxSpeed   Speed on a straight line (error ≈ 0)
     * @param minSpeed   Speed in a sharp curve (error >= threshold)
     * @param threshold  Normalized error at which minSpeed is reached
     */
    SpeedProfile(float maxSpeed, float minSpeed, float threshold);

    /** @param absError  Absolute error value (sign ignored) */
    float compute(float absError) const;

    /** Update parameters at runtime (BLE tuner). */
    void setParams(float maxSpeed, float minSpeed, float threshold);

private:
    float _maxSpeed;
    float _minSpeed;
    float _threshold;
};
```

- [ ] **Step 2: Criar src/control/SpeedProfile.cpp**

```cpp
#include "SpeedProfile.h"
#include <algorithm>

SpeedProfile::SpeedProfile(float maxSpeed, float minSpeed, float threshold)
    : _maxSpeed(maxSpeed), _minSpeed(minSpeed), _threshold(threshold) {}

float SpeedProfile::compute(float absError) const {
    float e = std::abs(absError);
    if (_threshold <= 0.0f) return _minSpeed;

    float t = std::min(e / _threshold, 1.0f);
    return _maxSpeed + (_minSpeed - _maxSpeed) * t;
}

void SpeedProfile::setParams(float maxSpeed, float minSpeed, float threshold) {
    _maxSpeed = maxSpeed;
    _minSpeed = minSpeed;
    _threshold = threshold;
}
```

- [ ] **Step 3: Rodar testes**

```bash
pio test -e native -f test_speed_profile
```

Expected: `7 Tests 0 Failures 0 Ignored  OK`

- [ ] **Step 4: Confirmar smoke + PID ainda passam**

```bash
pio test -e native -f test_smoke && pio test -e native -f test_pid
```

Expected: ambos `OK`

- [ ] **Step 5: Commit**

```bash
git add test/test_speed_profile/ src/control/SpeedProfile.h src/control/SpeedProfile.cpp
git commit -m "feat: SpeedProfile TDD — 7 testes passando (interpolação linear, clamp)"
```

---

## Task 6: Calibration — RED

**Files:**
- Create: `test/test_calibration/test_calibration.cpp`

- [ ] **Step 1: Criar test/test_calibration/test_calibration.cpp**

```cpp
#include <unity.h>
#include <cstdint>
#include "sensors/Calibration.h"

void setUp(void) {}
void tearDown(void) {}

void test_initial_min_max_inverted(void) {
    // Antes de qualquer update, min deve ser alto e max baixo (para capturar qualquer valor)
    Calibration cal(8);
    // Qualquer valor real deve ficar entre os limites iniciais
    uint16_t raw[8] = {500, 500, 500, 500, 500, 500, 500, 500};
    cal.update(raw, 8);
    // Após um update, deve normalizar sem crash
    uint16_t norm = cal.normalize(0, 500);
    TEST_ASSERT_TRUE(norm <= 1000);
}

void test_update_captures_extremes(void) {
    Calibration cal(8);
    uint16_t low[8]  = {100, 100, 100, 100, 100, 100, 100, 100};
    uint16_t high[8] = {900, 900, 900, 900, 900, 900, 900, 900};
    cal.update(low, 8);
    cal.update(high, 8);
    // 100 deve normalizar para 0, 900 para 1000
    TEST_ASSERT_EQUAL_UINT16(0,    cal.normalize(0, 100));
    TEST_ASSERT_EQUAL_UINT16(1000, cal.normalize(0, 900));
}

void test_normalize_0_to_1000(void) {
    Calibration cal(8);
    uint16_t low[8]  = {0,    0,    0,    0,    0,    0,    0,    0};
    uint16_t high[8] = {1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023};
    cal.update(low, 8);
    cal.update(high, 8);
    TEST_ASSERT_EQUAL_UINT16(0,    cal.normalize(0, 0));
    TEST_ASSERT_EQUAL_UINT16(1000, cal.normalize(0, 1023));
    TEST_ASSERT_UINT16_WITHIN(10, 500, cal.normalize(0, 512));
}

void test_normalize_clamps_outliers(void) {
    Calibration cal(8);
    uint16_t lo[8] = {200, 200, 200, 200, 200, 200, 200, 200};
    uint16_t hi[8] = {800, 800, 800, 800, 800, 800, 800, 800};
    cal.update(lo, 8);
    cal.update(hi, 8);
    TEST_ASSERT_EQUAL_UINT16(0,    cal.normalize(0, 0));    // abaixo do min
    TEST_ASSERT_EQUAL_UINT16(1000, cal.normalize(0, 1023)); // acima do max
}

void test_normalize_handles_zero_range(void) {
    Calibration cal(8);
    uint16_t same[8] = {500, 500, 500, 500, 500, 500, 500, 500};
    cal.update(same, 8);
    // min == max → sem divisão por zero; retorna 0
    uint16_t result = cal.normalize(0, 500);
    TEST_ASSERT_TRUE(result <= 1000);
}

void test_reset_clears(void) {
    Calibration cal(8);
    uint16_t hi[8] = {900, 900, 900, 900, 900, 900, 900, 900};
    cal.update(hi, 8);
    cal.reset();
    uint16_t lo[8] = {100, 100, 100, 100, 100, 100, 100, 100};
    cal.update(lo, 8);
    // Após reset + update com só 100, normalize(100) deve ser 0 ou indeterminado mas sem crash
    uint16_t norm = cal.normalize(0, 100);
    TEST_ASSERT_TRUE(norm <= 1000);
}

void test_weighted_position_center(void) {
    Calibration cal(8);
    // Calibrar com range 0–1000
    uint16_t lo[8]  = {0, 0, 0, 0, 0, 0, 0, 0};
    uint16_t hi[8]  = {1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000};
    cal.update(lo, 8);
    cal.update(hi, 8);
    // Linha no centro: sensores 3 e 4 (índices 0-based) bem acesos, restantes apagados
    // Pesos: s0=-3500, s1=-2500, s2=-1500, s3=-500, s4=+500, s5=+1500, s6=+2500, s7=+3500
    uint16_t raw[8] = {0, 0, 0, 800, 800, 0, 0, 0};
    int32_t pos = cal.weightedPosition(raw, 8);
    TEST_ASSERT_INT32_WITHIN(200, 0, pos);  // ≈ 0 (centro)
}

void test_weighted_position_left(void) {
    Calibration cal(8);
    uint16_t lo[8]  = {0, 0, 0, 0, 0, 0, 0, 0};
    uint16_t hi[8]  = {1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000};
    cal.update(lo, 8);
    cal.update(hi, 8);
    uint16_t raw[8] = {800, 800, 0, 0, 0, 0, 0, 0};  // linha à esquerda
    int32_t pos = cal.weightedPosition(raw, 8);
    TEST_ASSERT_TRUE(pos < -2000);
}

void test_weighted_position_right(void) {
    Calibration cal(8);
    uint16_t lo[8]  = {0, 0, 0, 0, 0, 0, 0, 0};
    uint16_t hi[8]  = {1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000};
    cal.update(lo, 8);
    cal.update(hi, 8);
    uint16_t raw[8] = {0, 0, 0, 0, 0, 0, 800, 800};  // linha à direita
    int32_t pos = cal.weightedPosition(raw, 8);
    TEST_ASSERT_TRUE(pos > 2000);
}

void test_line_lost_returns_last_known(void) {
    Calibration cal(8);
    uint16_t lo[8]  = {0, 0, 0, 0, 0, 0, 0, 0};
    uint16_t hi[8]  = {1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000};
    cal.update(lo, 8);
    cal.update(hi, 8);
    // Primeira leitura válida: linha à esquerda → posição negativa
    uint16_t withLine[8] = {800, 800, 0, 0, 0, 0, 0, 0};
    int32_t lastPos = cal.weightedPosition(withLine, 8);
    // Segunda leitura: todos zeros (linha perdida)
    uint16_t noLine[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int32_t lostPos = cal.weightedPosition(noLine, 8);
    TEST_ASSERT_EQUAL_INT32(lastPos, lostPos);  // mantém última posição conhecida
    TEST_ASSERT_TRUE(cal.isLineLost());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_initial_min_max_inverted);
    RUN_TEST(test_update_captures_extremes);
    RUN_TEST(test_normalize_0_to_1000);
    RUN_TEST(test_normalize_clamps_outliers);
    RUN_TEST(test_normalize_handles_zero_range);
    RUN_TEST(test_reset_clears);
    RUN_TEST(test_weighted_position_center);
    RUN_TEST(test_weighted_position_left);
    RUN_TEST(test_weighted_position_right);
    RUN_TEST(test_line_lost_returns_last_known);
    return UNITY_END();
}
```

- [ ] **Step 2: Confirmar RED**

```bash
pio test -e native -f test_calibration
```

Expected: `error: 'Calibration' was not declared`

---

## Task 7: Calibration — GREEN

**Files:**
- Create: `src/sensors/Calibration.h`
- Create: `src/sensors/Calibration.cpp`

- [ ] **Step 1: Criar src/sensors/Calibration.h**

```cpp
#pragma once
#include <cstdint>

/**
 * Per-sensor min/max calibration and weighted-position calculator.
 * Stores last known position when line is lost.
 */
class Calibration {
public:
    explicit Calibration(uint8_t sensorCount);

    /** Feed raw ADC readings to expand per-sensor min/max. */
    void update(const uint16_t* raw, uint8_t count);

    /**
     * Normalize a raw ADC value to 0..1000 using per-sensor calibration.
     * Clamps outliers. Returns 0 if range is zero.
     */
    uint16_t normalize(uint8_t idx, uint16_t raw) const;

    /**
     * Compute weighted position from raw readings.
     * Weights: sensor i → (i - (count-1)/2) * 1000
     * Range: -(count-1)/2*1000 .. +(count-1)/2*1000  (≈ -3500..+3500 for 8 sensors)
     * Returns last known position if all normalized values are below threshold.
     */
    int32_t weightedPosition(const uint16_t* raw, uint8_t count);

    /** True if the last weightedPosition() call detected a lost line. */
    bool isLineLost() const;

    /** Reset calibration min/max to initial state. */
    void reset();

private:
    static constexpr uint8_t  MAX_SENSORS    = 8;
    static constexpr uint16_t LINE_THRESHOLD = 50;  // normalized; below = no line

    uint8_t  _count;
    uint16_t _min[MAX_SENSORS];
    uint16_t _max[MAX_SENSORS];
    int32_t  _lastPosition;
    bool     _lineLost;
};
```

- [ ] **Step 2: Criar src/sensors/Calibration.cpp**

```cpp
#include "Calibration.h"
#include <algorithm>
#include <cstring>

Calibration::Calibration(uint8_t sensorCount)
    : _count(sensorCount), _lastPosition(0), _lineLost(false) {
    reset();
}

void Calibration::update(const uint16_t* raw, uint8_t count) {
    uint8_t n = std::min(count, _count);
    for (uint8_t i = 0; i < n; i++) {
        if (raw[i] < _min[i]) _min[i] = raw[i];
        if (raw[i] > _max[i]) _max[i] = raw[i];
    }
}

uint16_t Calibration::normalize(uint8_t idx, uint16_t raw) const {
    if (idx >= _count) return 0;
    uint16_t lo = _min[idx];
    uint16_t hi = _max[idx];
    if (hi <= lo) return 0;
    if (raw <= lo) return 0;
    if (raw >= hi) return 1000;
    return static_cast<uint16_t>((static_cast<uint32_t>(raw - lo) * 1000) / (hi - lo));
}

int32_t Calibration::weightedPosition(const uint16_t* raw, uint8_t count) {
    uint8_t n = std::min(count, _count);
    int32_t  weightedSum = 0;
    uint32_t totalWeight = 0;

    for (uint8_t i = 0; i < n; i++) {
        uint16_t norm = normalize(i, raw[i]);
        if (norm < LINE_THRESHOLD) continue;
        // Peso do sensor i: posição relativa ao centro × 1000
        // Para n=8: -3500, -2500, -1500, -500, +500, +1500, +2500, +3500
        int32_t sensorPos = (static_cast<int32_t>(i) * 1000) - ((n - 1) * 500);
        weightedSum += sensorPos * norm;
        totalWeight += norm;
    }

    if (totalWeight == 0) {
        _lineLost = true;
        return _lastPosition;
    }

    _lineLost = false;
    _lastPosition = weightedSum / static_cast<int32_t>(totalWeight);
    return _lastPosition;
}

bool Calibration::isLineLost() const {
    return _lineLost;
}

void Calibration::reset() {
    for (uint8_t i = 0; i < MAX_SENSORS; i++) {
        _min[i] = 65535;
        _max[i] = 0;
    }
    _lastPosition = 0;
    _lineLost     = false;
}
```

- [ ] **Step 3: Rodar testes**

```bash
pio test -e native -f test_calibration
```

Expected: `10 Tests 0 Failures 0 Ignored  OK`

- [ ] **Step 4: Regressão — todos os testes anteriores**

```bash
pio test -e native -f test_smoke && pio test -e native -f test_pid && pio test -e native -f test_speed_profile
```

Expected: todos `OK`

- [ ] **Step 5: Commit**

```bash
git add test/test_calibration/ src/sensors/Calibration.h src/sensors/Calibration.cpp
git commit -m "feat: Calibration TDD — 10 testes passando (normalização, posição ponderada, line-lost)"
```

---

## Task 8: DifferentialDrive — RED

**Files:**
- Create: `test/test_motors/test_differential.cpp`

- [ ] **Step 1: Criar test/test_motors/test_differential.cpp**

```cpp
#include <unity.h>
#include "motors/DifferentialDrive.h"

void setUp(void) {}
void tearDown(void) {}

void test_zero_correction_equal_speeds(void) {
    DifferentialDrive dd;
    WheelSpeeds ws = dd.compute(160, 0.0f);
    TEST_ASSERT_EQUAL_INT16(160, ws.left);
    TEST_ASSERT_EQUAL_INT16(160, ws.right);
}

void test_positive_correction_turns_right(void) {
    // Correção positiva → robô vira à direita → motor esq mais rápido, dir mais lento
    DifferentialDrive dd;
    WheelSpeeds ws = dd.compute(160, 50.0f);
    TEST_ASSERT_TRUE(ws.left > ws.right);
}

void test_negative_correction_turns_left(void) {
    DifferentialDrive dd;
    WheelSpeeds ws = dd.compute(160, -50.0f);
    TEST_ASSERT_TRUE(ws.right > ws.left);
}

void test_clamps_to_pwm_range(void) {
    DifferentialDrive dd;
    WheelSpeeds ws = dd.compute(160, 10000.0f);  // correção absurda
    TEST_ASSERT_TRUE(ws.left  <=  255);
    TEST_ASSERT_TRUE(ws.right >= -255);
    ws = dd.compute(160, -10000.0f);
    TEST_ASSERT_TRUE(ws.right <=  255);
    TEST_ASSERT_TRUE(ws.left  >= -255);
}

void test_allows_reverse_in_tight_curve(void) {
    // Curva fechada: um motor pode ir a ré (valor negativo)
    DifferentialDrive dd;
    WheelSpeeds ws = dd.compute(60, 200.0f);  // base baixa, correção alta
    TEST_ASSERT_TRUE(ws.right < 0);
}

void test_zero_base_spin_in_place(void) {
    DifferentialDrive dd;
    WheelSpeeds ws = dd.compute(0, 100.0f);
    TEST_ASSERT_TRUE(ws.left  > 0);
    TEST_ASSERT_TRUE(ws.right < 0);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_zero_correction_equal_speeds);
    RUN_TEST(test_positive_correction_turns_right);
    RUN_TEST(test_negative_correction_turns_left);
    RUN_TEST(test_clamps_to_pwm_range);
    RUN_TEST(test_allows_reverse_in_tight_curve);
    RUN_TEST(test_zero_base_spin_in_place);
    return UNITY_END();
}
```

- [ ] **Step 2: Confirmar RED**

```bash
pio test -e native -f test_motors
```

Expected: `error: 'DifferentialDrive' was not declared`

---

## Task 9: DifferentialDrive — GREEN

**Files:**
- Create: `src/motors/DifferentialDrive.h`
- Create: `src/motors/DifferentialDrive.cpp`

- [ ] **Step 1: Criar src/motors/DifferentialDrive.h**

```cpp
#pragma once
#include <cstdint>

/** Output of a differential drive computation. */
struct WheelSpeeds {
    int16_t left;   // -255..+255, negativo = ré
    int16_t right;  // -255..+255, negativo = ré
};

/**
 * Converts base speed + PID correction into left/right PWM values.
 * Convention: positive correction → turn right (left wheel faster).
 */
class DifferentialDrive {
public:
    DifferentialDrive();

    /**
     * @param baseSpeed  Target speed for both wheels when correction=0 (0..255)
     * @param correction PID output; positive = turn right, negative = turn left
     * @return           Clamped wheel speeds
     */
    WheelSpeeds compute(int16_t baseSpeed, float correction) const;
};
```

- [ ] **Step 2: Criar src/motors/DifferentialDrive.cpp**

```cpp
#include "DifferentialDrive.h"
#include <algorithm>

DifferentialDrive::DifferentialDrive() {}

WheelSpeeds DifferentialDrive::compute(int16_t baseSpeed, float correction) const {
    WheelSpeeds ws;
    ws.left  = static_cast<int16_t>(baseSpeed + correction);
    ws.right = static_cast<int16_t>(baseSpeed - correction);
    ws.left  = static_cast<int16_t>(std::max(-255, std::min(255, (int)ws.left)));
    ws.right = static_cast<int16_t>(std::max(-255, std::min(255, (int)ws.right)));
    return ws;
}
```

- [ ] **Step 3: Rodar testes**

```bash
pio test -e native -f test_motors
```

Expected: `6 Tests 0 Failures 0 Ignored  OK`

- [ ] **Step 4: Regressão total**

```bash
pio test -e native
```

Expected: todos os suites passam — smoke, pid, speed_profile, calibration, motors.

- [ ] **Step 5: Commit final**

```bash
git add test/test_motors/ src/motors/DifferentialDrive.h src/motors/DifferentialDrive.cpp
git commit -m "feat: DifferentialDrive TDD — 6 testes passando (clamp, ré em curva fechada)"
```

---

## Self-Review

**Spec coverage:**
- [x] Remover devcontainer → Task 0
- [x] Scaffold PlatformIO (platformio.ini, config.h, main.cpp, estrutura, .gitignore) → Task 1
- [x] Smoke test → Task 1 Step 6-8
- [x] PIDController TDD (9 testes) → Tasks 2-3
- [x] SpeedProfile TDD (7 testes) → Tasks 4-5
- [x] Calibration TDD (10 testes) → Tasks 6-7
- [x] DifferentialDrive TDD (6 testes) → Tasks 8-9
- [x] Regressão total ao final → Task 9 Step 4

**Placeholder scan:** Nenhum TBD ou TODO encontrado. Todo step tem código completo.

**Type consistency:**
- `PIDController(kp, ki, kd, outMin, outMax)` — consistente em testes e header
- `SpeedProfile(maxSpeed, minSpeed, threshold)` — consistente
- `Calibration(sensorCount)` — consistente; `normalize(idx, raw)`, `weightedPosition(raw, count)` — consistente
- `DifferentialDrive::compute(baseSpeed, correction)` → `WheelSpeeds{left, right}` — consistente em testes e header
