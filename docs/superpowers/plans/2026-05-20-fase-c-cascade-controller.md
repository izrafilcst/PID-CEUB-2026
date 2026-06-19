# Fase C — CascadeController Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Substituir o controle open-loop por uma arquitetura **cascade PID** — o PID externo (posição da linha) gera uma correção em RPM, dois PIDs internos (um por motor) fecham a malha de velocidade com a leitura dos `VelocityEstimator`s. Resultado: robô anda na velocidade comandada independente de carga, atrito ou voltagem.

**Architecture:** `CascadeController` recebe `correction` (saída do PID externo) e `targetBaseRpm` (alvo de velocidade do `SpeedProfile` traduzido para RPM), além das medições atuais `actualRpmL/R` (cliente injeta — desacopla da classe `VelocityEstimator` para facilitar testes). Tradução: `setpointL = base + correction`, `setpointR = base − correction`. Cada PID interno recebe `setpoint − actualRpm` como erro e gera PWM. Saturação: setpoints clamped em `[−maxRpm, +maxRpm]`; PWMs clamped em `[−maxPwm, +maxPwm]`. `main.cpp` é atualizado para instanciar 2× `Encoder`, 2× `VelocityEstimator`, 2× `PIDController` interno + 1× `CascadeController`, e o loop chama na ordem: ler encoders → atualizar estimadores → ler sensores → calcular posição → outer PID → cascade → motors.

**Tech Stack:** C++17, Unity Test Framework, integração com módulos existentes (`PIDController`, `LineFollower`, `SpeedProfile`).

---

## File Map

| File | Action | Purpose |
|------|--------|---------|
| `src/control/CascadeController.h` | Create | Interface (correction+base+RPM → PWM L/R) |
| `src/control/CascadeController.cpp` | Create | Implementação pura (sem dependência de hardware) |
| `test/test_cascade/test_cascade.cpp` | Create | 8 testes Unity nativos |
| `src/config.h` | Modify | `KP_VEL_DEFAULT`, `KI_VEL_DEFAULT`, `KD_VEL_DEFAULT`, `MAX_RPM_DEFAULT`, `BASE_RPM_DEFAULT` |
| `src/main.cpp` | Modify | Wire up encoders + estimators + cascade no Core 0 loop |

---

## Task C1: Criar testes RED para CascadeController

**Files:**
- Create: `test/test_cascade/test_cascade.cpp`

- [ ] **Step 1: Criar arquivo de teste com 8 testes**

