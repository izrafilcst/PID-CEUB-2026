#pragma once
#include <cstddef>
#include "comm/CommandQueue.h"
#include "comm/SpinLock.h"

#ifndef NATIVE_BUILD
#include <NimBLEDevice.h>
#endif

/**
 * Transporte BLE (NimBLE). Recebe JSON na característica 0xABCE e enfileira; envia JSON
 * por notificação na 0xABCD. Não interpreta nada — isso é do CommandProtocol/CommandHandler.
 */
class BLETuner {
public:
    BLETuner() = default;

    void begin(const char* deviceName);
    bool popCommand(char* out, size_t len) { return _queue.pop(out, len); }
    bool consumeNewConnection();
    bool isConnected() const;
    void notify(const char* json);

#ifdef NATIVE_BUILD
    bool injectForTest(const char* json);
#endif

    static constexpr const char* SERVICE_UUID  = "0000abcc-0000-1000-8000-00805f9b34fb";
    static constexpr const char* CHAR_TEL_UUID = "0000abcd-0000-1000-8000-00805f9b34fb";
    static constexpr const char* CHAR_CMD_UUID = "0000abce-0000-1000-8000-00805f9b34fb";

private:
    CommandQueue     _queue;
    mutable SpinLock _lock;
    bool _connected = false;
    bool _newConn = false;

#ifndef NATIVE_BUILD
    NimBLECharacteristic* _charTel = nullptr;
    NimBLECharacteristic* _charCmd = nullptr;
    static BLETuner* _instance;

    class WriteCallback : public NimBLECharacteristicCallbacks {
    public:
        void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override;
    };
    class ServerCallback : public NimBLEServerCallbacks {
    public:
        void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override;
        void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override;
    };
    WriteCallback  _writeCallback;
    ServerCallback _serverCallback;
#endif
};
