# Fase D — Reconciled Implementation Spec
# Generated: 2026-05-31 by Architect (Wave 1)
# Based on plan: docs/superpowers/plans/2026-05-20-fase-d-ble-tuner-dashboard.md
# Code snapshot: main branch, last commit f72b387

## BLOCKERS — resolve before writing any code

None. All methods referenced by the plan exist in the current codebase:

- `CascadeController::setInnerGains(float, float, float)` — EXISTS (line 39 of CascadeController.h)
- `CascadeController::setMaxRpm(float)` — EXISTS (line 41 of CascadeController.h)
- `VelocityEstimator::startCalibration()` — EXISTS (line 47 of VelocityEstimator.h)
- `VelocityEstimator::finishCalibration(int)` — EXISTS (line 48 of VelocityEstimator.h)
- `VelocityEstimator::getEffectivePPR()` — EXISTS (line 38 of VelocityEstimator.h)

IMPORTANT BLOCKER NOTE for D4 Step 2 (notifyInfo calibration confirmation):
`BLETuner::notifyInfo` has signature `notifyInfo(const char* name, const char* fw, const char* mode)`
which maps to (name, fw, mode) — not (name, status, value) as the plan implies.
The plan's D4 call `ble.notifyInfo(c.leftSide ? "CAL_L" : "CAL_R", ok ? "OK" : "FAIL", buf)` will
COMPILE because all params are `const char*`, but the JSON emitted will be:
  `{"t":"info","name":"CAL_L","fw":"OK","mode":"1.25"}`
This is semantically wrong (fw/mode fields are misused) but functional for this phase.
The Coder must document this as a known deviation and NOT add a new overload of notifyInfo
(that would break the existing native stub). Acceptable as-is for Fase D.

---

## Summary of changes per file

| File | Changes |
|------|---------|
| src/comm/BLETuner.h | 5 edits: 3 new structs, 6 new public methods, updated notifyTelemetry signature, extended private block, updated docstring |
| src/comm/BLETuner.cpp | 5 edits: extend _onWrite parser, update notifyTelemetry signature + snprintf + buffer size, update native stub, add 6 new consumer implementations |
| src/main.cpp | 5 edits: add g_baseRpm global, add 3 new BLE command handlers in taskComm, replace BASE_RPM_DEFAULT with g_baseRpm in cascade.compute call, replace BASE_RPM_DEFAULT with g_baseRpm in setpointL/R snapshot, extend notifyTelemetry call site with 4 new args |
| test/test_ble_tuner/test_ble_tuner.cpp | CREATE new file (8 tests) |
| lfr-js.js | 4 edits: extend state object, extend pushPoint, add sendCmd/send* functions, add bindNumericSlider + DOMContentLoaded block + calibrateModal |

---

## FILE: src/comm/BLETuner.h

### Change H-1: Add 3 new structs after SpeedParams

ANCHOR (exact current text, lines 15-20):
```cpp
struct SpeedParams {
    int   baseSpeed  = 160;
    int   minSpeed   = 60;
    int   maxSpeed   = 230;
    float threshold  = 0.6f;
};
```

Insert immediately after the closing `};` of SpeedParams (before the `/**` docblock):
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
    bool leftSide  = true;
};
```

DRIFT NOTE: Plan's Step 1 grep expected structs at lines 9 and 15 — confirmed correct.

### Change H-2: Add 6 new public method declarations

ANCHOR (exact current text, lines 41-49):
```cpp
    // Recepção de comandos — consumir no loop Core 1
    bool hasNewPID()   const;
    bool hasNewSpeed() const;
    PIDParams   getPID();    // consome flag
    SpeedParams getSpeed();  // consome flag
    bool consumeStart();
    bool consumeStop();
    bool consumeReset();
    bool consumeNewConnection();  // true uma vez quando cliente conecta
```

Insert immediately after `bool consumeNewConnection();  // true uma vez quando cliente conecta`:
```cpp
    // Novos comandos — Fase D
    bool hasNewVelocityPID() const;
    bool hasNewRpm()         const;
    bool hasNewCalibration() const;
    VelocityPIDParams getVelocityPID();  // consome flag
    RpmParams         getRpm();          // consome flag
    CalibrationCmd    getCalibration();  // consome flag
```

### Change H-3: Update notifyTelemetry declaration

