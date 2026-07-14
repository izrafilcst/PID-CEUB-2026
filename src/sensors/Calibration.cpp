#include "sensors/Calibration.h"
#include <algorithm>
#include <cmath>

Calibration::Calibration(int sensorCount)
    : _count(sensorCount), _lastPosition(0.0f), _lineLost(false), _crossing(false) {
    assert(sensorCount > 0 && sensorCount <= MAX_SENSORS);
    reset();
}

void Calibration::reset() {
    for (int i = 0; i < _count; i++) {
        _min[i] = INT_MAX;
        _max[i] = INT_MIN;
    }
    _lastPosition = 0.0f;
    _lineLost = false;
    _crossing = false;
}

int Calibration::getMin(int idx) const { return _min[idx]; }
int Calibration::getMax(int idx) const { return _max[idx]; }

void Calibration::update(const int* rawValues) {
    for (int i = 0; i < _count; i++) {
        _min[i] = std::min(_min[i], rawValues[i]);
        _max[i] = std::max(_max[i], rawValues[i]);
    }
}

void Calibration::normalize(const int* rawValues, int* normalizedOut) const {
    for (int i = 0; i < _count; i++) {
        int range = _max[i] - _min[i];
        if (range <= 0) {
            normalizedOut[i] = 0;
        } else {
            int val = (rawValues[i] - _min[i]) * NORM_MAX / range;
            normalizedOut[i] = std::max(0, std::min(NORM_MAX, val));
        }
    }
}

float Calibration::weightedPosition(const int* normalized) const {
    long sum = 0;
    long weightedSum = 0;
    int  activeCount = 0;
    const int halfRange = (_count - 1) * static_cast<int>(POSITION_SCALE) / 2;

    for (int i = 0; i < _count; i++) {
        int pos = i * static_cast<int>(POSITION_SCALE) - halfRange;
        sum += normalized[i];
        weightedSum += static_cast<long>(pos) * normalized[i];
        if (normalized[i] >= CROSSING_ACTIVE_LEVEL) activeCount++;
    }

    if (sum < LINE_LOST_THRESHOLD) {
        _lineLost = true;
        _crossing = false;
        return _lastPosition;
    }

    _lineLost = false;
    _crossing = (activeCount >= CROSSING_MIN_ACTIVE);
    _lastPosition = static_cast<float>(weightedSum) / static_cast<float>(sum);
    return _lastPosition;
}
