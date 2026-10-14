#pragma once
#include <cstddef>
#include <cstdint>
#include "comm/Telemetry.h"
#include "config/RobotConfig.h"

constexpr size_t BLE_MAX_MSG = 180;

enum class CmdType : uint8_t {
    NONE, SET, GET, SAVE, LOAD, DEFAULTS, PROFILE, SENSOR_CAL,
    START, STOP, RESET, ENC_CAL_START, ENC_CAL_FINISH
};

struct SetItem {
    char  key[8];
    float value;
};

struct Command {
    static constexpr uint8_t MAX_ITEMS = 4;
    CmdType type = CmdType::NONE;
    SetItem items[MAX_ITEMS] = {};
    uint8_t count = 0;
    uint8_t profile = 0;
    bool    force = false;
    bool    leftSide = true;
    int     rotations = 0;
};

/**
 * Parses one command (spec §4.5). New: set/get/save/load/defaults/prof/scal.
 * Legacy: pid/pidv/spd/rpm become SET items; cal/start/stop/reset keep their meaning.
 * Returns false (and type NONE) for malformed or unknown commands.
 */
bool parseCommand(const char* json, Command& out);

bool formatAck(char* buf, size_t n, const char* key, SetResult result, float applied);
bool formatAckSimple(char* buf, size_t n, const char* what, bool ok, const char* err);
bool formatCfgGroup(char* buf, size_t n, const RobotConfig& cfg, const char* group);
bool formatCfgMeta(char* buf, size_t n, uint8_t profile, bool dirty, const char* fw);
/** `stkControl`/`stkComm`: lowest free stack of each task since boot, bytes (spec §4.5, E-2). */
bool formatInfo(char* buf, size_t n, const char* name, const char* fw, bool cfgErr,
                uint32_t stkControl, uint32_t stkComm);
bool formatCalResult(char* buf, size_t n, const CalReport& report);
bool formatEncCal(char* buf, size_t n, const CalReport& report);
bool formatTelemetry(char* buf, size_t n, const TelemetryFrame& f);
bool formatStatus(char* buf, size_t n, const StatusFrame& s);