ANCHOR (exact current text, lines 53-56):
```cpp
    void notifyTelemetry(float pos, float corr, int vL, int vR, uint32_t dtUs,
                         const float* sensors, int nSensors, float batPct,
                         float bestLapSec, bool hasLap,
                         const float* laps, int nLaps);
```

Replace with:
```cpp
    void notifyTelemetry(float pos, float corr, int vL, int vR, uint32_t dtUs,
                         float rpmL, float rpmR, float spL, float spR,
                         const float* sensors, int nSensors, float batPct,
                         float bestLapSec, bool hasLap,
                         const float* laps, int nLaps);
```

### Change H-4: Extend private block

ANCHOR (exact current text, lines 63-71):
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

Replace with:
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

### Change H-5: Update docstring protocol block

ANCHOR (exact current text, lines 22-33):
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

Replace with:
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

---

## FILE: src/comm/BLETuner.cpp

### Change C-1: Replace the _onWrite if/else chain

ANCHOR (exact current text, lines 80-98):
```cpp
    taskENTER_CRITICAL(&_mux);
    if (strncmp(tp, "pid", 3) == 0) {
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
    } else if (strncmp(tp, "start", 5) == 0) {
        _cmdStart = true;
    } else if (strncmp(tp, "stop", 4) == 0) {
        _cmdStop = true;
    } else if (strncmp(tp, "reset", 5) == 0) {
        _cmdReset = true;
    }
    taskEXIT_CRITICAL(&_mux);
```

Replace with (pidv branch MUST come before pid branch — prefix match order):
```cpp
    taskENTER_CRITICAL(&_mux);
    if (strncmp(tp, "pidv", 4) == 0) {
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
        const char* op = strstr(json, "\"op\":\"");
        if (op) {
            op += 6;
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
    taskEXIT_CRITICAL(&_mux);
```

DRIFT NOTE: The plan's Step 2 anchor matches exactly. No line number drift.

### Change C-2: Update notifyTelemetry signature + buffer size

ANCHOR (exact current text, lines 119-122):
```cpp
void BLETuner::notifyTelemetry(float pos, float corr, int vL, int vR, uint32_t dtUs,
                                const float* sensors, int nSensors, float batPct,
                                float bestLapSec, bool hasLap,
                                const float* laps, int nLaps) {
```

Replace with:
```cpp
void BLETuner::notifyTelemetry(float pos, float corr, int vL, int vR, uint32_t dtUs,
                                float rpmL, float rpmR, float spL, float spR,
                                const float* sensors, int nSensors, float batPct,
                                float bestLapSec, bool hasLap,
                                const float* laps, int nLaps) {
```

ANCHOR (exact current text, line 153):
```cpp
    char buf[320];
```

Replace with:
```cpp
    char buf[400];
```

### Change C-3: Update snprintf body in notifyTelemetry

ANCHOR (exact current text, lines 154-157):
```cpp
    snprintf(buf, sizeof(buf),
        "{\"t\":\"tel\",\"pos\":%.1f,\"corr\":%.1f,\"vL\":%d,\"vR\":%d,"
        "\"dt\":%lu,\"s\":%s,\"bat\":%.1f,\"lap\":%s,\"laps\":%s}",
        pos, corr, vL, vR, static_cast<unsigned long>(dtUs), sarr, batPct, lapVal, larr);
```

Replace with:
```cpp
    snprintf(buf, sizeof(buf),
        "{\"t\":\"tel\",\"pos\":%.1f,\"corr\":%.1f,\"vL\":%d,\"vR\":%d,"
        "\"dt\":%lu,\"rpmL\":%.1f,\"rpmR\":%.1f,\"spL\":%.1f,\"spR\":%.1f,"
        "\"s\":%s,\"bat\":%.1f,\"lap\":%s,\"laps\":%s}",
        pos, corr, vL, vR, static_cast<unsigned long>(dtUs),
        rpmL, rpmR, spL, spR,
        sarr, batPct, lapVal, larr);
```

### Change C-4: Update native stub

ANCHOR (exact current text, lines 174-176):
```cpp
void BLETuner::notifyTelemetry(float, float, int, int, uint32_t,
                                const float*, int, float,
                                float, bool, const float*, int) {}
```

Replace with:
```cpp
void BLETuner::notifyTelemetry(float, float, int, int, uint32_t,
                                float, float, float, float,
                                const float*, int, float,
                                float, bool, const float*, int) {}
```

### Change C-5: Add 6 new consumer implementations