```cpp
// test/test_cascade/test_cascade.cpp
#include <unity.h>
#include "control/CascadeController.h"
#include "control/PIDController.h"

// PIDs internos para os testes — Kp=1, Ki=0, Kd=0 → erro direto vira PWM
// (controlador puramente proporcional simplifica verificações)
static PIDController* innerL = nullptr;
static PIDController* innerR = nullptr;
static CascadeController* cc = nullptr;

void setUp() {
    innerL = new PIDController(1.0f, 0.0f, 0.0f, -1000.0f, 1000.0f, 0.01f);
    innerR = new PIDController(1.0f, 0.0f, 0.0f, -1000.0f, 1000.0f, 0.01f);
    // maxRpm=1000, maxPwm=1023 (10-bit LEDC do ESP32)
    cc = new CascadeController(*innerL, *innerR, 1000.0f, 1023);
}

void tearDown() {
    delete cc;     cc = nullptr;
    delete innerR; innerR = nullptr;
    delete innerL; innerL = nullptr;
}

// ─── 1. Linha reta (correction=0) → setpoints iguais ─────────────────────
void test_cascade_straight_line_equal_setpoints() {
    int pwmL = 0, pwmR = 0;
    // correction=0, base=500, ambos motores parados (rpm=0) → erro=500 cada
    cc->compute(0.0f, 500.0f, 0.0f, 0.0f, pwmL, pwmR);
    TEST_ASSERT_EQUAL_INT(pwmL, pwmR);
    TEST_ASSERT_EQUAL_INT(500, pwmL);
}

// ─── 2. Correção positiva → motor L acelera, motor R desacelera ──────────
void test_cascade_positive_correction_differentiates_motors() {
    int pwmL = 0, pwmR = 0;
    // correction=+100, base=500 → setL=600, setR=400 (ambos rpm=0)
    cc->compute(100.0f, 500.0f, 0.0f, 0.0f, pwmL, pwmR);
    TEST_ASSERT_TRUE(pwmL > pwmR);
    TEST_ASSERT_EQUAL_INT(600, pwmL);
    TEST_ASSERT_EQUAL_INT(400, pwmR);
}

// ─── 3. Motor atingiu setpoint → PWM cai a zero ──────────────────────────
void test_cascade_motor_at_setpoint_outputs_zero() {
    int pwmL = 0, pwmR = 0;
    // setpoint=500, actualRpm=500 → erro=0 → pwm=0
    cc->compute(0.0f, 500.0f, 500.0f, 500.0f, pwmL, pwmR);
    TEST_ASSERT_EQUAL_INT(0, pwmL);
    TEST_ASSERT_EQUAL_INT(0, pwmR);
}

// ─── 4. Motor mais lento que setpoint → PWM positivo ─────────────────────
void test_cascade_slow_motor_gets_positive_pwm() {
    int pwmL = 0, pwmR = 0;
    // setpoint=500, actual=200 → erro=300 → pwm=300
    cc->compute(0.0f, 500.0f, 200.0f, 500.0f, pwmL, pwmR);
    TEST_ASSERT_EQUAL_INT(300, pwmL);
    TEST_ASSERT_EQUAL_INT(0, pwmR);
}

// ─── 5. Setpoint > maxRpm → clamp em maxRpm ──────────────────────────────
void test_cascade_setpoint_clamped_to_max_rpm() {
    int pwmL = 0, pwmR = 0;
    // base=1500 (> maxRpm=1000), correction=0 → setpoint clamped em 1000
    cc->compute(0.0f, 1500.0f, 0.0f, 0.0f, pwmL, pwmR);
    TEST_ASSERT_EQUAL_INT(1000, pwmL);
    TEST_ASSERT_EQUAL_INT(1000, pwmR);
}

// ─── 6. Setpoint negativo → motor em reverso ─────────────────────────────
void test_cascade_negative_setpoint_drives_reverse() {
    int pwmL = 0, pwmR = 0;
    // base=300, correction=+500 → setL=800, setR=-200 (R inverte)
    cc->compute(500.0f, 300.0f, 0.0f, 0.0f, pwmL, pwmR);
    TEST_ASSERT_EQUAL_INT(800, pwmL);
    TEST_ASSERT_EQUAL_INT(-200, pwmR);
}

// ─── 7. PWM saturado em ±maxPwm quando erro extremo ──────────────────────
void test_cascade_pwm_clamped_to_max() {
    // PID interno com Kp=10 — erro=200 → output 2000, clamp em 1023
    PIDController bigL(10.0f, 0.0f, 0.0f, -1023.0f, 1023.0f, 0.01f);
    PIDController bigR(10.0f, 0.0f, 0.0f, -1023.0f, 1023.0f, 0.01f);
    CascadeController bigCC(bigL, bigR, 1000.0f, 1023);
    int pwmL = 0, pwmR = 0;
    bigCC.compute(0.0f, 200.0f, 0.0f, 0.0f, pwmL, pwmR);
    TEST_ASSERT_EQUAL_INT(1023, pwmL);
    TEST_ASSERT_EQUAL_INT(1023, pwmR);
}

// ─── 8. setInnerGains atualiza ambos PIDs internos ───────────────────────
void test_cascade_set_inner_gains_updates_both_pids() {
    cc->setInnerGains(5.0f, 0.1f, 0.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, innerL->getKp());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, innerR->getKp());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f, innerL->getKi());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, innerR->getKd());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_cascade_straight_line_equal_setpoints);
    RUN_TEST(test_cascade_positive_correction_differentiates_motors);
    RUN_TEST(test_cascade_motor_at_setpoint_outputs_zero);
    RUN_TEST(test_cascade_slow_motor_gets_positive_pwm);
    RUN_TEST(test_cascade_setpoint_clamped_to_max_rpm);
    RUN_TEST(test_cascade_negative_setpoint_drives_reverse);
    RUN_TEST(test_cascade_pwm_clamped_to_max);
    RUN_TEST(test_cascade_set_inner_gains_updates_both_pids);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar — deve falhar no link**

```bash
pio test -e native -f test_cascade 2>&1 | tail -10
```

Expected: erros reclamando que `control/CascadeController.h` não existe.

- [ ] **Step 3: Commit (RED state)**

```bash
git add test/test_cascade/test_cascade.cpp
git commit -m "test(cascade): add 8 failing tests for cascade PID controller"
```

---

## Task C2: Criar CascadeController.h

**Files:**
- Create: `src/control/CascadeController.h`

- [ ] **Step 1: Escrever interface**

```cpp
// src/control/CascadeController.h
#pragma once
#include "control/PIDController.h"

