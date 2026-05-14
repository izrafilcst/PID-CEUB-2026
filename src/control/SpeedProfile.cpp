#include "control/SpeedProfile.h"
#include <cmath>
#include <algorithm>

SpeedProfile::SpeedProfile(int minSpeed, int baseSpeed, int maxSpeed, float errorThreshold)
    : _minSpeed(minSpeed), _baseSpeed(baseSpeed), _maxSpeed(maxSpeed), _threshold(errorThreshold) {}

int SpeedProfile::compute(float normalizedError) const {
    float absError = std::abs(normalizedError);

    if (absError <= 0.0f) {
        return _maxSpeed;
    }
    if (absError >= _threshold) {
        return _minSpeed;
    }
    // interpolacao linear: 0 -> maxSpeed, threshold -> minSpeed
    float t = absError / _threshold;
    float speed = _maxSpeed + t * (_minSpeed - _maxSpeed);
    return std::max(_minSpeed, std::min(_maxSpeed, static_cast<int>(speed)));
}

void SpeedProfile::setParams(int minSpeed, int baseSpeed, int maxSpeed, float errorThreshold) {
    _minSpeed = minSpeed;
    _baseSpeed = baseSpeed;
    _maxSpeed = maxSpeed;
    _threshold = errorThreshold;
}