ANCHOR (exact current text, line 276, end of file):
```cpp
bool BLETuner::consumeNewConnection() {
#ifndef NATIVE_BUILD
    taskENTER_CRITICAL(&_mux);
#endif
    bool v = _newConn;
    _newConn = false;
#ifndef NATIVE_BUILD
    taskEXIT_CRITICAL(&_mux);
#endif
    return v;
}
```

Append after the closing `}` of `consumeNewConnection()`:
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
    _cal.op = CalibrationCmd::Op::NONE;
#ifndef NATIVE_BUILD
    taskEXIT_CRITICAL(&_mux);
#endif
    return c;
}
```

---

## FILE: src/main.cpp

### Change M-1: Add g_baseRpm global

ANCHOR (exact current text, lines 95-96):
```cpp
static TelemetrySnapshot   g_snap;
static portMUX_TYPE        g_snapMux        = portMUX_INITIALIZER_UNLOCKED;
```

Insert immediately after the `g_snapMux` line:
```cpp
static volatile float      g_baseRpm        = BASE_RPM_DEFAULT;  // mutável via BLE
```

DRIFT NOTE: The plan's anchor matches the current code exactly. `g_snapMux` is on line 96.

### Change M-2: Add 3 new BLE command handlers in taskComm

ANCHOR (exact current text, lines 165-171):
```cpp
        // Aplica parâmetros de velocidade recebidos via BLE
        if (ble.hasNewSpeed()) {
            SpeedParams s = ble.getSpeed();
            taskENTER_CRITICAL(&lineFollowerMux);
            lineFollower.setSpeed(s.minSpeed, s.baseSpeed, s.maxSpeed, s.threshold);
            taskEXIT_CRITICAL(&lineFollowerMux);
        }
```

Insert immediately after the closing `}` of the `hasNewSpeed()` block:
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
                char buf[40];
                snprintf(buf, sizeof(buf), "%.2f", target.getEffectivePPR());
                // NOTE: notifyInfo(name, fw, mode) — fw/mode fields are repurposed
                // as status/value for this confirmation message. Tracked as known
                // deviation; proper fix (new overload) is out of Fase D scope.
                ble.notifyInfo(c.leftSide ? "CAL_L" : "CAL_R",
                               ok ? "OK" : "FAIL", buf);
            }
        }
```

DRIFT NOTE: Plan D4 Step 2 anchor is `ble.hasNewSpeed()` block — confirmed matches lines 165-171.

### Change M-3: Replace BASE_RPM_DEFAULT with g_baseRpm in cascade.compute call

ANCHOR (exact current text, lines 353-355):
```cpp
            cascade.compute(correctionRpm, BASE_RPM_DEFAULT,
                            rpmLActual, rpmRActual,
                            leftPwm, rightPwm);
```

Replace with:
```cpp
            cascade.compute(correctionRpm, g_baseRpm,
                            rpmLActual, rpmRActual,
                            leftPwm, rightPwm);
```

DRIFT NOTE: Plan's Task D4 Step 4 said `velL.getRPM(), velR.getRPM()` as third/fourth args.
ACTUAL current code uses `rpmLActual, rpmRActual` (pre-validated via isfinite, lines 345-348).
The current form is CORRECT per the Wave 3 review fix (C-1 Reviewer note at line 377-379).
Do NOT revert to `velL.getRPM()/velR.getRPM()`. Only change `BASE_RPM_DEFAULT` → `g_baseRpm`.

### Change M-4: Replace BASE_RPM_DEFAULT with g_baseRpm in setpoint snapshot

ANCHOR (exact current text, lines 381-382):
```cpp
                g_snap.setpointL = BASE_RPM_DEFAULT + correctionRpm;
                g_snap.setpointR = BASE_RPM_DEFAULT - correctionRpm;
```

Replace with:
```cpp
                g_snap.setpointL = g_baseRpm + correctionRpm;
                g_snap.setpointR = g_baseRpm - correctionRpm;
```

### Change M-5: Extend notifyTelemetry call site in taskComm

ANCHOR (exact current text, lines 230-234):
```cpp
            ble.notifyTelemetry(
                snap.pos, snap.corr, snap.vL, snap.vR, snap.dtUs,
                snap.sensorsPct, SENSOR_COUNT,
                batPct, bestLap, hasLap, laps, nLaps
            );
```

