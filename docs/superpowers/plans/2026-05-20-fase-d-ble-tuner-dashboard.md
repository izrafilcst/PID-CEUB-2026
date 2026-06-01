# Fase D — BLE Tuner Expandido + Dashboard com RPM Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expor os ganhos do PID interno (velocidade) + RPM máximo + procedimento de auto-calibração de encoder via BLE, e estender o dashboard web para mostrar RPM real vs setpoint e ajustar tudo em tempo real.

**Architecture:** Estende o protocolo JSON existente do `BLETuner` (já com 2 characteristics) adicionando 3 novos comandos: `{"t":"pidv",...}` (ganhos PID velocidade), `{"t":"rpm",...}` (max RPM + base RPM), `{"t":"cal",...}` (start/finish calibração de encoder com N rotações). A telemetria ganha 4 campos: `rpmL`, `rpmR`, `spL` (setpoint L), `spR`. O dashboard ([lfr-js.js](lfr-js.js)) parseia os novos campos, exibe 2 sliders adicionais (`pidv`, `rpm`), um botão "Calibrar Encoder" com modal pedindo número de rotações, e o gráfico ganha 2 linhas (RPM L e RPM R com cores distintas) e duas linhas tracejadas para os setpoints.

**Tech Stack:** C++17 (firmware), JavaScript ES2017+ (dashboard), Web Bluetooth API, JSON.

---

## File Map

| File | Action | Purpose |
|------|--------|---------|
| `src/comm/BLETuner.h` | Modify | Adicionar `VelocityPIDParams`, `RpmParams`, `CalibrationCmd`, consumers |
| `src/comm/BLETuner.cpp` | Modify | Parser dos 3 novos comandos JSON + telemetria estendida |
| `src/main.cpp` | Modify | taskComm aplica params recebidos no `cascade` e `velL/velR` |
| `test/test_ble_tuner/test_ble_tuner.cpp` | Create | 8 testes para parser e fluxo de calibração (native) |
| `lfr-js.js` | Modify | Novos sliders + parser de rpmL/rpmR/spL/spR + modal de calibração |

---

## Task D1: Estender BLETuner.h com novas struct e API

**Files:**
- Modify: `src/comm/BLETuner.h`

- [ ] **Step 1: Localizar declarações de struct (linhas 9-20)**

```bash
grep -n "^struct \|^class BLETuner" src/comm/BLETuner.h
```

Expected:
```
9:struct PIDParams {
15:struct SpeedParams {
34:class BLETuner {
```

- [ ] **Step 2: Adicionar novas structs após `SpeedParams` (linha ~20)**

Adicionar **após** o fechamento de `struct SpeedParams { ... };`:

```cpp
struct VelocityPIDParams {
    float kp = 0.8f;
    float ki = 0.0f;
    float kd = 0.05f;
};

struct RpmParams {
    float maxRpm  = 1200.0f;
    float baseRpm = 600.0f;
};

struct CalibrationCmd {
    enum class Op : uint8_t { NONE, START, FINISH };
    Op   op        = Op::NONE;
    int  rotations = 0;
    bool leftSide  = true;   // true = roda esquerda, false = direita
};
```

- [ ] **Step 3: Adicionar membros públicos novos na classe**

Localizar dentro de `class BLETuner` o bloco de "Recepção":
```cpp
    // Recepção de comandos — consumir no loop Core 1
    bool hasNewPID()   const;
    bool hasNewSpeed() const;
    PIDParams   getPID();
    SpeedParams getSpeed();
    bool consumeStart();
    bool consumeStop();
    bool consumeReset();
    bool consumeNewConnection();
```

Adicionar **logo abaixo**:
```cpp
    // Novos comandos — Fase D
    bool hasNewVelocityPID() const;
    bool hasNewRpm()         const;
    bool hasNewCalibration() const;
    VelocityPIDParams getVelocityPID();  // consome flag
    RpmParams         getRpm();          // consome flag
    CalibrationCmd    getCalibration();  // consome flag
```

- [ ] **Step 4: Atualizar `notifyTelemetry` para incluir RPM e setpoints**

Localizar a declaração:
```cpp
    void notifyTelemetry(float pos, float corr, int vL, int vR, uint32_t dtUs,
                         const float* sensors, int nSensors, float batPct,
                         float bestLapSec, bool hasLap,
                         const float* laps, int nLaps);
```