/**
 * Controlador em cascata para diferencial duplo:
 *
 *   correction (do PID externo de posição)
 *        ↓
 *   tradução: setpointL = base + correction,  setpointR = base − correction
 *        ↓
 *   2× PID interno (compute(setpoint, actualRpm)) → PWM L/R
 *
 * O cliente é responsável por medir `actualRpmL/R` (ex: VelocityEstimator)
 * e passar via `compute()`. Isso desacopla a classe do hardware e facilita
 * testes determinísticos.
 *
 * Saturação:
 *  - setpoint clamped em [−maxRpm, +maxRpm] antes de entrar no PID interno
 *  - PWM já é clamped pelos limites do PIDController interno
 *
 * Convenções de sinal:
 *  - correction > 0  → curva para a direita (motor L mais rápido que R)
 *  - PWM > 0         → frente; PWM < 0 → ré (MotorDriver decide direção)
 */
class CascadeController {
public:
    CascadeController(PIDController& innerLeft, PIDController& innerRight,
                      float maxRpm, int maxPwm);

    // correction      : saída do PID externo (em unidade de RPM diferencial)
    // targetBaseRpm   : alvo de velocidade linear do robô (em RPM)
    // actualRpmL/R    : RPM real medido (ex: VelocityEstimator::getRPM())
    // pwmL/pwmR       : saídas (referência), com sinal
    void compute(float correction, float targetBaseRpm,
                 float actualRpmL, float actualRpmR,
                 int& pwmL, int& pwmR);

    // Aplica os mesmos ganhos em ambos os PIDs internos (uso típico via BLE).
    void setInnerGains(float kp, float ki, float kd);

    void setMaxRpm(float maxRpm);
    void setMaxPwm(int   maxPwm);

private:
    PIDController& _pidL;
    PIDController& _pidR;
    float _maxRpm;
    int   _maxPwm;

    float _clampF(float v, float lo, float hi) const;
};
```

- [ ] **Step 2: Adicionar constantes em config.h**

Localizar a seção `// ═══ ENCODER / VELOCITY (Fase B) ═══` em `src/config.h` e adicionar **logo após**:

```cpp
// ═══ CASCADE PID (Fase C — controle interno de velocidade) ═════════════════
// Ganhos INICIAIS conservadores; sintonizar via BLE em pista.
#define KP_VEL_DEFAULT   0.8f
#define KI_VEL_DEFAULT   0.0f
#define KD_VEL_DEFAULT   0.05f

// Faixa esperada de RPM no shaft de saída — N20 1500 RPM @ 6V.
// MAX_RPM é o teto absoluto (clamp); BASE_RPM é o alvo nominal em reta.
#define MAX_RPM_DEFAULT  1200.0f
#define BASE_RPM_DEFAULT  600.0f
```

- [ ] **Step 3: Verificar que compila isoladamente**

```bash
pio run -e native 2>&1 | tail -3
```

Expected: ainda erros de link de teste (cpp não criado), mas headers OK.

- [ ] **Step 4: Commit**

```bash
git add src/control/CascadeController.h src/config.h
git commit -m "feat(cascade): add cascade controller interface and config defaults"
```

---

## Task C3: Implementar CascadeController.cpp

**Files:**
- Create: `src/control/CascadeController.cpp`

- [ ] **Step 1: Escrever implementação**

