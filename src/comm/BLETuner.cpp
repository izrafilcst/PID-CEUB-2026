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
    if (_instance) _instance->_clientConnected = true;
}
void BLETuner::ServerCallback::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& /*connInfo*/, int /*reason*/) {
    if (_instance) _instance->_clientConnected = false;
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

    if (pChar->getUUID().toString() == CHAR_KP_UUID) {
        _pid.kp = std::atof(val.c_str());
        _newPID = true;
    } else if (pChar->getUUID().toString() == CHAR_KI_UUID) {
        _pid.ki = std::atof(val.c_str());
        _newPID = true;
    } else if (pChar->getUUID().toString() == CHAR_KD_UUID) {
        _pid.kd = std::atof(val.c_str());
        _newPID = true;
    } else if (pChar->getUUID().toString() == CHAR_SPEED_UUID) {
        // formato CSV: "base,min,max,threshold"
        char buf[64];
        strncpy(buf, val.c_str(), sizeof(buf) - 1);
        char* tok = strtok(buf, ",");
        if (tok) _speed.baseSpeed = atoi(tok);
        tok = strtok(nullptr, ",");
        if (tok) _speed.minSpeed = atoi(tok);
        tok = strtok(nullptr, ",");
        if (tok) _speed.maxSpeed = atoi(tok);
        tok = strtok(nullptr, ",");
        if (tok) _speed.threshold = std::atof(tok);
        _newSpeed = true;
    }
}

void BLETuner::update() {
    // NimBLE é event-driven — sem polling necessário
}

void BLETuner::notifyStatus(const char* csv) {
    if (_clientConnected && _charTelemetry) {
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

bool BLETuner::hasNewPID() const   { return _newPID; }
bool BLETuner::hasNewSpeed() const { return _newSpeed; }

PIDParams BLETuner::getPID() {
    _newPID = false;
    return _pid;
}

SpeedParams BLETuner::getSpeed() {
    _newSpeed = false;
    return _speed;
}
