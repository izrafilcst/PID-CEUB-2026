#pragma once
#include <cstddef>
#include <cstdint>
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
    /** True once after a client enables notifications on 0xABCD (time to send info + cfg). */
    bool consumeNewSubscription();
    bool isConnected() const;
    void notify(const char* json);

#ifdef NATIVE_BUILD
    bool injectForTest(const char* json);
    void injectSubscribeForTest(uint16_t subValue) { _onSubscribe(subValue); }
#endif

    static constexpr const char* SERVICE_UUID  = "0000abcc-0000-1000-8000-00805f9b34fb";
    static constexpr const char* CHAR_TEL_UUID = "0000abcd-0000-1000-8000-00805f9b34fb";
    static constexpr const char* CHAR_CMD_UUID = "0000abce-0000-1000-8000-00805f9b34fb";

private:
    /** subValue do CCCD: bit 0 = notificações, bit 1 = indicações. */
    void _onSubscribe(uint16_t subValue);

    CommandQueue     _queue;
    mutable SpinLock _lock;
    bool _connected = false;
    bool _newConn = false;
    bool _newSub = false;

#ifndef NATIVE_BUILD
    NimBLECharacteristic* _charTel = nullptr;
    NimBLECharacteristic* _charCmd = nullptr;
    static BLETuner* _instance;

    class WriteCallback : public NimBLECharacteristicCallbacks {
    public:
        void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override;
    };
    class TelCallback : public NimBLECharacteristicCallbacks {
    public:
        void onSubscribe(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo,
                         uint16_t subValue) override;
    };
    class ServerCallback : public NimBLEServerCallbacks {
    public:
        void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override;
        void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override;
    };
    WriteCallback  _writeCallback;
    TelCallback    _telCallback;
    ServerCallback _serverCallback;
#endif
};
