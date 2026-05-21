// src/sensors/VelocityEstimator.cpp
#include "sensors/VelocityEstimator.h"
#include "config.h"
#include <cstdio>
#include <cmath>

VelocityEstimator::VelocityEstimator(Encoder& enc, IPersistentStore& store, const char* keyPrefix)
    : _enc(enc), _store(store), _keyPrefix(keyPrefix),
      _pprX4(ENCODER_DEFAULT_PPR_X4),
      _alpha(VELOCITY_FILTER_ALPHA),
      _rpmFiltered(0.0f)
{
    snprintf(_key, sizeof(_key), "%s_ppr", _keyPrefix);
}

void VelocityEstimator::begin() {
    if (_store.exists(_key)) {
        _pprX4 = _store.getFloat(_key, ENCODER_DEFAULT_PPR_X4);
    } else {
        _pprX4 = ENCODER_DEFAULT_PPR_X4;
    }
    _rpmFiltered = 0.0f;
    // Discard accumulated encoder delta before first use — intentional.
    // Without this, stale counts from before begin() would corrupt the first
    // RPM reading. Future maintainers: do NOT remove this call.
    _enc.getDelta();
}

void VelocityEstimator::update(uint32_t dtMs) {
    if (dtMs == 0 || _pprX4 <= 0.0f) {
        // Sem dt válido — mantém valor filtrado anterior, sem novo cálculo
        return;
    }
    int32_t delta = _enc.getDelta();
    // pulsos/ms → rotações/ms → rotações/min: ×60_000 / pprX4
    float rpmRaw = (static_cast<float>(delta) * 60000.0f)
                 / (static_cast<float>(dtMs) * _pprX4);
    _rpmFiltered = _alpha * rpmRaw + (1.0f - _alpha) * _rpmFiltered;
}

float VelocityEstimator::getRPM() const {
    return _rpmFiltered;
}

float VelocityEstimator::getEffectivePPR() const {
    return _pprX4;
}

void VelocityEstimator::setFilterAlpha(float alpha) {
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    _alpha = alpha;
}

void VelocityEstimator::startCalibration() {
    _enc.reset();
    _rpmFiltered = 0.0f;
}

bool VelocityEstimator::finishCalibration(int rotations) {
    if (rotations <= 0) return false;
    int32_t count = _enc.getCount();
    if (count == 0) return false;
    float absCount = static_cast<float>(std::abs(count));
    _pprX4 = absCount / static_cast<float>(rotations);
    _store.putFloat(_key, _pprX4);
    _rpmFiltered = 0.0f;
    // Sync the delta baseline so the next update() starts from zero pulses.
    // Without this, calibration counts would bleed into the first RPM reading.
    _enc.getDelta();
    return true;
}
