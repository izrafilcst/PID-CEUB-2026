#include "comm/CommandProtocol.h"
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

/** Escreve em sequência num buffer; ok() fica false se algo não coube. */
class Writer {
public:
    Writer(char* buf, size_t n) : _buf(buf), _n(n) {
        if (n > 0) buf[0] = '\0';
        else _ok = false;
    }

    void add(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
        if (!_ok) return;
        va_list ap;
        va_start(ap, fmt);
        const int r = std::vsnprintf(_buf + _len, _n - _len, fmt, ap);
        va_end(ap);
        if (r < 0 || static_cast<size_t>(r) >= _n - _len) {
            _ok = false;
            return;
        }
        _len += static_cast<size_t>(r);
    }

    bool ok() const { return _ok; }

private:
    char*  _buf;
    size_t _n;
    size_t _len = 0;
    bool   _ok = true;
};

const char* findValue(const char* json, const char* key) {
    char pattern[24];
    const int n = std::snprintf(pattern, sizeof pattern, "\"%s\":", key);
    if (n <= 0 || static_cast<size_t>(n) >= sizeof pattern) return nullptr;
    const char* p = std::strstr(json, pattern);
    if (!p) return nullptr;
    p += n;
    while (*p == ' ') p++;
    return p;
}

bool readNumber(const char* json, const char* key, float& out) {
    const char* p = findValue(json, key);
    if (!p) return false;
    char* end = nullptr;
    const float v = std::strtof(p, &end);
    if (end == p) return false;
    out = v;
    return true;
}

bool readString(const char* json, const char* key, char* out, size_t len) {
    const char* p = findValue(json, key);
    if (!p || *p != '"') return false;
    p++;
    size_t i = 0;
    while (*p && *p != '"') {
        if (i + 1 >= len) return false;
        out[i++] = *p++;
    }
    if (*p != '"') return false;
    out[i] = '\0';
    return true;
}

bool addItem(Command& c, const char* key, float value) {
    const size_t len = std::strlen(key);
    if (c.count >= Command::MAX_ITEMS || len >= sizeof(c.items[0].key)) return false;
    std::memcpy(c.items[c.count].key, key, len + 1);
    c.items[c.count].value = value;
    c.count++;
    return true;
}

void addIfPresent(Command& c, const char* json, const char* jsonKey, const char* cfgKey,
                  float scale) {
    float v = 0.0f;
    if (readNumber(json, jsonKey, v)) addItem(c, cfgKey, v * scale);
}

bool fail(Command& out) {
    out = Command{};
    return false;
}

bool setOf(Command& out) {
    if (out.count == 0) return fail(out);
    out.type = CmdType::SET;
    return true;
}

const char* errText(SetResult r) {
    switch (r) {
        case SetResult::UNKNOWN_KEY: return "key";
        case SetResult::OUT_OF_RANGE: return "range";
        case SetResult::LOCKED_WHILE_RUNNING: return "running";
        case SetResult::OK: break;
    }
    return "";
}

int clampInt(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }

}  // namespace