Substituir por:
```cpp
    void notifyTelemetry(float pos, float corr, int vL, int vR, uint32_t dtUs,
                         float rpmL, float rpmR, float spL, float spR,
                         const float* sensors, int nSensors, float batPct,
                         float bestLapSec, bool hasLap,
                         const float* laps, int nLaps);
```

- [ ] **Step 5: Adicionar membros privados para as novas flags**

Localizar a seção `private:` (linha ~63):
```cpp
private:
    PIDParams   _pid;
    SpeedParams _speed;
    bool _newPID   = false;
    bool _newSpeed = false;
    bool _cmdStart = false;
    bool _cmdStop  = false;
    bool _cmdReset = false;
    bool _newConn  = false;
```

Substituir por:
```cpp
private:
    PIDParams         _pid;
    SpeedParams       _speed;
    VelocityPIDParams _velPid;
    RpmParams         _rpm;
    CalibrationCmd    _cal;
    bool _newPID    = false;
    bool _newSpeed  = false;
    bool _newVelPID = false;
    bool _newRpm    = false;
    bool _newCal    = false;
    bool _cmdStart  = false;
    bool _cmdStop   = false;
    bool _cmdReset  = false;
    bool _newConn   = false;
```

- [ ] **Step 6: Compilar (esperar falha em main.cpp se já usar a antiga signature)**

```bash
pio run -e esp32dev 2>&1 | tail -20
```

Expected: erros em `main.cpp` relativos à signature de `notifyTelemetry` (esperado — será corrigido na D4).

- [ ] **Step 7: Commit**

```bash
git add src/comm/BLETuner.h
git commit -m "feat(ble): extend tuner interface with velocity PID, RPM, calibration"
```

---

## Task D2: Implementar parser dos novos comandos no BLETuner.cpp

**Files:**
- Modify: `src/comm/BLETuner.cpp`

- [ ] **Step 1: Localizar `_onWrite()` (linhas 70-99)**

```bash
grep -n "void BLETuner::_onWrite" src/comm/BLETuner.cpp
```

- [ ] **Step 2: Estender o parser para os 3 novos tipos de mensagem**

Localizar dentro de `_onWrite()`:
```cpp
    if (strncmp(tp, "pid", 3) == 0) {
        _pid.kp = jsonFloat(json, "kp", _pid.kp);
        _pid.ki = jsonFloat(json, "ki", _pid.ki);
        _pid.kd = jsonFloat(json, "kd", _pid.kd);
        _newPID = true;
    } else if (strncmp(tp, "spd", 3) == 0) {
```

Substituir o bloco inteiro `if/else if` por:
```cpp
    if (strncmp(tp, "pidv", 4) == 0) {
        // PID INTERNO (velocidade) — precisa vir antes de "pid" porque é prefixo
        _velPid.kp = jsonFloat(json, "kp", _velPid.kp);
        _velPid.ki = jsonFloat(json, "ki", _velPid.ki);
        _velPid.kd = jsonFloat(json, "kd", _velPid.kd);
        _newVelPID = true;
    } else if (strncmp(tp, "pid", 3) == 0) {
        _pid.kp = jsonFloat(json, "kp", _pid.kp);
        _pid.ki = jsonFloat(json, "ki", _pid.ki);
        _pid.kd = jsonFloat(json, "kd", _pid.kd);
        _newPID = true;
    } else if (strncmp(tp, "spd", 3) == 0) {
        _speed.baseSpeed = jsonInt(json,  "base", _speed.baseSpeed);
        _speed.minSpeed  = jsonInt(json,  "min",  _speed.minSpeed);
        _speed.maxSpeed  = jsonInt(json,  "max",  _speed.maxSpeed);
        _speed.threshold = jsonFloat(json, "thrs", _speed.threshold);
        _newSpeed = true;
    } else if (strncmp(tp, "rpm", 3) == 0) {
        _rpm.maxRpm  = jsonFloat(json, "max",  _rpm.maxRpm);
        _rpm.baseRpm = jsonFloat(json, "base", _rpm.baseRpm);
        _newRpm = true;
    } else if (strncmp(tp, "cal", 3) == 0) {
        // {"t":"cal","op":"start","side":"L","rot":10}
        // {"t":"cal","op":"finish","side":"L","rot":10}
        const char* op = strstr(json, "\"op\":\"");
        if (op) {
            op += 6;  // após "op":"
            if (strncmp(op, "start", 5) == 0) {
                _cal.op = CalibrationCmd::Op::START;
            } else if (strncmp(op, "finish", 6) == 0) {
                _cal.op = CalibrationCmd::Op::FINISH;
            }
            _cal.rotations = jsonInt(json, "rot", 10);
            const char* sd = strstr(json, "\"side\":\"");
            _cal.leftSide = !(sd && sd[8] == 'R');
            _newCal = true;
        }
    } else if (strncmp(tp, "start", 5) == 0) {
        _cmdStart = true;
    } else if (strncmp(tp, "stop", 4) == 0) {
        _cmdStop = true;
    } else if (strncmp(tp, "reset", 5) == 0) {
        _cmdReset = true;
    }
```

