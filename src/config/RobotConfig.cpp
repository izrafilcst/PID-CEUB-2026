#include "config/RobotConfig.h"
#include <cmath>
#include <cstring>

namespace robotcfg {
namespace {

using FT = FieldType;

#define HW_KEY(k, t, mn, mx) {#k, "hw", t, mn, mx, true, false, offsetof(HardwareConfig, k)}
#define TP_KEY(k, g, t, mn, mx, live) {#k, g, t, mn, mx, false, live, offsetof(TuningProfile, k)}

const KeyInfo KEYS[] = {
    HW_KEY(invL,  FT::U8, 0.0f, 1.0f),
    HW_KEY(invR,  FT::U8, 0.0f, 1.0f),
    HW_KEY(swap,  FT::U8, 0.0f, 1.0f),
    HW_KEY(einvL, FT::U8, 0.0f, 1.0f),
    HW_KEY(einvR, FT::U8, 0.0f, 1.0f),
    HW_KEY(pol,   FT::U8, 0.0f, 1.0f),
    HW_KEY(sinv,  FT::U8, 0.0f, 1.0f),
    HW_KEY(chem,  FT::U8, 0.0f, 1.0f),
    HW_KEY(vcut,  FT::F32, 5.6f, 7.6f),
    HW_KEY(vbk,   FT::F32, 0.8f, 1.2f),
    HW_KEY(pprL,  FT::F32, 1.0f, 10000.0f),
    HW_KEY(pprR,  FT::F32, 1.0f, 10000.0f),
    TP_KEY(kp,      "pid",  FT::F32, 0.0f,   20.0f,   true),
    TP_KEY(ki,      "pid",  FT::F32, 0.0f,   50.0f,   true),
    TP_KEY(kd,      "pid",  FT::F32, 0.0f,   2.0f,    true),
    TP_KEY(dlp,     "pid",  FT::F32, 0.05f,  1.0f,    true),
    TP_KEY(mode,    "vel",  FT::U8,  0.0f,   1.0f,    false),
    TP_KEY(kpv,     "vel",  FT::F32, 0.0f,   0.05f,   true),
    TP_KEY(kiv,     "vel",  FT::F32, 0.0f,   1.0f,    true),
    TP_KEY(kdv,     "vel",  FT::F32, 0.0f,   0.01f,   true),
    TP_KEY(rpmMax,  "vel",  FT::F32, 100.0f, 2000.0f, true),
    TP_KEY(vmax,    "spd",  FT::F32, 0.0f,   1.0f,    true),
    TP_KEY(vmin,    "spd",  FT::F32, 0.0f,   1.0f,    true),
    TP_KEY(thr,     "spd",  FT::F32, 0.05f,  1.0f,    true),
    TP_KEY(acc,     "spd",  FT::F32, 0.1f,   50.0f,   true),
    TP_KEY(vref,    "misc", FT::F32, 5.0f,   8.4f,    true),
    TP_KEY(vcomp,   "misc", FT::U8,  0.0f,   1.0f,    true),
    TP_KEY(lostMs,  "misc", FT::U16, 50.0f,  5000.0f, true),
    TP_KEY(startMs, "misc", FT::U16, 0.0f,   5000.0f, true),
};

#undef HW_KEY
#undef TP_KEY

constexpr size_t KEY_COUNT = sizeof(KEYS) / sizeof(KEYS[0]);

const uint8_t* fieldPtr(const RobotConfig& cfg, const KeyInfo& k) {
    const uint8_t* base = k.hardware ? reinterpret_cast<const uint8_t*>(&cfg.hw)
                                     : reinterpret_cast<const uint8_t*>(&cfg.tune);
    return base + k.offset;
}

uint8_t* fieldPtr(RobotConfig& cfg, const KeyInfo& k) {
    return const_cast<uint8_t*>(fieldPtr(static_cast<const RobotConfig&>(cfg), k));
}

float readField(const RobotConfig& cfg, const KeyInfo& k) {
    const uint8_t* p = fieldPtr(cfg, k);
    switch (k.type) {
        case FT::U8:  { uint8_t v;  std::memcpy(&v, p, sizeof v); return v; }
        case FT::U16: { uint16_t v; std::memcpy(&v, p, sizeof v); return v; }
        case FT::F32: { float v;    std::memcpy(&v, p, sizeof v); return v; }
    }
    return 0.0f;
}

void writeField(RobotConfig& cfg, const KeyInfo& k, float value) {
    uint8_t* p = fieldPtr(cfg, k);
    switch (k.type) {
        case FT::U8: {
            const uint8_t v = static_cast<uint8_t>(value);
            std::memcpy(p, &v, sizeof v);
            break;
        }
        case FT::U16: {
            const uint16_t v = static_cast<uint16_t>(value);
            std::memcpy(p, &v, sizeof v);
            break;
        }
        case FT::F32:
            std::memcpy(p, &value, sizeof value);
            break;
    }
}

}  // namespace

HardwareConfig factoryHardware() { return HardwareConfig{}; }

TuningProfile factoryProfile(uint8_t index) {
    static constexpr float VMAX[PROFILE_COUNT] = {0.35f, 0.50f, 0.70f};
    TuningProfile p{};
    p.vmax = VMAX[index < PROFILE_COUNT ? index : 0];
    return p;
}

RobotConfig factoryConfig() {
    RobotConfig c{};
    c.hw = factoryHardware();
    c.tune = factoryProfile(0);
    c.profile = 0;
    return c;
}

const KeyInfo* keyTable(size_t& count) {
    count = KEY_COUNT;
    return KEYS;
}

const KeyInfo* findKey(const char* key) {
    if (!key) return nullptr;
    for (const KeyInfo& k : KEYS) {
        if (std::strcmp(k.key, key) == 0) return &k;
    }
    return nullptr;
}

SetResult set(RobotConfig& cfg, const char* key, float value, bool running) {
    const KeyInfo* k = findKey(key);
    if (!k) return SetResult::UNKNOWN_KEY;
    if (running && !k->liveInRun) return SetResult::LOCKED_WHILE_RUNNING;
    if (!std::isfinite(value)) return SetResult::OUT_OF_RANGE;
    const float v = (k->type == FT::F32) ? value : std::round(value);
    if (v < k->min || v > k->max) return SetResult::OUT_OF_RANGE;
    if (std::strcmp(k->key, "vmin") == 0 && v > cfg.tune.vmax) return SetResult::OUT_OF_RANGE;
    if (std::strcmp(k->key, "vmax") == 0 && v < cfg.tune.vmin) return SetResult::OUT_OF_RANGE;
    writeField(cfg, *k, v);
    return SetResult::OK;
}

bool get(const RobotConfig& cfg, const char* key, float& value) {
    const KeyInfo* k = findKey(key);
    if (!k) return false;
    value = readField(cfg, *k);
    return true;
}

bool validate(const RobotConfig& cfg) {
    for (const KeyInfo& k : KEYS) {
        const float v = readField(cfg, k);
        if (!std::isfinite(v) || v < k.min || v > k.max) return false;
    }
    return cfg.tune.vmin <= cfg.tune.vmax && cfg.profile < PROFILE_COUNT;
}

}  // namespace robotcfg
