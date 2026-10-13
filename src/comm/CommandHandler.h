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
 * Comandos de estado viram ControlMsg para a task control (core 1).
 */
class CommandHandler {
public:
    CommandHandler(RobotConfig& live, ConfigStore& store, const char* fwVersion);

    void handle(const Command& cmd, bool running, ResponseSink& sink, ControlMsg& msg);
    void sendConfig(ResponseSink& sink) const;

    bool dirty() const { return _dirty; }
    bool consumeConfigChanged();
    const RobotConfig& live() const { return _live; }

private:
    void _handleSet(const Command& cmd, bool running, ResponseSink& sink);
    void _ack(ResponseSink& sink, const char* what, bool ok, const char* err) const;
    void _reloadFromStore(uint8_t profile);

    RobotConfig& _live;
    ConfigStore& _store;
    const char*  _fw;
    bool _dirty = false;
    bool _changed = false;
};