```cpp
// src/control/CascadeController.cpp
#include "control/CascadeController.h"
#include <algorithm>

CascadeController::CascadeController(PIDController& innerLeft, PIDController& innerRight,
                                     float maxRpm, int maxPwm)
    : _pidL(innerLeft), _pidR(innerRight), _maxRpm(maxRpm), _maxPwm(maxPwm) {}

void CascadeController::compute(float correction, float targetBaseRpm,
                                float actualRpmL, float actualRpmR,
                                int& pwmL, int& pwmR) {
    // 1. Traduz correction + base em setpoints por motor.
    float setpointL = targetBaseRpm + correction;
    float setpointR = targetBaseRpm - correction;

    // 2. Clamp dos setpoints (proteção contra over-PID externo).
    setpointL = _clampF(setpointL, -_maxRpm, +_maxRpm);
    setpointR = _clampF(setpointR, -_maxRpm, +_maxRpm);

    // 3. PID interno: erro = setpoint − measurement → saída em PWM.
    float outL = _pidL.compute(setpointL, actualRpmL);
    float outR = _pidR.compute(setpointR, actualRpmR);

    // 4. Saturação final em PWM (já vem clamped pelo PIDController, mas garantimos
    //    cast seguro para int e respeito ao limite externo se setMaxPwm() mudou).
    outL = _clampF(outL, -static_cast<float>(_maxPwm), +static_cast<float>(_maxPwm));
    outR = _clampF(outR, -static_cast<float>(_maxPwm), +static_cast<float>(_maxPwm));
    pwmL = static_cast<int>(outL);
    pwmR = static_cast<int>(outR);
}

void CascadeController::setInnerGains(float kp, float ki, float kd) {
    _pidL.setTunings(kp, ki, kd);
    _pidR.setTunings(kp, ki, kd);
}

void CascadeController::setMaxRpm(float maxRpm) { _maxRpm = maxRpm; }
void CascadeController::setMaxPwm(int   maxPwm) { _maxPwm = maxPwm; }

float CascadeController::_clampF(float v, float lo, float hi) const {
    return std::max(lo, std::min(hi, v));
}
```

- [ ] **Step 2: Rodar testes — TODOS devem passar**

```bash
pio test -e native -f test_cascade
```

Expected:
```
8 Tests 0 Failures 0 Ignored
OK
```

- [ ] **Step 3: Commit (GREEN state)**

```bash
git add src/control/CascadeController.cpp
git commit -m "feat(cascade): implement cascade controller with saturation"
```

---

## Task C4: Integrar no main.cpp — instanciar módulos

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Localizar bloco de instâncias estáticas (linhas 25-58)**

```bash
grep -n "static MotorDriver" src/main.cpp
```

Expected output:
```
58:static MotorDriver motors(cfgA, cfgB, PIN_MOTOR_STBY);
```

- [ ] **Step 2: Adicionar includes no topo (após linha 20)**

Localizar:
```cpp
#include "comm/Logger.h"
```

E adicionar **logo abaixo**:
```cpp
#include "sensors/Encoder.h"
#include "sensors/VelocityEstimator.h"
#include "storage/NvsStore.h"
#include "control/CascadeController.h"
```

- [ ] **Step 3: Adicionar instâncias estáticas após `static MotorDriver motors(...)`**

Localizar linha 58:
```cpp
static MotorDriver motors(cfgA, cfgB, PIN_MOTOR_STBY);
```

Adicionar **logo abaixo** (antes da seção "═══ Estado compartilhado"):

```cpp
// ═══ Cascade PID — Fase C ════════════════════════════════════════════════════
static Encoder encL(PIN_ENC_LEFT_A,  PIN_ENC_LEFT_B);
static Encoder encR(PIN_ENC_RIGHT_A, PIN_ENC_RIGHT_B);

static NvsStore nvs;
static VelocityEstimator velL(encL, nvs, "left");
static VelocityEstimator velR(encR, nvs, "right");

static PIDController pidVelL(
    KP_VEL_DEFAULT, KI_VEL_DEFAULT, KD_VEL_DEFAULT,
    -static_cast<float>(PWM_MAX), +static_cast<float>(PWM_MAX), 0.002f);
static PIDController pidVelR(
    KP_VEL_DEFAULT, KI_VEL_DEFAULT, KD_VEL_DEFAULT,
    -static_cast<float>(PWM_MAX), +static_cast<float>(PWM_MAX), 0.002f);

static CascadeController cascade(pidVelL, pidVelR, MAX_RPM_DEFAULT, PWM_MAX);
```

