#pragma once
#include <cstdint>

#ifndef NATIVE_BUILD
#include <NimBLEDevice.h>
#endif

struct PIDParams {
    float kp = 3.0f;
    float ki = 0.0f;
    float kd = 12.0f;
};

struct SpeedParams {
    int   baseSpeed  = 160;
    int   minSpeed   = 60;
    int   maxSpeed   = 230;
    float threshold  = 0.6f;
};

class BLETuner {
public:
    BLETuner();

    void begin(const char* deviceName = "LFR-Tuner");
    void update();                  // chamar do loop Core 1

    bool hasNewPID() const;
    bool hasNewSpeed() const;
    PIDParams   getPID();           // consome flag
    SpeedParams getSpeed();         // consome flag

    void notifyStatus(const char* csv);   // envia telemetria ao cliente

    // UUIDs do serviço BLE
    static constexpr const char* SERVICE_UUID        = "12345678-1234-1234-1234-123456789abc";
    static constexpr const char* CHAR_KP_UUID        = "12345678-1234-1234-1234-123456789001";
    static constexpr const char* CHAR_KI_UUID        = "12345678-1234-1234-1234-123456789002";
    static constexpr const char* CHAR_KD_UUID        = "12345678-1234-1234-1234-123456789003";
    static constexpr const char* CHAR_SPEED_UUID     = "12345678-1234-1234-1234-123456789004";
    static constexpr const char* CHAR_TELEMETRY_UUID = "12345678-1234-1234-1234-123456789005";

private:
    PIDParams   _pid;
    SpeedParams _speed;
    bool _newPID   = false;
    bool _newSpeed = false;

#ifndef NATIVE_BUILD
    NimBLECharacteristic* _charKp         = nullptr;
    NimBLECharacteristic* _charKi         = nullptr;
    NimBLECharacteristic* _charKd         = nullptr;
    NimBLECharacteristic* _charSpeed      = nullptr;
    NimBLECharacteristic* _charTelemetry  = nullptr;
    bool _clientConnected = false;

    void _onWrite(NimBLECharacteristic* pChar);
    static BLETuner* _instance;

    class WriteCallback : public NimBLECharacteristicCallbacks {
    public:
        void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override;
    };
    WriteCallback _writeCallback;

    class ServerCallback : public NimBLEServerCallbacks {
    public:
        void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override;
        void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override;
    };
    ServerCallback _serverCallback;
#endif
};
