#include "comm/BLETuner.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>

#ifndef NATIVE_BUILD
#include <NimBLEDevice.h>

BLETuner* BLETuner::_instance = nullptr;

// ── Callbacks BLE ──────────────────────────────────────────────────────────

void BLETuner::WriteCallback::onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& /*connInfo*/) {
    if (_instance) _instance->_onWrite(pChar->getValue());
}

void BLETuner::ServerCallback::onConnect(NimBLEServer* /*pServer*/, NimBLEConnInfo& /*connInfo*/) {
    if (!_instance) return;
    taskENTER_CRITICAL(&_instance->_mux);
    _instance->_clientConnected = true;
    _instance->_newConn = true;
    taskEXIT_CRITICAL(&_instance->_mux);
}

void BLETuner::ServerCallback::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& /*connInfo*/, int /*reason*/) {
    if (_instance) {
        taskENTER_CRITICAL(&_instance->_mux);
        _instance->_clientConnected = false;
        taskEXIT_CRITICAL(&_instance->_mux);
    }
    pServer->startAdvertising();
}

// ── begin ──────────────────────────────────────────────────────────────────

void BLETuner::begin(const char* deviceName) {
    NimBLEDevice::init(deviceName);
    NimBLEServer* pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&_serverCallback);

    NimBLEService* pService = pServer->createService(SERVICE_UUID);

    _charTel = pService->createCharacteristic(CHAR_TEL_UUID,
                   NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    _charCmd = pService->createCharacteristic(CHAR_CMD_UUID,
                   NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    _charCmd->setCallbacks(&_writeCallback);

    pService->start();
    NimBLEDevice::startAdvertising();
}

// ── JSON parser (recepção de comandos) ─────────────────────────────────────

static float jsonFloat(const char* json, const char* key, float def) {
    char search[32];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char* p = strstr(json, search);
    if (!p) return def;
    p += strlen(search);
    while (*p == ' ') p++;
    return static_cast<float>(std::atof(p));
}

static int jsonInt(const char* json, const char* key, int def) {
    return static_cast<int>(jsonFloat(json, key, static_cast<float>(def)));
}

void BLETuner::_onWrite(const std::string& val) {
    if (val.empty()) return;
    const char* json = val.c_str();

    // Extrai campo "t"
    const char* tp = strstr(json, "\"t\":\"");
    if (!tp) return;
    tp += 5;  // avança após "t":"

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
}

// ── notifyInfo ─────────────────────────────────────────────────────────────

void BLETuner::notifyInfo(const char* name, const char* fw, const char* mode) {
    taskENTER_CRITICAL(&_mux);
    bool ok = _clientConnected && _charTel;
    taskEXIT_CRITICAL(&_mux);
    if (!ok) return;

    char buf[128];
    snprintf(buf, sizeof(buf),
        "{\"t\":\"info\",\"name\":\"%s\",\"fw\":\"%s\",\"mode\":\"%s\"}",
        name, fw, mode);
    _charTel->setValue(buf);
    _charTel->notify();
}

// ── notifyTelemetry ────────────────────────────────────────────────────────

void BLETuner::notifyTelemetry(float pos, float corr, int vL, int vR, uint32_t dtUs,
                                const float* sensors, int nSensors, float batPct,
                                float bestLapSec, bool hasLap,
                                const float* laps, int nLaps) {
    taskENTER_CRITICAL(&_mux);
    bool ok = _clientConnected && _charTel;
    taskEXIT_CRITICAL(&_mux);
    if (!ok) return;

    // Monta array de sensores: [v0,v1,...,v7]
    char sarr[96] = "[";
    for (int i = 0; i < nSensors && i < 8; i++) {
        char tmp[12];
        snprintf(tmp, sizeof(tmp), "%.1f", sensors[i]);
        if (i > 0) strncat(sarr, ",", sizeof(sarr) - strlen(sarr) - 1);
        strncat(sarr, tmp, sizeof(sarr) - strlen(sarr) - 1);
    }
    strncat(sarr, "]", sizeof(sarr) - strlen(sarr) - 1);

    // Monta array de voltas: [t0,t1,...,t4]
    char larr[80] = "[";
    for (int i = 0; i < nLaps && i < 5; i++) {
        char tmp[12];
        snprintf(tmp, sizeof(tmp), "%.2f", laps[i]);
        if (i > 0) strncat(larr, ",", sizeof(larr) - strlen(larr) - 1);
        strncat(larr, tmp, sizeof(larr) - strlen(larr) - 1);
    }
    strncat(larr, "]", sizeof(larr) - strlen(larr) - 1);

    // Monta valor de melhor volta (null se não disponível)
    char lapVal[16];
    if (hasLap) snprintf(lapVal, sizeof(lapVal), "%.2f", bestLapSec);
    else        snprintf(lapVal, sizeof(lapVal), "null");

    char buf[320];
    snprintf(buf, sizeof(buf),
        "{\"t\":\"tel\",\"pos\":%.1f,\"corr\":%.1f,\"vL\":%d,\"vR\":%d,"
        "\"dt\":%lu,\"s\":%s,\"bat\":%.1f,\"lap\":%s,\"laps\":%s}",
        pos, corr, vL, vR, static_cast<unsigned long>(dtUs), sarr, batPct, lapVal, larr);

    _charTel->setValue(buf);
    _charTel->notify();
}

// ── update ─────────────────────────────────────────────────────────────────

void BLETuner::update() {
    // NimBLE é event-driven — sem polling necessário nesta implementação
}

#else
// ── Stubs native (testes unitários sem hardware) ───────────────────────────
BLETuner::BLETuner() {}
void BLETuner::begin(const char*) {}
void BLETuner::update() {}
void BLETuner::notifyInfo(const char*, const char*, const char*) {}
void BLETuner::notifyTelemetry(float, float, int, int, uint32_t,
                                const float*, int, float,
                                float, bool, const float*, int) {}
#endif

// ── API de consumo de flags (ambos os ambientes) ───────────────────────────

bool BLETuner::hasNewPID() const {
#ifndef NATIVE_BUILD
    taskENTER_CRITICAL(&const_cast<BLETuner*>(this)->_mux);
#endif
    bool v = _newPID;
#ifndef NATIVE_BUILD
    taskEXIT_CRITICAL(&const_cast<BLETuner*>(this)->_mux);
#endif
    return v;
}

bool BLETuner::hasNewSpeed() const {
#ifndef NATIVE_BUILD
    taskENTER_CRITICAL(&const_cast<BLETuner*>(this)->_mux);
#endif
    bool v = _newSpeed;
#ifndef NATIVE_BUILD
    taskEXIT_CRITICAL(&const_cast<BLETuner*>(this)->_mux);
#endif
    return v;
}

PIDParams BLETuner::getPID() {
#ifndef NATIVE_BUILD
    taskENTER_CRITICAL(&_mux);
#endif
    PIDParams p = _pid;
    _newPID = false;
#ifndef NATIVE_BUILD
    taskEXIT_CRITICAL(&_mux);
#endif
    return p;
}

SpeedParams BLETuner::getSpeed() {
#ifndef NATIVE_BUILD
    taskENTER_CRITICAL(&_mux);
#endif
    SpeedParams s = _speed;
    _newSpeed = false;
#ifndef NATIVE_BUILD
    taskEXIT_CRITICAL(&_mux);
#endif
    return s;
}

bool BLETuner::consumeStart() {
#ifndef NATIVE_BUILD
    taskENTER_CRITICAL(&_mux);
#endif
    bool v = _cmdStart;
    _cmdStart = false;
#ifndef NATIVE_BUILD
    taskEXIT_CRITICAL(&_mux);
#endif
    return v;
}

bool BLETuner::consumeStop() {
#ifndef NATIVE_BUILD
    taskENTER_CRITICAL(&_mux);
#endif
    bool v = _cmdStop;
    _cmdStop = false;
#ifndef NATIVE_BUILD
    taskEXIT_CRITICAL(&_mux);
#endif
    return v;
}

bool BLETuner::consumeReset() {
#ifndef NATIVE_BUILD
    taskENTER_CRITICAL(&_mux);
#endif
    bool v = _cmdReset;
    _cmdReset = false;
#ifndef NATIVE_BUILD
    taskEXIT_CRITICAL(&_mux);
#endif
    return v;
}

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
