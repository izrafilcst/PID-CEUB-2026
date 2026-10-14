#include "config/ConfigStore.h"
#include "config.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

constexpr uint16_t MAGIC = 0x4C46;  // "LF"
constexpr uint16_t HW_VERSION = 1;
constexpr uint16_t PROFILE_VERSION = 1;
constexpr uint16_t ACTIVE_VERSION = 1;
constexpr uint16_t CAL_VERSION = 1;
constexpr size_t HEADER = 6;
constexpr size_t CRC_LEN = 4;
constexpr size_t MAX_BLOB = 128;
constexpr float PPR_MIN = ENCODER_PPR_X4_MIN;
constexpr float PPR_MAX = ENCODER_PPR_X4_MAX;

const char* const KEY_HW = "hw";
const char* const KEY_ACTIVE = "prof_act";
const char* const KEY_CAL = "cal";
const char* const KEY_PPR_L = "left_ppr";
const char* const KEY_PPR_R = "right_ppr";

static_assert(sizeof(HardwareConfig) + HEADER + CRC_LEN <= MAX_BLOB, "HardwareConfig blob");
static_assert(sizeof(TuningProfile) + HEADER + CRC_LEN <= MAX_BLOB, "TuningProfile blob");
static_assert(sizeof(CalibrationData) + HEADER + CRC_LEN <= MAX_BLOB, "CalibrationData blob");

void put16(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
}
uint16_t get16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
void put32(uint8_t* p, uint32_t v) {
    for (int i = 0; i < 4; i++) p[i] = static_cast<uint8_t>(v >> (8 * i));
}
uint32_t get32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

void profileKey(uint8_t index, char* out, size_t len) {
    std::snprintf(out, len, "prof%u", static_cast<unsigned>(index));
}

bool pprSane(float v) { return std::isfinite(v) && v >= PPR_MIN && v <= PPR_MAX; }

}  // namespace

ConfigStore::ConfigStore(IPersistentStore& store) : _store(store) {}

uint32_t ConfigStore::crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

bool ConfigStore::_write(const char* key, uint16_t version, const void* payload, uint16_t size) {
    uint8_t buf[MAX_BLOB];
    put16(buf, MAGIC);
    put16(buf + 2, version);
    put16(buf + 4, size);
    std::memcpy(buf + HEADER, payload, size);
    put32(buf + HEADER + size, crc32(buf, HEADER + size));
    return _store.putBlob(key, buf, HEADER + size + CRC_LEN);
}

bool ConfigStore::_read(const char* key, uint16_t version, void* payload, uint16_t size) {
    uint8_t buf[MAX_BLOB];
    const size_t len = _store.getBlob(key, buf, sizeof buf);
    if (len != HEADER + size + CRC_LEN) return false;
    if (get16(buf) != MAGIC || get16(buf + 2) != version || get16(buf + 4) != size) return false;
    if (get32(buf + HEADER + size) != crc32(buf, HEADER + size)) return false;
    std::memcpy(payload, buf + HEADER, size);
    return true;
}

bool ConfigStore::loadHardware(HardwareConfig& hw) {
    HardwareConfig tmp{};
    RobotConfig probe = robotcfg::factoryConfig();
    bool ok = _read(KEY_HW, HW_VERSION, &tmp, sizeof tmp);
    if (ok) {
        probe.hw = tmp;
        ok = robotcfg::validate(probe);
    }
    hw = ok ? tmp : robotcfg::factoryHardware();
    // PPR: as chaves da auto-calibração do VelocityEstimator prevalecem
    if (_store.exists(KEY_PPR_L)) {
        const float v = _store.getFloat(KEY_PPR_L, 0.0f);
        if (pprSane(v)) hw.pprL = v;
    }
    if (_store.exists(KEY_PPR_R)) {
        const float v = _store.getFloat(KEY_PPR_R, 0.0f);
        if (pprSane(v)) hw.pprR = v;
    }
    return ok;
}

bool ConfigStore::loadProfile(uint8_t index, TuningProfile& out) {
    if (index >= PROFILE_COUNT) {
        out = robotcfg::factoryProfile(0);
        return false;
    }
    char key[8];
    profileKey(index, key, sizeof key);
    TuningProfile tmp{};
    RobotConfig probe = robotcfg::factoryConfig();
    bool ok = _read(key, PROFILE_VERSION, &tmp, sizeof tmp);
    if (ok) {
        probe.tune = tmp;
        ok = robotcfg::validate(probe);
    }
    out = ok ? tmp : robotcfg::factoryProfile(index);
    return ok;
}

uint8_t ConfigStore::loadActiveIndex() {
    uint8_t idx = 0;
    if (!_read(KEY_ACTIVE, ACTIVE_VERSION, &idx, sizeof idx) || idx >= PROFILE_COUNT) return 0;
    return idx;
}

ConfigStore::LoadReport ConfigStore::load(RobotConfig& cfg) {
    LoadReport r{};
    r.hardwareOk = loadHardware(cfg.hw);
    cfg.profile = loadActiveIndex();
    r.profileOk = loadProfile(cfg.profile, cfg.tune);
    return r;
}

bool ConfigStore::saveActive(const RobotConfig& cfg) {
    if (!robotcfg::validate(cfg)) return false;
    char key[8];
    profileKey(cfg.profile, key, sizeof key);
    bool ok = _write(KEY_HW, HW_VERSION, &cfg.hw, sizeof cfg.hw);
    ok = _write(key, PROFILE_VERSION, &cfg.tune, sizeof cfg.tune) && ok;
    ok = _write(KEY_ACTIVE, ACTIVE_VERSION, &cfg.profile, sizeof cfg.profile) && ok;
    _store.putFloat(KEY_PPR_L, cfg.hw.pprL);
    _store.putFloat(KEY_PPR_R, cfg.hw.pprR);
    return ok;
}

bool ConfigStore::saveCalibration(const CalibrationData& data) {
    return _write(KEY_CAL, CAL_VERSION, &data, sizeof data);
}

bool ConfigStore::loadCalibration(CalibrationData& data) {
    return _read(KEY_CAL, CAL_VERSION, &data, sizeof data);
}