- [ ] **Step 3: Atualizar signature de `notifyTelemetry()` e formato do JSON**

Localizar:
```cpp
void BLETuner::notifyTelemetry(float pos, float corr, int vL, int vR, uint32_t dtUs,
                                const float* sensors, int nSensors, float batPct,
                                float bestLapSec, bool hasLap,
                                const float* laps, int nLaps) {
```

Substituir por:
```cpp
void BLETuner::notifyTelemetry(float pos, float corr, int vL, int vR, uint32_t dtUs,
                                float rpmL, float rpmR, float spL, float spR,
                                const float* sensors, int nSensors, float batPct,
                                float bestLapSec, bool hasLap,
                                const float* laps, int nLaps) {
```

E localizar o `snprintf(buf, ...)` que monta o JSON:
```cpp
    snprintf(buf, sizeof(buf),
        "{\"t\":\"tel\",\"pos\":%.1f,\"corr\":%.1f,\"vL\":%d,\"vR\":%d,"
        "\"dt\":%lu,\"s\":%s,\"bat\":%.1f,\"lap\":%s,\"laps\":%s}",
        pos, corr, vL, vR, static_cast<unsigned long>(dtUs), sarr, batPct, lapVal, larr);
```

Substituir por:
```cpp
    snprintf(buf, sizeof(buf),
        "{\"t\":\"tel\",\"pos\":%.1f,\"corr\":%.1f,\"vL\":%d,\"vR\":%d,"
        "\"dt\":%lu,\"rpmL\":%.1f,\"rpmR\":%.1f,\"spL\":%.1f,\"spR\":%.1f,"
        "\"s\":%s,\"bat\":%.1f,\"lap\":%s,\"laps\":%s}",
        pos, corr, vL, vR, static_cast<unsigned long>(dtUs),
        rpmL, rpmR, spL, spR,
        sarr, batPct, lapVal, larr);
```

Também aumentar o tamanho do buffer (já estava 320, agora precisa ~400):
```cpp
    char buf[400];
```

(Localizar a linha `char buf[320];` dentro de `notifyTelemetry` e substituir.)

- [ ] **Step 4: Atualizar stub native de `notifyTelemetry`**

Localizar:
```cpp
void BLETuner::notifyTelemetry(float, float, int, int, uint32_t,
                                const float*, int, float,
                                float, bool, const float*, int) {}
```

Substituir por:
```cpp
void BLETuner::notifyTelemetry(float, float, int, int, uint32_t,
                                float, float, float, float,
                                const float*, int, float,
                                float, bool, const float*, int) {}
```

- [ ] **Step 5: Adicionar implementações dos novos consumers**

Adicionar **antes** do final do arquivo (após `consumeNewConnection()`):

```cpp
bool BLETuner::hasNewVelocityPID() const {
#ifndef NATIVE_BUILD
    taskENTER_CRITICAL(&const_cast<BLETuner*>(this)->_mux);
#endif
    bool v = _newVelPID;
#ifndef NATIVE_BUILD
    taskEXIT_CRITICAL(&const_cast<BLETuner*>(this)->_mux);
#endif
    return v;
}

bool BLETuner::hasNewRpm() const {
#ifndef NATIVE_BUILD
    taskENTER_CRITICAL(&const_cast<BLETuner*>(this)->_mux);
#endif
    bool v = _newRpm;
#ifndef NATIVE_BUILD
    taskEXIT_CRITICAL(&const_cast<BLETuner*>(this)->_mux);
#endif
    return v;
}

bool BLETuner::hasNewCalibration() const {
#ifndef NATIVE_BUILD
    taskENTER_CRITICAL(&const_cast<BLETuner*>(this)->_mux);
#endif
    bool v = _newCal;
#ifndef NATIVE_BUILD
    taskEXIT_CRITICAL(&const_cast<BLETuner*>(this)->_mux);
#endif
    return v;
}

VelocityPIDParams BLETuner::getVelocityPID() {
#ifndef NATIVE_BUILD
    taskENTER_CRITICAL(&_mux);
#endif
    VelocityPIDParams p = _velPid;
    _newVelPID = false;
#ifndef NATIVE_BUILD
    taskEXIT_CRITICAL(&_mux);
#endif
    return p;
}

RpmParams BLETuner::getRpm() {
#ifndef NATIVE_BUILD
    taskENTER_CRITICAL(&_mux);
#endif
    RpmParams r = _rpm;
    _newRpm = false;
#ifndef NATIVE_BUILD
    taskEXIT_CRITICAL(&_mux);
#endif
    return r;
}

CalibrationCmd BLETuner::getCalibration() {
#ifndef NATIVE_BUILD
    taskENTER_CRITICAL(&_mux);
#endif
    CalibrationCmd c = _cal;
    _newCal = false;
    _cal.op = CalibrationCmd::Op::NONE;  // limpa para evitar reprocesso
#ifndef NATIVE_BUILD
    taskEXIT_CRITICAL(&_mux);
#endif
    return c;
}
```

