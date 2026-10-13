#include "motors/OutputStage.h"
#include <algorithm>
#include <cmath>

static float clampUnit(float v) {
    if (!std::isfinite(v)) return 0.0f;
    return std::max(-1.0f, std::min(1.0f, v));
}

MotorDuties OutputStage::compute(float uLeft, float uRight, float vbat,
                                 const HardwareConfig& hw, const TuningProfile& tune) const {
    float k = 1.0f;
    if (tune.vcomp && std::isfinite(vbat) && vbat >= MIN_VALID_VBAT) k = tune.vref / vbat;
    float l = clampUnit(clampUnit(uLeft) * k);
    float r = clampUnit(clampUnit(uRight) * k);
    if (hw.invL) l = -l;
    if (hw.invR) r = -r;
    const int dl = static_cast<int>(std::lround(l * static_cast<float>(_pwmMax)));
    const int dr = static_cast<int>(std::lround(r * static_cast<float>(_pwmMax)));
    return hw.swap ? MotorDuties{dr, dl} : MotorDuties{dl, dr};
}