Replace with:
```cpp
            ble.notifyTelemetry(
                snap.pos, snap.corr, snap.vL, snap.vR, snap.dtUs,
                snap.rpmL, snap.rpmR, snap.setpointL, snap.setpointR,
                snap.sensorsPct, SENSOR_COUNT,
                batPct, bestLap, hasLap, laps, nLaps
            );
```

DRIFT NOTE: TelemetrySnapshot already has `rpmL`, `rpmR`, `setpointL`, `setpointR` fields
(confirmed lines 88-91 of main.cpp). Field names match exactly what the plan expects
(plan uses `spL`/`spR` only in the JSON key names; the struct fields are `setpointL`/`setpointR`).

---

## FILE: test/test_ble_tuner/test_ble_tuner.cpp (CREATE)

Create this file exactly as specified in the plan's Task D3 Step 1. No drift found.
The test for `getPID()` default `p.kd` expects `12.0f` — confirmed matches `PIDParams.kd = 12.0f`
in BLETuner.h line 12.

---

## FILE: lfr-js.js

### Change J-1: Extend state object

ANCHOR (exact current text, lines 4-15):
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

Replace with:
```js
const state = {
  running: true,
  pid:  { kp: 3.0, ki: 0.0, kd: 12.0 },
  pidv: { kp: 0.8, ki: 0.0, kd: 0.05 },
  spd:  { base: 160, min: 60, max: 230, thrs: 0.6 },
  rpm:  { max: 1200, base: 600 },
  battery: 72.0,
  bestLap: null,
  lapTimes: [],
  logLines: [],
  chartPts: { pos: [], corr: [], vL: [], vR: [], rpmL: [], rpmR: [], spL: [], spR: [] },
  t: 0,
  lastRaf: 0,
};
```

DRIFT NOTE: The plan anchor matches exactly. No drift.

### Change J-2: Extend pushPoint function

ANCHOR (exact current text, lines 40-49):
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

Replace with:
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

DRIFT NOTE: Plan anchor matches exactly. The `??` (nullish coalescing) operator requires
ES2020 or later — acceptable per plan's "ES2017+" target; Chrome/Chromium on ESP32
web dashboard supports it. No change needed.

### Change J-3: Add sendCmd helpers

ANCHOR: locate end of the `bindAllSliders` function (line 470 closing `}`), insert
the new block AFTER it and BEFORE the `// ─── Toggles de Acessibilidade` comment.

Insert between line 470 `}` (end of bindAllSliders) and line 472 `// ─── Toggles`:
```js
// ─── Envio de comandos BLE ────────────────────────────────────────────────────
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

window.__lfr_sendVelocityPID          = sendVelocityPID;
window.__lfr_sendRpmParams            = sendRpmParams;
window.__lfr_startEncoderCalibration  = startEncoderCalibration;
window.__lfr_finishEncoderCalibration = finishEncoderCalibration;
```

DRIFT NOTE: Plan's D6 Step 1 says "before any `})();` if IIFE" — confirmed there is NO
wrapping IIFE in lfr-js.js; the file is module-level. Insert at end of bindAllSliders.

### Change J-4: Add bindNumericSlider, DOMContentLoaded extension, calibrateModal