- [ ] **Step 6: Compilar — esperado erro só em main.cpp (call site da Telemetry)**

```bash
pio run -e esp32dev 2>&1 | tail -10
```

Expected: erro de signature mismatch em main.cpp — corrigido na D4.

- [ ] **Step 7: Commit**

```bash
git add src/comm/BLETuner.cpp
git commit -m "feat(ble): parse new commands (pidv, rpm, cal) and extend telemetry JSON"
```

---

## Task D3: Adicionar testes nativos para o parser

**Files:**
- Create: `test/test_ble_tuner/test_ble_tuner.cpp`

- [ ] **Step 1: Criar testes para os novos comandos**

```cpp
// test/test_ble_tuner/test_ble_tuner.cpp
#include <unity.h>
#include "comm/BLETuner.h"

// Em NATIVE_BUILD, _onWrite() não existe (é privado e parte do bloco ESP32).
// Estes testes focam no contrato de consumers — em native build o estado
// começa com flags = false e structs com defaults. Smoke tests apenas.

static BLETuner* t = nullptr;
void setUp()    { t = new BLETuner(); }
void tearDown() { delete t; t = nullptr; }

void test_ble_default_velocity_pid_returns_zero_flag() {
    TEST_ASSERT_FALSE(t->hasNewVelocityPID());
}

void test_ble_default_rpm_returns_zero_flag() {
    TEST_ASSERT_FALSE(t->hasNewRpm());
}

void test_ble_default_calibration_returns_none() {
    TEST_ASSERT_FALSE(t->hasNewCalibration());
    CalibrationCmd c = t->getCalibration();
    TEST_ASSERT_TRUE(c.op == CalibrationCmd::Op::NONE);
}

void test_ble_velocity_pid_defaults_are_safe() {
    VelocityPIDParams p = t->getVelocityPID();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.8f,  p.kp);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f,  p.ki);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.05f, p.kd);
}

void test_ble_rpm_defaults_are_safe() {
    RpmParams r = t->getRpm();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1200.0f, r.maxRpm);
    TEST_ASSERT_FLOAT_WITHIN(0.001f,  600.0f, r.baseRpm);
}

void test_ble_calibration_default_rotations_zero() {
    CalibrationCmd c = t->getCalibration();
    TEST_ASSERT_EQUAL_INT(0, c.rotations);
}

void test_ble_calibration_default_left_side_true() {
    CalibrationCmd c = t->getCalibration();
    TEST_ASSERT_TRUE(c.leftSide);
}

void test_ble_existing_pid_consumer_still_works() {
    // Garantia de não-regressão
    TEST_ASSERT_FALSE(t->hasNewPID());
    PIDParams p = t->getPID();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f,  p.kp);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.0f, p.kd);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_ble_default_velocity_pid_returns_zero_flag);
    RUN_TEST(test_ble_default_rpm_returns_zero_flag);
    RUN_TEST(test_ble_default_calibration_returns_none);
    RUN_TEST(test_ble_velocity_pid_defaults_are_safe);
    RUN_TEST(test_ble_rpm_defaults_are_safe);
    RUN_TEST(test_ble_calibration_default_rotations_zero);
    RUN_TEST(test_ble_calibration_default_left_side_true);
    RUN_TEST(test_ble_existing_pid_consumer_still_works);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e confirmar GREEN**

```bash
pio test -e native -f test_ble_tuner
```

Expected: `8 Tests 0 Failures 0 Ignored`

- [ ] **Step 3: Commit**

```bash
git add test/test_ble_tuner/test_ble_tuner.cpp
git commit -m "test(ble): add smoke tests for new consumers and default values"
```

---

## Task D4: Atualizar main.cpp para aplicar params e enviar telemetria estendida

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Localizar `taskComm()` (linha ~100)**

```bash
grep -n "static void taskComm" src/main.cpp
```

- [ ] **Step 2: Adicionar handlers para os 3 novos comandos dentro de `taskComm`**

Localizar o bloco existente:
```cpp
        // Aplica parâmetros de velocidade recebidos via BLE
        if (ble.hasNewSpeed()) {
            SpeedParams s = ble.getSpeed();
            taskENTER_CRITICAL(&lineFollowerMux);
            lineFollower.setSpeed(s.minSpeed, s.baseSpeed, s.maxSpeed, s.threshold);
            taskEXIT_CRITICAL(&lineFollowerMux);
        }
