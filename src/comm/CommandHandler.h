#pragma once
#include <cstdint>
#include "comm/CommandProtocol.h"
#include "config/ConfigStore.h"
#include "config/RobotConfig.h"

class ResponseSink {
public:
    virtual ~ResponseSink() = default;
    virtual void send(const char* json) = 0;
};

enum class ControlRequest : uint8_t {
    NONE, START, STOP, RESET, SENSOR_CAL, ENC_CAL_START, ENC_CAL_FINISH
};

/** Dados do frame info (spec §4.5, E-2), lidos na hora de enviar. */
struct DeviceInfo {
    const char* name = "LFR";
    bool        cfgErr = false;
    uint32_t    stkControl = 0;  ///< menor folga de pilha da task control, bytes
    uint32_t    stkComm = 0;     ///< idem, task comm
};
using DeviceInfoFn = DeviceInfo (*)();

struct ControlMsg {
    ControlRequest req = ControlRequest::NONE;
    bool leftSide = true;
    int  rotations = 0;
};

/**
 * Aplica comandos à configuração viva (spec §4.5). Roda na task comm (core 0).
 * - set: valida chave a chave; marca "dirty".
 * - save/load/defaults/prof: recusados em ARMED/RUNNING (escrita na flash para o cache).
 * - prof: recusa com "dirty" se houver mudança não salva, a menos que venha f:1.
 * - lock = 1 (modo competição): em ARMED/RUNNING todo comando é recusado com "locked".
 * - get: info + cfg. O info também sai quando o cliente ativa as notificações (A-WP1-01).
 * Comandos de estado viram ControlMsg para a task control (core 1).
 */
class CommandHandler {
public:
    CommandHandler(RobotConfig& live, ConfigStore& store, const char* fwVersion);

    void handle(const Command& cmd, bool running, ResponseSink& sink, ControlMsg& msg);
    void sendConfig(ResponseSink& sink) const;
    /** Sends the info frame (fw, cfgerr, stk). */
    void sendInfo(ResponseSink& sink) const;
    /** info followed by the cfg sequence: answer to get and to a new subscription. */
    void sendInfoAndConfig(ResponseSink& sink) const;
    /** Where the info values come from (name, cfgerr, stack headroom). Null → defaults. */
    void setInfoSource(DeviceInfoFn fn) { _infoFn = fn; }

    bool dirty() const { return _dirty; }
    bool consumeConfigChanged();
    const RobotConfig& live() const { return _live; }

private:
    void _handleSet(const Command& cmd, bool running, ResponseSink& sink);
    void _refuseLocked(const Command& cmd, ResponseSink& sink) const;
    void _ack(ResponseSink& sink, const char* what, bool ok, const char* err) const;
    void _reloadFromStore(uint8_t profile);

    RobotConfig& _live;
    ConfigStore& _store;
    const char*  _fw;
    DeviceInfoFn _infoFn = nullptr;
    bool _dirty = false;
    bool _changed = false;
};