- [ ] **Step 4: Compilar — deve passar (instâncias não usadas ainda)**

```bash
pio run -e esp32dev 2>&1 | tail -10
```

Expected: build successful, possivelmente warnings de "unused" — esperado.

- [ ] **Step 5: Commit**

```bash
git add src/main.cpp
git commit -m "feat(main): wire cascade controller + encoders + velocity estimators"
```

---

## Task C5: Inicializar encoders/NVS no setup()

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Localizar `setup()` (linha 208)**

```bash
grep -n "^void setup" src/main.cpp
```

Expected: `208:void setup() {`

- [ ] **Step 2: Adicionar inicialização dos novos módulos logo após `sensors.begin()`**

Localizar dentro de `setup()`:
```cpp
    sensors.begin();
    motors.begin();
```

Substituir por:
```cpp
    sensors.begin();
    motors.begin();
    encL.begin();
    encR.begin();
    nvs.begin("lfr");
    velL.begin();
    velR.begin();
```

- [ ] **Step 3: Compilar**

```bash
pio run -e esp32dev 2>&1 | tail -3
```

Expected: build successful.

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "feat(main): initialize encoders, NVS and velocity estimators in setup"
```

---

## Task C6: Substituir DifferentialDrive pelo CascadeController no loop RUNNING

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Localizar o `case RobotState::RUNNING` (linha ~267)**

```bash
grep -n "case RobotState::RUNNING" src/main.cpp
```

- [ ] **Step 2: Substituir o bloco completo do case RUNNING**

Localizar:
```cpp
        case RobotState::RUNNING: {
            uint32_t loopStart = micros();

            int raw[SENSOR_COUNT];
            sensors.readAll(raw);

            int   leftPwm  = 0;
            int   rightPwm = 0;
            float normErr  = 0.0f;

            taskENTER_CRITICAL(&lineFollowerMux);
            lineFollower.update(raw, leftPwm, rightPwm, &normErr);
            taskEXIT_CRITICAL(&lineFollowerMux);

            motors.setSpeed(MotorId::A, leftPwm);
            motors.setSpeed(MotorId::B, rightPwm);
```

Substituir por:
```cpp
        case RobotState::RUNNING: {
            uint32_t loopStart = micros();

            // 1. Atualiza estimadores de velocidade (dt fixo do loop principal)
            constexpr uint32_t LOOP_DT_MS = 2;  // LOOP_PERIOD_US = 2000
            velL.update(LOOP_DT_MS);
            velR.update(LOOP_DT_MS);

            // 2. Lê sensores de linha
            int raw[SENSOR_COUNT];
            sensors.readAll(raw);

            // 3. PID externo via LineFollower — retorna PWMs antigos (open-loop)
            //    Ignoramos os PWMs; pegamos só a correção via normErr.
            int   discardL = 0;
            int   discardR = 0;
            float normErr  = 0.0f;
            taskENTER_CRITICAL(&lineFollowerMux);
            lineFollower.update(raw, discardL, discardR, &normErr);
            taskEXIT_CRITICAL(&lineFollowerMux);

            // 4. Cascade: traduz correction (em PWM units) para RPM diferencial.
            //    discardL − discardR ≈ 2× correction (DifferentialDrive interno).
            float correctionRpm = static_cast<float>(discardL - discardR) * 0.5f
                                  * (MAX_RPM_DEFAULT / static_cast<float>(PWM_MAX));

            int leftPwm  = 0;
            int rightPwm = 0;
            cascade.compute(correctionRpm, BASE_RPM_DEFAULT,
                            velL.getRPM(), velR.getRPM(),
                            leftPwm, rightPwm);

            // 5. Aplica PWM nos motores
            motors.setSpeed(MotorId::A, leftPwm);
            motors.setSpeed(MotorId::B, rightPwm);
```

(O restante do case — telemetry snapshot, logger, btnPressed, dtUs check — fica intocado.)

- [ ] **Step 3: Compilar**

```bash
pio run -e esp32dev 2>&1 | tail -10
```

Expected: build successful (alguns warnings de unused acceptable, sem erros).

- [ ] **Step 4: Rodar todos os testes nativos — sem regressão**

```bash
pio test -e native 2>&1 | tail -3
```

Expected: `xx Tests 0 Failures` (todos os suites passam, incluindo test_cascade).

- [ ] **Step 5: Commit**

```bash
git add src/main.cpp
git commit -m "feat(main): replace open-loop with cascade PID in RUNNING state"
```

---

## Task C7: Telemetria estendida com RPM

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Localizar `struct TelemetrySnapshot` (linha ~61)**

```bash
grep -n "struct TelemetrySnapshot" src/main.cpp
```

- [ ] **Step 2: Adicionar campos rpmL/R e setpointL/R**

Localizar:
```cpp
struct TelemetrySnapshot {
    float    pos   = 0.0f;
    float    corr  = 0.0f;
    int      vL    = 0;
    int      vR    = 0;
    uint32_t dtUs  = 0;
    float    sensorsPct[SENSOR_COUNT] = {};  // [0–100]
};
```

Substituir por:
```cpp
struct TelemetrySnapshot {
    float    pos       = 0.0f;
    float    corr      = 0.0f;
    int      vL        = 0;
    int      vR        = 0;
    uint32_t dtUs      = 0;
    float    rpmL      = 0.0f;
    float    rpmR      = 0.0f;
    float    setpointL = 0.0f;
    float    setpointR = 0.0f;
    float    sensorsPct[SENSOR_COUNT] = {};
};
```

- [ ] **Step 3: Atualizar snapshot dentro do case RUNNING**

Localizar dentro de `case RobotState::RUNNING` o bloco:
```cpp
                taskENTER_CRITICAL(&g_snapMux);
                g_snap.pos  = normErr * 3500.0f;
                g_snap.corr = static_cast<float>(leftPwm - rightPwm);
                g_snap.vL   = leftPwm;
                g_snap.vR   = rightPwm;
                g_snap.dtUs = dtUs;
                for (int i = 0; i < SENSOR_COUNT; i++)
                    g_snap.sensorsPct[i] = norm[i] / 10.0f;
                taskEXIT_CRITICAL(&g_snapMux);
```

Substituir por:
```cpp
                taskENTER_CRITICAL(&g_snapMux);
                g_snap.pos       = normErr * 3500.0f;
                g_snap.corr      = static_cast<float>(leftPwm - rightPwm);
                g_snap.vL        = leftPwm;
                g_snap.vR        = rightPwm;
                g_snap.dtUs      = dtUs;
                g_snap.rpmL      = velL.getRPM();
                g_snap.rpmR      = velR.getRPM();
                g_snap.setpointL = BASE_RPM_DEFAULT + correctionRpm;
                g_snap.setpointR = BASE_RPM_DEFAULT - correctionRpm;
                for (int i = 0; i < SENSOR_COUNT; i++)
                    g_snap.sensorsPct[i] = norm[i] / 10.0f;
                taskEXIT_CRITICAL(&g_snapMux);
```

- [ ] **Step 4: Compilar — verifica que telemetria estendida não quebra nada**

```bash
pio run -e esp32dev 2>&1 | tail -3
```

Expected: build successful. (BLETuner ainda não envia os novos campos — isso é Fase D.)

- [ ] **Step 5: Commit**

```bash
git add src/main.cpp
git commit -m "feat(main): extend telemetry snapshot with RPM and setpoint per motor"
```

---

## Critério de "Done" — Fase C

- [ ] `pio test -e native -f test_cascade` → 8/8 PASSED
- [ ] `pio run -e esp32dev` → SUCCESS
- [ ] Sem regressão em testes existentes (`pio test -e native` → todos suites OK)
- [ ] main.cpp instancia: 2× Encoder, NvsStore, 2× VelocityEstimator, 2× PIDController (interno), 1× CascadeController
- [ ] Loop RUNNING usa cascade em vez de DifferentialDrive direto
- [ ] TelemetrySnapshot expõe rpmL/R + setpointL/R (consumido na Fase D)
- [ ] DifferentialDrive ainda existe (usado internamente pelo LineFollower antigo) — removeremos numa fase futura se necessário