```

Adicionar **logo abaixo**:
```cpp
        // Aplica ganhos do PID interno de velocidade
        if (ble.hasNewVelocityPID()) {
            VelocityPIDParams p = ble.getVelocityPID();
            taskENTER_CRITICAL(&lineFollowerMux);
            cascade.setInnerGains(p.kp, p.ki, p.kd);
            taskEXIT_CRITICAL(&lineFollowerMux);
        }

        // Aplica novo MAX_RPM (clamp do cascade)
        if (ble.hasNewRpm()) {
            RpmParams r = ble.getRpm();
            taskENTER_CRITICAL(&lineFollowerMux);
            cascade.setMaxRpm(r.maxRpm);
            // baseRpm vai pro setpoint do cascade — armazenamos em variável global
            g_baseRpm = r.baseRpm;
            taskEXIT_CRITICAL(&lineFollowerMux);
        }

        // Comando de calibração de encoder
        if (ble.hasNewCalibration()) {
            CalibrationCmd c = ble.getCalibration();
            VelocityEstimator& target = c.leftSide ? velL : velR;
            if (c.op == CalibrationCmd::Op::START) {
                target.startCalibration();
                beep(50);
            } else if (c.op == CalibrationCmd::Op::FINISH) {
                bool ok = target.finishCalibration(c.rotations);
                beep(ok ? 200 : 500);
                // Envia confirmação via notifyInfo
                char buf[40];
                snprintf(buf, sizeof(buf), "%.2f", target.getEffectivePPR());
                ble.notifyInfo(c.leftSide ? "CAL_L" : "CAL_R",
                               ok ? "OK" : "FAIL", buf);
            }
        }
```

- [ ] **Step 3: Adicionar variável global `g_baseRpm` e dar default**

Localizar a seção de estado compartilhado (após `static TelemetrySnapshot g_snap;`):
```cpp
static TelemetrySnapshot   g_snap;
static portMUX_TYPE        g_snapMux        = portMUX_INITIALIZER_UNLOCKED;
```

Adicionar **logo abaixo**:
```cpp
static volatile float      g_baseRpm        = BASE_RPM_DEFAULT;  // mutável via BLE
```

- [ ] **Step 4: Usar `g_baseRpm` dentro de `case RUNNING`**

Localizar no bloco RUNNING:
```cpp
            cascade.compute(correctionRpm, BASE_RPM_DEFAULT,
                            velL.getRPM(), velR.getRPM(),
                            leftPwm, rightPwm);
```

Substituir por:
```cpp
            cascade.compute(correctionRpm, g_baseRpm,
                            velL.getRPM(), velR.getRPM(),
                            leftPwm, rightPwm);
```

E no snapshot, atualizar setpoints:
```cpp
                g_snap.setpointL = BASE_RPM_DEFAULT + correctionRpm;
                g_snap.setpointR = BASE_RPM_DEFAULT - correctionRpm;
```

Substituir por:
```cpp
                g_snap.setpointL = g_baseRpm + correctionRpm;
                g_snap.setpointR = g_baseRpm - correctionRpm;
```

- [ ] **Step 5: Atualizar chamada de `notifyTelemetry` com os 4 novos campos**

Localizar dentro de `taskComm`:
```cpp
            ble.notifyTelemetry(
                snap.pos, snap.corr, snap.vL, snap.vR, snap.dtUs,
                snap.sensorsPct, SENSOR_COUNT,
                batPct, bestLap, hasLap, laps, nLaps
            );
