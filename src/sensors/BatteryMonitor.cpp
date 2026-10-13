#include "sensors/BatteryMonitor.h"
#include <cmath>

namespace {

struct Point {
    float v;
    float pct;
};

// Curvas de 2S em repouso; valores típicos de mercado (só para indicação)
const Point LIION[] = {{6.0f, 0}, {6.8f, 10}, {7.2f, 40}, {7.6f, 70}, {8.0f, 90}, {8.4f, 100}};
const Point LIPO[]  = {{6.6f, 0}, {7.2f, 15}, {7.4f, 40}, {7.7f, 70}, {8.0f, 90}, {8.4f, 100}};

uint8_t interpolate(const Point* pts, int n, float v) {
    if (!std::isfinite(v) || v <= pts[0].v) return 0;
    if (v >= pts[n - 1].v) return 100;
    for (int i = 1; i < n; i++) {
        if (v <= pts[i].v) {
            const float t = (v - pts[i - 1].v) / (pts[i].v - pts[i - 1].v);
            const float pct = pts[i - 1].pct + t * (pts[i].pct - pts[i - 1].pct);
            return static_cast<uint8_t>(std::lround(pct));
        }
    }
    return 100;
}

}  // namespace

BatteryMonitor::BatteryMonitor(float dividerRatio, float tauS, uint32_t lowHoldMs)
    : _ratio(dividerRatio), _tauS(tauS > 0.0f ? tauS : 0.001f), _lowHoldMs(lowHoldMs) {}

void BatteryMonitor::update(uint32_t adcMilliVolts, float dtS, uint32_t nowMs,
                            const HardwareConfig& hw) {
    const float v = static_cast<float>(adcMilliVolts) / 1000.0f * _ratio * hw.vbk;
    if (!_has) {
        _volts = v;
        _has = true;
    } else {
        const float dt = (std::isfinite(dtS) && dtS > 0.0f) ? dtS : 0.0f;
        _volts += (dt / (_tauS + dt)) * (v - _volts);
    }
    if (_volts < hw.vcut) {
        if (!_below) {
            _below = true;
            _belowSince = nowMs;
        }
        _low = nowMs - _belowSince >= _lowHoldMs;
    } else {
        _below = false;
        _low = false;
    }
}

uint8_t BatteryMonitor::percentFor(float volts, uint8_t chem) {
    return chem == 0 ? interpolate(LIPO, 6, volts) : interpolate(LIION, 6, volts);
}
