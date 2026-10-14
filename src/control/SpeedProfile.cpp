#include "control/SpeedProfile.h"
#include <algorithm>
#include <cmath>

void SpeedProfile::setParams(float vmin, float vmax, float thr, float acc) {
    _vmin = vmin;
    _vmax = vmax;
    _thr = (thr > 0.0f) ? thr : 1.0f;
    _acc = (acc > 0.0f) ? acc : 0.0f;
}

float SpeedProfile::target(float absError) const {
    const float e = std::fabs(absError);
    if (!std::isfinite(e)) return _vmin;
    if (e <= 0.0f) return _vmax;
    if (e >= _thr) return _vmin;
    return _vmax + (e / _thr) * (_vmin - _vmax);
}

float SpeedProfile::step(float absError, float dtS) {
    const float tgt = target(absError);
    if (tgt <= _current) {
        _current = tgt;
        return _current;
    }
    const float dt = (std::isfinite(dtS) && dtS > 0.0f) ? dtS : 0.0f;
    _current = std::min(tgt, _current + _acc * dt);
    return _current;
}