```

Substituir por:
```cpp
            ble.notifyTelemetry(
                snap.pos, snap.corr, snap.vL, snap.vR, snap.dtUs,
                snap.rpmL, snap.rpmR, snap.setpointL, snap.setpointR,
                snap.sensorsPct, SENSOR_COUNT,
                batPct, bestLap, hasLap, laps, nLaps
            );
```

- [ ] **Step 6: Compilar — agora deve dar SUCCESS**

```bash
pio run -e esp32dev 2>&1 | tail -5
```

Expected: build successful.

- [ ] **Step 7: Rodar todos os testes nativos — sem regressão**

```bash
pio test -e native 2>&1 | tail -3
```

Expected: todos os suites passam.

- [ ] **Step 8: Commit**

```bash
git add src/main.cpp
git commit -m "feat(main): consume new BLE commands and emit extended telemetry"
```

---

## Task D5: Estender dashboard JS — parser de telemetria

**Files:**
- Modify: `lfr-js.js`

- [ ] **Step 1: Localizar a estrutura `state`**

```bash
grep -n "^const state" lfr-js.js
```

- [ ] **Step 2: Adicionar campos no `state` para os novos parâmetros**

Localizar:
```js
const state = {
  running: true,
  pid: { kp: 3.0, ki: 0.0, kd: 12.0 },
  spd: { base: 160, min: 60, max: 230, thrs: 0.6 },
  battery: 72.0,
  bestLap: null,
  lapTimes: [],
  logLines: [],
  chartPts: { pos: [], corr: [], vL: [], vR: [] },
  t: 0,
  lastRaf: 0,
};
```

Substituir por:
```js
const state = {
  running: true,
  pid:  { kp: 3.0, ki: 0.0, kd: 12.0 },
  pidv: { kp: 0.8, ki: 0.0, kd: 0.05 },          // PID interno velocidade
  spd:  { base: 160, min: 60, max: 230, thrs: 0.6 },
  rpm:  { max: 1200, base: 600 },                // novos limites de RPM
  battery: 72.0,
  bestLap: null,
  lapTimes: [],
  logLines: [],
  chartPts: { pos: [], corr: [], vL: [], vR: [], rpmL: [], rpmR: [], spL: [], spR: [] },
  t: 0,
  lastRaf: 0,
};
```

- [ ] **Step 3: Atualizar `pushPoint(frame)` para incluir os novos campos**

Localizar:
```js
function pushPoint(frame) {
  const keys = ['pos', 'corr', 'vL', 'vR'];
  const vals  = [frame.pos, frame.corr, frame.vL, frame.vR];
  keys.forEach(function(k, i) {
    state.chartPts[k].push(parseFloat(vals[i].toFixed(1)));
    if (state.chartPts[k].length > CHART_WIN) state.chartPts[k].shift();
  });
  const ce = document.getElementById('chartEmpty');
  if (ce) ce.style.display = 'none';
}
```

Substituir por:
```js
function pushPoint(frame) {
  const keys = ['pos', 'corr', 'vL', 'vR', 'rpmL', 'rpmR', 'spL', 'spR'];
  const vals = [
    frame.pos,  frame.corr, frame.vL,   frame.vR,
    frame.rpmL ?? 0, frame.rpmR ?? 0, frame.spL ?? 0, frame.spR ?? 0,
  ];
  keys.forEach(function(k, i) {
    state.chartPts[k].push(parseFloat(vals[i].toFixed(1)));
    if (state.chartPts[k].length > CHART_WIN) state.chartPts[k].shift();
  });
  const ce = document.getElementById('chartEmpty');
  if (ce) ce.style.display = 'none';
}
```

- [ ] **Step 4: Adicionar parser BLE para os novos campos (se houver função de recepção)**

Localizar a função que processa mensagens BLE recebidas. Se a função for `onTelemetryReceived(json)` (ou similar), garantir que ela mapeia `rpmL`, `rpmR`, `spL`, `spR` para o frame antes de chamar `pushPoint`.

Buscar:
```bash
grep -n "rpmL\|onTelemetry\|JSON.parse" lfr-js.js
```

Se houver um parser que faz `frame = JSON.parse(s)` e já passa o `frame` adiante, **nenhuma mudança extra é necessária** — o `pushPoint` já está preparado.

Se NÃO houver, adicionar wrapper logo após `genFrame()`:
```js
// Aceita frame vindo do BLE — já em formato JSON nativo
function applyTelemetryFrame(rawJson) {
  let f;
  try { f = JSON.parse(rawJson); } catch (e) { return; }
  if (!f || f.t !== 'tel') return;
  pushPoint(f);
  updateMetrics(f);
}
window.__lfr_applyTelemetry = applyTelemetryFrame;  // exposto para handler BLE
```

- [ ] **Step 5: Salvar e abrir no navegador para checar console errors**

(Manual — não há build automatizável para o JS aqui.)

- [ ] **Step 6: Commit**

```bash
git add lfr-js.js
git commit -m "feat(dashboard): parse extended telemetry (rpmL/rpmR/spL/spR)"
```

---

## Task D6: Adicionar sliders e botão de calibração no dashboard

**Files:**
- Modify: `lfr-js.js`

- [ ] **Step 1: Adicionar função para enviar comandos BLE no formato JSON**

Adicionar ao final do arquivo (antes de qualquer outro `})();` se houver IIFE):

```js
// ─── Envio de comandos BLE ────────────────────────────────────────────────
// Espera-se que window.__lfr_bleWrite(jsonString) esteja implementado pelo
// adaptador BLE (Web Bluetooth) responsável pela conexão. Se não existir,
// vira no-op silencioso (modo simulação).
function sendCmd(obj) {
  const s = JSON.stringify(obj);
  if (typeof window.__lfr_bleWrite === 'function') {
    window.__lfr_bleWrite(s);
  } else {
    console.log('[sim] sendCmd', s);
  }
}

