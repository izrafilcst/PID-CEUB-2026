#pragma once
#include <cstdint>

#ifndef NATIVE_BUILD
#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
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
class BLETuner {
public:
    BLETuner();

    void begin(const char* deviceName = "LFR-RACER-01");
    void update();

    // Recepção de comandos — consumir no loop Core 1
    bool hasNewPID()   const;
    bool hasNewSpeed() const;
    PIDParams   getPID();    // consome flag
    SpeedParams getSpeed();  // consome flag
    bool consumeStart();
    bool consumeStop();
    bool consumeReset();
    bool consumeNewConnection();  // true uma vez quando cliente conecta

    // Novos comandos — Fase D
    bool hasNewVelocityPID() const;
    bool hasNewRpm()         const;
    bool hasNewCalibration() const;
    VelocityPIDParams getVelocityPID();  // consome flag
    RpmParams         getRpm();          // consome flag
    CalibrationCmd    getCalibration();  // consome flag

    // Envio de telemetria — chamar no loop Core 1
    void notifyInfo(const char* name, const char* fw, const char* mode);
    void notifyTelemetry(float pos, float corr, int vL, int vR, uint32_t dtUs,
                         float rpmL, float rpmR, float spL, float spR,
                         const float* sensors, int nSensors, float batPct,
                         float bestLapSec, bool hasLap,
                         const float* laps, int nLaps);

    // UUIDs do serviço BLE (16-bit em formato 128-bit)
    static constexpr const char* SERVICE_UUID = "0000abcc-0000-1000-8000-00805f9b34fb";
    static constexpr const char* CHAR_TEL_UUID = "0000abcd-0000-1000-8000-00805f9b34fb";
    static constexpr const char* CHAR_CMD_UUID = "0000abce-0000-1000-8000-00805f9b34fb";

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

#ifndef NATIVE_BUILD
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
    NimBLECharacteristic* _charTel = nullptr;
    NimBLECharacteristic* _charCmd = nullptr;
    bool _clientConnected = false;

    void _onWrite(const std::string& val);
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
