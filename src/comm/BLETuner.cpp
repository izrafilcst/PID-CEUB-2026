#include "comm/BLETuner.h"
#include <cstring>

bool BLETuner::consumeNewConnection() {
    SpinGuard guard(_lock);
    const bool n = _newConn;
    _newConn = false;
    return n;
}

bool BLETuner::isConnected() const {
    SpinGuard guard(_lock);
    return _connected;
}

#ifndef NATIVE_BUILD

BLETuner* BLETuner::_instance = nullptr;

void BLETuner::WriteCallback::onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo&) {
    if (!_instance) return;
    const std::string v = pChar->getValue();
    _instance->_queue.push(v.c_str(), v.size());
}

void BLETuner::ServerCallback::onConnect(NimBLEServer*, NimBLEConnInfo&) {
    if (!_instance) return;
    SpinGuard guard(_instance->_lock);
    _instance->_connected = true;
    _instance->_newConn = true;
}

void BLETuner::ServerCallback::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo&, int) {
    if (_instance) {
        SpinGuard guard(_instance->_lock);
        _instance->_connected = false;
    }
    pServer->startAdvertising();
}

void BLETuner::begin(const char* deviceName) {
    _instance = this;
    NimBLEDevice::init(deviceName);
    NimBLEDevice::setMTU(185);
    NimBLEServer* server = NimBLEDevice::createServer();
    server->setCallbacks(&_serverCallback);
    NimBLEService* service = server->createService(SERVICE_UUID);
    _charTel = service->createCharacteristic(CHAR_TEL_UUID,
                                             NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    _charCmd = service->createCharacteristic(CHAR_CMD_UUID,
                                             NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    _charCmd->setCallbacks(&_writeCallback);
    service->start();
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(SERVICE_UUID);
    adv->start();
}

void BLETuner::notify(const char* json) {
    if (!json || !_charTel || !isConnected()) return;
    // setValue(ptr, len): envia só os bytes do texto, nunca o buffer inteiro
    _charTel->setValue(reinterpret_cast<const uint8_t*>(json), std::strlen(json));
    _charTel->notify();
}

#else  // NATIVE_BUILD

void BLETuner::begin(const char*) {}
void BLETuner::notify(const char*) {}
bool BLETuner::injectForTest(const char* json) {
    return json && _queue.push(json, std::strlen(json));
}

#endif