function sendVelocityPID() {
  sendCmd({ t: 'pidv', kp: state.pidv.kp, ki: state.pidv.ki, kd: state.pidv.kd });
}

function sendRpmParams() {
  sendCmd({ t: 'rpm', max: state.rpm.max, base: state.rpm.base });
}

function startEncoderCalibration(side, rotations) {
  sendCmd({ t: 'cal', op: 'start', side: side, rot: rotations });
}

function finishEncoderCalibration(side, rotations) {
  sendCmd({ t: 'cal', op: 'finish', side: side, rot: rotations });
}

// Exporta para uso por handlers de UI (botões/sliders)
window.__lfr_sendVelocityPID         = sendVelocityPID;
window.__lfr_sendRpmParams           = sendRpmParams;
window.__lfr_startEncoderCalibration = startEncoderCalibration;
window.__lfr_finishEncoderCalibration = finishEncoderCalibration;
```

- [ ] **Step 2: Adicionar listeners para os novos sliders (se HTML existir)**

Localizar funções de inicialização de sliders no JS (busque por `addEventListener.*input` ou `slider`):

```bash
grep -n "addEventListener.*input\|slider" lfr-js.js
```

Se houver helper genérico `bindSlider(id, target, key, sendFn)`, adicionar chamadas para:
- `bindSlider('sl-kp-vel',  state.pidv, 'kp', sendVelocityPID)`
- `bindSlider('sl-ki-vel',  state.pidv, 'ki', sendVelocityPID)`
- `bindSlider('sl-kd-vel',  state.pidv, 'kd', sendVelocityPID)`
- `bindSlider('sl-rpm-max', state.rpm,  'max', sendRpmParams)`
- `bindSlider('sl-rpm-base',state.rpm,  'base', sendRpmParams)`

Se NÃO houver helper genérico, adicionar bloco inline ao final do arquivo:
```js
function bindNumericSlider(id, target, key, onChange) {
  const el = document.getElementById(id);
  if (!el) return;
  el.value = target[key];
  el.addEventListener('input', () => {
    target[key] = parseFloat(el.value);
    if (onChange) onChange();
  });
}

document.addEventListener('DOMContentLoaded', () => {
  bindNumericSlider('sl-kp-vel',   state.pidv, 'kp', sendVelocityPID);
  bindNumericSlider('sl-ki-vel',   state.pidv, 'ki', sendVelocityPID);
  bindNumericSlider('sl-kd-vel',   state.pidv, 'kd', sendVelocityPID);
  bindNumericSlider('sl-rpm-max',  state.rpm,  'max', sendRpmParams);
  bindNumericSlider('sl-rpm-base', state.rpm,  'base', sendRpmParams);

  const btnCalL = document.getElementById('btn-cal-l');
  const btnCalR = document.getElementById('btn-cal-r');
  if (btnCalL) btnCalL.addEventListener('click', () => calibrateModal('L'));
  if (btnCalR) btnCalR.addEventListener('click', () => calibrateModal('R'));
});