bool parseCommand(const char* json, Command& out) {
    out = Command{};
    if (!json) return false;
    char t[12];
    if (!readString(json, "t", t, sizeof t)) return false;

    if (std::strcmp(t, "set") == 0) {
        char k[8];
        float v = 0.0f;
        if (!readString(json, "k", k, sizeof k) || !readNumber(json, "v", v)) return fail(out);
        if (!addItem(out, k, v)) return fail(out);
        out.type = CmdType::SET;
        return true;
    }
    if (std::strcmp(t, "pid") == 0) {
        addIfPresent(out, json, "kp", "kp", 1.0f);
        addIfPresent(out, json, "ki", "ki", 1.0f);
        addIfPresent(out, json, "kd", "kd", 1.0f);
        return setOf(out);
    }
    if (std::strcmp(t, "pidv") == 0) {
        addIfPresent(out, json, "kp", "kpv", 1.0f);
        addIfPresent(out, json, "ki", "kiv", 1.0f);
        addIfPresent(out, json, "kd", "kdv", 1.0f);
        return setOf(out);
    }
    if (std::strcmp(t, "spd") == 0) {  // legado em 0–255; "base" não existe mais (F-15)
        addIfPresent(out, json, "max", "vmax", 1.0f / 255.0f);
        addIfPresent(out, json, "min", "vmin", 1.0f / 255.0f);
        addIfPresent(out, json, "thrs", "thr", 1.0f);
        return setOf(out);
    }
    if (std::strcmp(t, "rpm") == 0) {
        addIfPresent(out, json, "max", "rpmMax", 1.0f);
        return setOf(out);
    }
    if (std::strcmp(t, "prof") == 0) {
        float n = 0.0f;
        if (!readNumber(json, "n", n) || n < 0.0f || n > 255.0f || n != std::floor(n)) {
            return fail(out);
        }
        float f = 0.0f;
        out.type = CmdType::PROFILE;
        out.profile = static_cast<uint8_t>(n);
        out.force = readNumber(json, "f", f) && f != 0.0f;
        return true;
    }
    if (std::strcmp(t, "cal") == 0) {
        char op[8];
        if (!readString(json, "op", op, sizeof op)) return fail(out);
        if (std::strcmp(op, "start") == 0) out.type = CmdType::ENC_CAL_START;
        else if (std::strcmp(op, "finish") == 0) out.type = CmdType::ENC_CAL_FINISH;
        else return fail(out);
        float rot = 10.0f;
        readNumber(json, "rot", rot);
        out.rotations = static_cast<int>(rot);
        char side[4];
        out.leftSide = !(readString(json, "side", side, sizeof side) && side[0] == 'R');
        return true;
    }

    struct Simple { const char* name; CmdType type; };
    static const Simple SIMPLE[] = {
        {"get", CmdType::GET},           {"save", CmdType::SAVE},
        {"load", CmdType::LOAD},         {"defaults", CmdType::DEFAULTS},
        {"scal", CmdType::SENSOR_CAL},   {"start", CmdType::START},
        {"stop", CmdType::STOP},         {"reset", CmdType::RESET},
    };
    for (const Simple& s : SIMPLE) {
        if (std::strcmp(t, s.name) == 0) {
            out.type = s.type;
            return true;
        }
    }
    return fail(out);
}

bool formatAck(char* buf, size_t n, const char* key, SetResult result, float applied) {
    Writer w(buf, n);
    if (result == SetResult::OK) {
        w.add("{\"t\":\"ack\",\"k\":\"%s\",\"ok\":1,\"v\":%.6g}", key,
              static_cast<double>(applied));
    } else {
        w.add("{\"t\":\"ack\",\"k\":\"%s\",\"ok\":0,\"err\":\"%s\"}", key, errText(result));
    }
    return w.ok();
}

bool formatAckSimple(char* buf, size_t n, const char* what, bool ok, const char* err) {
    Writer w(buf, n);
    if (ok) w.add("{\"t\":\"ack\",\"k\":\"%s\",\"ok\":1}", what);
    else w.add("{\"t\":\"ack\",\"k\":\"%s\",\"ok\":0,\"err\":\"%s\"}", what, err ? err : "fail");
    return w.ok();
}

bool formatCfgGroup(char* buf, size_t n, const RobotConfig& cfg, const char* group) {
    Writer w(buf, n);
    w.add("{\"t\":\"cfg\",\"g\":\"%s\"", group);
    size_t count = 0;
    const robotcfg::KeyInfo* keys = robotcfg::keyTable(count);
    for (size_t i = 0; i < count; i++) {
        if (std::strcmp(keys[i].group, group) != 0) continue;
        float v = 0.0f;
        robotcfg::get(cfg, keys[i].key, v);
        if (keys[i].type == robotcfg::FieldType::F32) {
            w.add(",\"%s\":%.6g", keys[i].key, static_cast<double>(v));
        } else {
            w.add(",\"%s\":%ld", keys[i].key, std::lround(v));
        }
    }
    w.add("}");
    return w.ok();
}

