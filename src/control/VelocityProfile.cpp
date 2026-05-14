#include "control/VelocityProfile.h"
#include <cmath>
#include <algorithm>

VelocityProfile::VelocityProfile(int minSpeed, int baseSpeed, int maxSpeed,
                                 float errorThreshold, float curvatureWeight)
    : _head(0), _count(0),
      _minSpeed(minSpeed), _baseSpeed(baseSpeed), _maxSpeed(maxSpeed),
      _threshold(errorThreshold), _curvatureWeight(curvatureWeight)
{
    for (int i = 0; i < BUFFER_SIZE; i++) {
        _buf[i] = {0.0f, 0u};
    }
}

void VelocityProfile::update(float position, uint32_t timestampMs) {
    _buf[_head] = {position, timestampMs};
    _head = (_head + 1) % BUFFER_SIZE;
    if (_count < BUFFER_SIZE) _count++;
}

float VelocityProfile::getCurvatureScore() const {
    if (_count < 2) return 0.0f;

    float totalVelocity = 0.0f;
    int   pairs = 0;

    // Iterate oldest→newest: oldest index = (_head - _count + BUFFER_SIZE) % BUFFER_SIZE
    for (int k = 0; k < _count - 1; k++) {
        int iPrev = (_head - _count + k     + BUFFER_SIZE) % BUFFER_SIZE;
        int iCurr = (_head - _count + k + 1 + BUFFER_SIZE) % BUFFER_SIZE;

        float dp = std::abs(_buf[iCurr].pos - _buf[iPrev].pos);
        uint32_t dt = _buf[iCurr].ts - _buf[iPrev].ts;

        if (dt > 0) {
            totalVelocity += dp / static_cast<float>(dt);
            pairs++;
        }
    }

    if (pairs == 0) return 0.0f;

    float avgVelocity = totalVelocity / static_cast<float>(pairs);
    return std::min(1.0f, avgVelocity / CURVATURE_NORM);
}

int VelocityProfile::computeSpeed() const {
    if (_count == 0) return _baseSpeed;

    // Last sample = current position
    int lastIdx = (_head - 1 + BUFFER_SIZE) % BUFFER_SIZE;
    float errorNorm = std::abs(_buf[lastIdx].pos) / POSITION_MAX;

    // Error-based speed (same formula as SpeedProfile)
    int errorSpeed;
    if (errorNorm <= 0.0f) {
        errorSpeed = _maxSpeed;
    } else if (errorNorm >= _threshold) {
        errorSpeed = _minSpeed;
    } else {
        float t = errorNorm / _threshold;
        errorSpeed = static_cast<int>(_maxSpeed + t * (_minSpeed - _maxSpeed));
    }

    // Curvature-based effective max speed
    float curvScore = getCurvatureScore();
    float curvReduction = curvScore * _curvatureWeight * static_cast<float>(_maxSpeed - _minSpeed);
    int effectiveMax = static_cast<int>(_maxSpeed - curvReduction);

    int speed = std::min(errorSpeed, effectiveMax);
    return std::max(_minSpeed, std::min(_maxSpeed, speed));
}

void VelocityProfile::setParams(int minSpeed, int baseSpeed, int maxSpeed, float errorThreshold) {
    _minSpeed  = minSpeed;
    _baseSpeed = baseSpeed;
    _maxSpeed  = maxSpeed;
    _threshold = errorThreshold;
}