function calibrateModal(side) {
  const n = window.prompt(`Calibrar encoder ${side}: quantas voltas vai dar?`, '10');
  if (!n) return;
  const rot = parseInt(n, 10);
  if (!Number.isFinite(rot) || rot <= 0) return;
  startEncoderCalibration(side, rot);
  window.alert(`Gire ${rot} voltas manualmente. Clique OK quando terminar.`);
  finishEncoderCalibration(side, rot);
}
```

- [ ] **Step 3: Commit**

```bash
git add lfr-js.js
git commit -m "feat(dashboard): add velocity PID sliders, RPM params and encoder cal modal"
```

---

## Task D7: Documentar protocolo BLE atualizado

**Files:**
- Modify: `src/comm/BLETuner.h` (docstring superior)

- [ ] **Step 1: Localizar o bloco de comentário do protocolo (linhas 22-33)**

```bash
grep -n "Protocolo (espelha" src/comm/BLETuner.h
```

- [ ] **Step 2: Substituir o bloco de comentário pela versão completa**

Localizar:
```cpp
/**
 * BLE tuner + telemetria usando protocolo JSON de 2 características.
 *
 * Protocolo (espelha BLE GATT do ESP32):
 *   0xABCD — Telemetry (Notify, ESP32→App):
 *     {"t":"info","name":…,"fw":…,"mode":…}
 *     {"t":"tel","pos":…,"corr":…,"vL":…,"vR":…,"dt":…,"s":[…],"bat":…,"lap":…,"laps":[…]}
 *   0xABCE — Command (Write, App→ESP32):
 *     {"t":"pid","kp":…,"ki":…,"kd":…}
 *     {"t":"spd","base":…,"min":…,"max":…,"thrs":…}
 *     {"t":"start"} / {"t":"stop"} / {"t":"reset"}
 */
```

Substituir por:
```cpp
/**
 * BLE tuner + telemetria usando protocolo JSON de 2 características.
 *
 * Protocolo (espelha BLE GATT do ESP32):
 *
 *   0xABCD — Telemetry (Notify, ESP32→App):
 *     {"t":"info","name":…,"fw":…,"mode":…}
 *     {"t":"tel","pos":…,"corr":…,"vL":…,"vR":…,"dt":…,
 *      "rpmL":…,"rpmR":…,"spL":…,"spR":…,
 *      "s":[…],"bat":…,"lap":…,"laps":[…]}
 *
 *   0xABCE — Command (Write, App→ESP32):
 *     {"t":"pid","kp":…,"ki":…,"kd":…}            — PID externo (posição da linha)
 *     {"t":"pidv","kp":…,"ki":…,"kd":…}           — PID interno (velocidade dos motores)
 *     {"t":"spd","base":…,"min":…,"max":…,"thrs":…}   — velocidade em PWM (legado)
 *     {"t":"rpm","max":…,"base":…}                — velocidade em RPM (novo, cascade)
 *     {"t":"cal","op":"start","side":"L"|"R","rot":N}  — inicia cal de encoder
 *     {"t":"cal","op":"finish","side":"L"|"R","rot":N} — finaliza e salva PPR
 *     {"t":"start"} / {"t":"stop"} / {"t":"reset"}
 */
```

- [ ] **Step 3: Verificar que ainda compila**

```bash
pio run -e esp32dev 2>&1 | tail -3
```

Expected: build successful.

- [ ] **Step 4: Commit**

```bash
git add src/comm/BLETuner.h
git commit -m "docs(ble): document complete extended protocol (pidv, rpm, cal)"
```

---

## Critério de "Done" — Fase D

- [ ] `pio test -e native -f test_ble_tuner` → 8/8 PASSED
- [ ] `pio test -e native` → todos os suites passam (sem regressão)
- [ ] `pio run -e esp32dev` → SUCCESS
- [ ] Dashboard JS parseia frames com `rpmL`, `rpmR`, `spL`, `spR` sem erro
- [ ] Sliders de PID velocidade enviam `{"t":"pidv",…}` por BLE
- [ ] Botão "Calibrar Encoder" envia `start` → `finish` em sequência
- [ ] Bench test ESP32 + dashboard: ao mover slider Kp_vel, valor é refletido em `cascade.setInnerGains()` (sem reboot)
- [ ] Docstring do BLETuner.h reflete protocolo completo