bool formatCfgMeta(char* buf, size_t n, uint8_t profile, bool dirty, const char* fw) {
    Writer w(buf, n);
    w.add("{\"t\":\"cfg\",\"g\":\"meta\",\"prof\":%u,\"dirty\":%u,\"fw\":\"%s\"}",
          static_cast<unsigned>(profile), dirty ? 1u : 0u, fw);
    return w.ok();
}

bool formatInfo(char* buf, size_t n, const char* name, const char* fw, bool cfgErr,
                uint32_t stkControl, uint32_t stkComm) {
    Writer w(buf, n);
    w.add("{\"t\":\"info\",\"name\":\"%s\",\"fw\":\"%s\",\"mode\":\"RUN\",\"cfgerr\":%u,",
          name, fw, cfgErr ? 1u : 0u);
    w.add("\"stk\":[%lu,%lu]}", static_cast<unsigned long>(stkControl),
          static_cast<unsigned long>(stkComm));
    return w.ok();
}

bool formatCalResult(char* buf, size_t n, const CalReport& r) {
    Writer w(buf, n);
    if (r.ok) {
        w.add("{\"t\":\"calres\",\"ok\":1}");
        return w.ok();
    }
    w.add("{\"t\":\"calres\",\"ok\":0,\"ch\":[");
    for (uint8_t i = 0; i < r.nBad && i < 8; i++) {
        w.add(i == 0 ? "%u" : ",%u", static_cast<unsigned>(r.bad[i]));
    }
    w.add("]}");
    return w.ok();
}

bool formatEncCal(char* buf, size_t n, const CalReport& r) {
    Writer w(buf, n);
    w.add("{\"t\":\"ecal\",\"side\":\"%s\",\"ok\":%u,\"ppr\":%.1f}",
          r.left ? "L" : "R", r.ok ? 1u : 0u, static_cast<double>(r.ppr));
    return w.ok();
}

bool formatTelemetry(char* buf, size_t n, const TelemetryFrame& f) {
    Writer w(buf, n);
    w.add("{\"t\":\"tel\",\"pos\":%.3f,\"corr\":%.3f,\"vL\":%d,\"vR\":%d,\"dt\":%u,",
          static_cast<double>(f.pos), static_cast<double>(f.corr),
          clampInt(f.vL, -9999, 9999), clampInt(f.vR, -9999, 9999),
          static_cast<unsigned>(f.dtUs));
    w.add("\"rpmL\":%d,\"rpmR\":%d,\"spL\":%d,\"spR\":%d,\"s\":[",
          clampInt(f.rpmL, -9999, 9999), clampInt(f.rpmR, -9999, 9999),
          clampInt(f.spL, -9999, 9999), clampInt(f.spR, -9999, 9999));
    for (int i = 0; i < 8; i++) {
        w.add(i == 0 ? "%u" : ",%u", static_cast<unsigned>(std::min<uint8_t>(f.s[i], 100)));
    }
    w.add("],\"st\":%u,\"lost\":%u}", static_cast<unsigned>(f.st), f.lost ? 1u : 0u);
    return w.ok();
}

bool formatStatus(char* buf, size_t n, const StatusFrame& s) {
    Writer w(buf, n);
    w.add("{\"t\":\"sta\",\"bat\":%u,\"vb\":%.2f,\"lap\":%.2f,\"laps\":[",
          static_cast<unsigned>(s.bat), static_cast<double>(s.vb), static_cast<double>(s.lap));
    for (uint8_t i = 0; i < s.nLaps && i < 5; i++) {
        w.add(i == 0 ? "%.2f" : ",%.2f", static_cast<double>(s.laps[i]));
    }
    w.add("],\"err\":%u,\"prof\":%u,\"dirty\":%u,\"ovr\":%u}",
          static_cast<unsigned>(s.err), static_cast<unsigned>(s.prof),
          s.dirty ? 1u : 0u, static_cast<unsigned>(s.ovr));
    return w.ok();
}
