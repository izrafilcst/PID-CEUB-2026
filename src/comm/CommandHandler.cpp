#include "comm/CommandHandler.h"

namespace {
constexpr size_t BUF = 192;
}

CommandHandler::CommandHandler(RobotConfig& live, ConfigStore& store, const char* fwVersion)
    : _live(live), _store(store), _fw(fwVersion) {}

bool CommandHandler::consumeConfigChanged() {
    const bool c = _changed;
    _changed = false;
    return c;
}

void CommandHandler::_ack(ResponseSink& sink, const char* what, bool ok, const char* err) const {
    char buf[BUF];
    if (formatAckSimple(buf, sizeof buf, what, ok, err)) sink.send(buf);
}

void CommandHandler::sendConfig(ResponseSink& sink) const {
    char buf[BUF];
    for (size_t g = 0; g < robotcfg::GROUP_COUNT; g++) {
        if (formatCfgGroup(buf, sizeof buf, _live, robotcfg::GROUPS[g])) sink.send(buf);
    }
    if (formatCfgMeta(buf, sizeof buf, _live.profile, _dirty, _fw)) sink.send(buf);
}

void CommandHandler::_reloadFromStore(uint8_t profile) {
    _store.loadHardware(_live.hw);
    _store.loadProfile(profile, _live.tune);
    _live.profile = profile;
    _dirty = false;
    _changed = true;
}

void CommandHandler::_handleSet(const Command& cmd, bool running, ResponseSink& sink) {
    SetResult results[Command::MAX_ITEMS];
    for (uint8_t i = 0; i < cmd.count; i++) {
        results[i] = robotcfg::set(_live, cmd.items[i].key, cmd.items[i].value, running);
    }
    // 2ª passada: vmin/vmax chegando juntos (legado "spd") dependem da ordem
    for (uint8_t i = 0; i < cmd.count; i++) {
        if (results[i] == SetResult::OUT_OF_RANGE) {
            results[i] = robotcfg::set(_live, cmd.items[i].key, cmd.items[i].value, running);
        }
    }
    char buf[BUF];
    for (uint8_t i = 0; i < cmd.count; i++) {
        if (results[i] == SetResult::OK) {
            _dirty = true;
            _changed = true;
        }
        float applied = 0.0f;
        robotcfg::get(_live, cmd.items[i].key, applied);
        if (formatAck(buf, sizeof buf, cmd.items[i].key, results[i], applied)) sink.send(buf);
    }
}

void CommandHandler::_refuseLocked(const Command& cmd, ResponseSink& sink) const {
    if (cmd.type == CmdType::SET) {  // inclusive os legados pid/pidv/spd/rpm: um ack por chave
        for (uint8_t i = 0; i < cmd.count; i++) _ack(sink, cmd.items[i].key, false, "locked");
        return;
    }
    const char* what = nullptr;
    switch (cmd.type) {
        case CmdType::GET:            what = "get"; break;
        case CmdType::SAVE:           what = "save"; break;
        case CmdType::LOAD:           what = "load"; break;
        case CmdType::DEFAULTS:       what = "defaults"; break;
        case CmdType::PROFILE:        what = "prof"; break;
        case CmdType::SENSOR_CAL:     what = "scal"; break;
        case CmdType::START:          what = "start"; break;
        case CmdType::STOP:           what = "stop"; break;
        case CmdType::RESET:          what = "reset"; break;
        case CmdType::ENC_CAL_START:
        case CmdType::ENC_CAL_FINISH: what = "cal"; break;
        case CmdType::SET:
        case CmdType::NONE:           break;
    }
    if (what) _ack(sink, what, false, "locked");
}

void CommandHandler::handle(const Command& cmd, bool running, ResponseSink& sink,
                            ControlMsg& msg) {
    msg = ControlMsg{};
    // Modo competição (spec §4.5, regra 5.9): em ARMED/RUNNING nada do BLE tem efeito
    if (running && _live.hw.lock) {
        _refuseLocked(cmd, sink);
        return;
    }
    switch (cmd.type) {
        case CmdType::SET:
            _handleSet(cmd, running, sink);
            break;
        case CmdType::GET:
            sendConfig(sink);
            break;
        case CmdType::SAVE: {
            if (running) { _ack(sink, "save", false, "running"); break; }
            const bool ok = _store.saveActive(_live);
            if (ok) _dirty = false;
            _ack(sink, "save", ok, ok ? nullptr : "store");
            break;
        }
        case CmdType::LOAD:
            if (running) { _ack(sink, "load", false, "running"); break; }
            _reloadFromStore(_live.profile);
            _ack(sink, "load", true, nullptr);
            sendConfig(sink);
            break;
        case CmdType::DEFAULTS:
            if (running) { _ack(sink, "defaults", false, "running"); break; }
            _live.hw = robotcfg::factoryHardware();
            _live.tune = robotcfg::factoryProfile(_live.profile);
            _dirty = true;
            _changed = true;
            _ack(sink, "defaults", true, nullptr);
            sendConfig(sink);
            break;
        case CmdType::PROFILE:
            if (cmd.profile >= PROFILE_COUNT) { _ack(sink, "prof", false, "range"); break; }
            if (running) { _ack(sink, "prof", false, "running"); break; }
            if (_dirty && !cmd.force) { _ack(sink, "prof", false, "dirty"); break; }
            _reloadFromStore(cmd.profile);
            _ack(sink, "prof", true, nullptr);
            sendConfig(sink);
            break;
        case CmdType::SENSOR_CAL:
            msg.req = ControlRequest::SENSOR_CAL;
            _ack(sink, "scal", true, nullptr);
            break;
        case CmdType::START:
            msg.req = ControlRequest::START;
            _ack(sink, "start", true, nullptr);
            break;
        case CmdType::STOP:
            msg.req = ControlRequest::STOP;
            _ack(sink, "stop", true, nullptr);
            break;
        case CmdType::RESET:
            msg.req = ControlRequest::RESET;
            _ack(sink, "reset", true, nullptr);
            break;
        case CmdType::ENC_CAL_START:
        case CmdType::ENC_CAL_FINISH:
            if (running) { _ack(sink, "cal", false, "running"); break; }
            msg.req = (cmd.type == CmdType::ENC_CAL_START) ? ControlRequest::ENC_CAL_START
                                                           : ControlRequest::ENC_CAL_FINISH;
            msg.leftSide = cmd.leftSide;
            msg.rotations = cmd.rotations;
            _ack(sink, "cal", true, nullptr);
            break;
        case CmdType::NONE:
            break;
    }
}