ANCHOR: existing `bindAllSliders` function — it already has a `bindAllSliders` function
and a `DOMContentLoaded` listener that calls `bindAllSliders()` at line 627.
Do NOT add a second DOMContentLoaded listener (plan's D6 Step 2 assumed it might not exist).

Instead, extend the EXISTING DOMContentLoaded listener. The exact current anchor:
```js
  bindAllSliders();
  bindAllButtons();
  startLapTimer();
```

(lines 626-628 inside the DOMContentLoaded callback)

Replace with:
```js
  bindAllSliders();
  bindAllButtons();
  bindNewSliders();
  startLapTimer();
```

Then add the `bindNewSliders` function and `calibrateModal` function AFTER the `sendCmd` block
added in J-3 (i.e., before `// ─── Toggles de Acessibilidade`):
```js
function bindNumericSlider(id, target, key, onChange) {
  const el = document.getElementById(id);
  if (!el) return;
  el.value = target[key];
  el.addEventListener('input', function() {
    target[key] = parseFloat(el.value);
    if (onChange) onChange();
  });
}

function calibrateModal(side) {
  const n = window.prompt('Calibrar encoder ' + side + ': quantas voltas vai dar?', '10');
  if (!n) return;
  const rot = parseInt(n, 10);
  if (!Number.isFinite(rot) || rot <= 0) return;
  startEncoderCalibration(side, rot);
  window.alert('Gire ' + rot + ' voltas manualmente. Clique OK quando terminar.');
  finishEncoderCalibration(side, rot);
}

function bindNewSliders() {
  bindNumericSlider('sl-kp-vel',   state.pidv, 'kp', sendVelocityPID);
  bindNumericSlider('sl-ki-vel',   state.pidv, 'ki', sendVelocityPID);
  bindNumericSlider('sl-kd-vel',   state.pidv, 'kd', sendVelocityPID);
  bindNumericSlider('sl-rpm-max',  state.rpm,  'max', sendRpmParams);
  bindNumericSlider('sl-rpm-base', state.rpm,  'base', sendRpmParams);

  const btnCalL = document.getElementById('btn-cal-l');
  const btnCalR = document.getElementById('btn-cal-r');
  if (btnCalL) btnCalL.addEventListener('click', function() { calibrateModal('L'); });
  if (btnCalR) btnCalR.addEventListener('click', function() { calibrateModal('R'); });
}
```

DRIFT NOTE: The plan's D6 Step 2 suggested checking for a generic `bindSlider` helper.
The ACTUAL lfr-js.js has `bindAllSliders` (line 449) which is a specialized loop over a
config array — NOT a generic `bindSlider(id, target, key, sendFn)` helper. The plan's
"If NÃO houver helper genérico" branch applies. Use the inline approach above, but
refactored as a named `bindNewSliders()` function called from the existing DOMContentLoaded
listener rather than adding a duplicate DOMContentLoaded event (which the plan would do).

DRIFT NOTE: The plan uses arrow functions (`() => {}`) in the DOMContentLoaded block.
The existing lfr-js.js uses `function` keyword throughout (ES5 style). Coder should use
`function` keyword for consistency with the existing file style.

DRIFT NOTE: No HTML file exists in the repo to add slider/button elements. The
`getElementById` calls in `bindNewSliders` will return `null` and silently no-op —
this is acceptable per the plan (simulator mode). The Coder does NOT need to create
or modify any HTML file.

### Change J-5: Add BLE telemetry receiver function

The plan's D5 Step 4 asks whether a BLE receiver function exists. Confirmed it does NOT —
the JS file has only the simulation RAF loop. Add the receiver function AFTER the `sendCmd`
block (before `bindNumericSlider`):

```js
// ─── BLE Telemetria Receiver ──────────────────────────────────────────────────
function applyTelemetryFrame(rawJson) {
  let f;
  try { f = JSON.parse(rawJson); } catch (e) { return; }
  if (!f || f.t !== 'tel') return;
  if (typeof f.bat === 'number') state.battery = f.bat;
  if (f.laps && Array.isArray(f.laps)) {
    state.lapTimes = f.laps;
    state.bestLap  = f.lap !== null && f.lap !== undefined ? f.lap : state.bestLap;
    renderLapList();
  }
  pushPoint(f);
  updateSensors(f.sensors || []);
  updateMetrics(f);
  logFrame(f);
}
window.__lfr_applyTelemetry = applyTelemetryFrame;
```

DRIFT NOTE: The plan's minimal version only called `pushPoint` and `updateMetrics`.
The extended version above also maps battery/laps from real BLE data, which is needed
for the dashboard to be useful with real hardware. The extra 5 lines cost nothing in risk.

---

## ORDER OF IMPLEMENTATION

The Coder MUST follow this order to avoid compile errors between tasks:

1. BLETuner.h changes (H-1 through H-5) — defines new types used everywhere
2. BLETuner.cpp changes (C-1 through C-5) — implements the new API
3. test/test_ble_tuner/test_ble_tuner.cpp (CREATE) — validate with `pio test -e native`
4. main.cpp changes (M-1 through M-5) — consumes new API
5. lfr-js.js changes (J-1 through J-5) — dashboard, no compile dependency

Intermediate compile check after step 2 will produce errors in main.cpp (notifyTelemetry
signature mismatch) — this is expected and resolves in step 4.

---

## VALIDATION COMMANDS

After all changes:
```bash
pio test -e native -f test_ble_tuner   # expect: 8/8 PASSED
pio test -e native                      # expect: all suites PASSED (no regression)
pio run -e esp32dev                     # expect: SUCCESS
```
