#include "comm/BLETuner.h"
#include <cstring>
#include <cstdlib>

#ifndef NATIVE_BUILD
#include <NimBLEDevice.h>

BLETuner* BLETuner::_instance = nullptr;

BLETuner::BLETuner() {
    _instance = this;
}

void BLETuner::WriteCallback::onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& /*connInfo*/) {
    if (_instance) _instance->_onWrite(pChar);
}

void BLETuner::ServerCallback::onConnect(NimBLEServer* /*pServer*/, NimBLEConnInfo& /*connInfo*/) {
    if (_instance) {
        taskENTER_CRITICAL(&_instance->_mux);
        _instance->_clientConnected = true;
        taskEXIT_CRITICAL(&_instance->_mux);
    }
}
void BLETuner::ServerCallback::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& /*connInfo*/, int /*reason*/) {
    if (_instance) {
        taskENTER_CRITICAL(&_instance->_mux);
        _instance->_clientConnected = false;
        taskEXIT_CRITICAL(&_instance->_mux);
    }
    pServer->startAdvertising();  // reanunciar após desconexão
}

void BLETuner::begin(const char* deviceName) {
    NimBLEDevice::init(deviceName);
    NimBLEServer* pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&_serverCallback);

    NimBLEService* pService = pServer->createService(SERVICE_UUID);

    auto makeChar = [&](const char* uuid) -> NimBLECharacteristic* {
        return pService->createCharacteristic(
            uuid,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY
        );
    };

    _charKp        = makeChar(CHAR_KP_UUID);
    _charKi        = makeChar(CHAR_KI_UUID);
    _charKd        = makeChar(CHAR_KD_UUID);
    _charSpeed     = makeChar(CHAR_SPEED_UUID);
    _charTelemetry = makeChar(CHAR_TELEMETRY_UUID);

    // Valores iniciais legíveis
    _charKp->setValue(std::to_string(_pid.kp));
    _charKi->setValue(std::to_string(_pid.ki));
    _charKd->setValue(std::to_string(_pid.kd));

    _charKp->setCallbacks(&_writeCallback);
    _charKi->setCallbacks(&_writeCallback);
    _charKd->setCallbacks(&_writeCallback);
    _charSpeed->setCallbacks(&_writeCallback);

    pService->start();
    NimBLEDevice::startAdvertising();
}

void BLETuner::_onWrite(NimBLECharacteristic* pChar) {
    std::string val = pChar->getValue();
    if (val.empty()) return;

    if (pChar == _charKp) {
        taskENTER_CRITICAL(&_mux);
        _pid.kp = std::atof(val.c_str());
        _newPID = true;
        taskEXIT_CRITICAL(&_mux);
    } else if (pChar == _charKi) {
        taskENTER_CRITICAL(&_mux);
        _pid.ki = std::atof(val.c_str());
        _newPID = true;
        taskEXIT_CRITICAL(&_mux);
    } else if (pChar == _charKd) {
        taskENTER_CRITICAL(&_mux);
        _pid.kd = std::atof(val.c_str());
        _newPID = true;
        taskEXIT_CRITICAL(&_mux);
    } else if (pChar == _charSpeed) {
        // formato CSV: "base,min,max,threshold"
        char buf[64];
        strncpy(buf, val.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        int   base = 160, minS = 60, maxS = 230;
        float thr  = 0.6f;
        char* tok = strtok(buf, ",");
        if (tok) base = atoi(tok);
        tok = strtok(nullptr, ",");
        if (tok) minS = atoi(tok);
        tok = strtok(nullptr, ",");
        if (tok) maxS = atoi(tok);
        tok = strtok(nullptr, ",");
        if (tok) thr = std::atof(tok);
        taskENTER_CRITICAL(&_mux);
        _speed.baseSpeed = base;
        _speed.minSpeed  = minS;
        _speed.maxSpeed  = maxS;
        _speed.threshold = thr;
        _newSpeed = true;
        taskEXIT_CRITICAL(&_mux);
    }
}

void BLETuner::update() {
    // NimBLE é event-driven — sem polling necessário
}

void BLETuner::notifyStatus(const char* csv) {
    taskENTER_CRITICAL(&_mux);
    bool connected = _clientConnected;
    taskEXIT_CRITICAL(&_mux);
    if (connected && _charTelemetry) {
        _charTelemetry->setValue(csv);
        _charTelemetry->notify();
    }
}

#else
// Stubs native
BLETuner::BLETuner() {}
void BLETuner::begin(const char*) {}
void BLETuner::update() {}
void BLETuner::notifyStatus(const char*) {}
#endif

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
