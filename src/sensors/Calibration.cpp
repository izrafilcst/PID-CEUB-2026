#include "sensors/Calibration.h"
#include <algorithm>

Calibration::Calibration(int sensorCount)
    : _count(sensorCount), _lastPosition(0.0f), _lineLost(false) {
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
}

int Calibration::getMin(int idx) const { return _min[idx]; }
int Calibration::getMax(int idx) const { return _max[idx]; }

int Calibration::_range(int i) const {
    // Canal nunca calibrado tem max < min. Comparar antes de subtrair evita o overflow de
    // INT_MIN − INT_MAX (F-02).
    if (_max[i] <= _min[i]) return 0;
    return _max[i] - _min[i];
}

void Calibration::update(const int* rawValues) {
    for (int i = 0; i < _count; i++) {
        _min[i] = std::min(_min[i], rawValues[i]);
        _max[i] = std::max(_max[i], rawValues[i]);
    }
}

void Calibration::normalize(const int* rawValues, int* normalizedOut) const {
    for (int i = 0; i < _count; i++) {
        const int range = _range(i);
        if (range <= 0) {
            normalizedOut[i] = 0;
            continue;
        }
        const long scaled = static_cast<long>(rawValues[i] - _min[i]) * NORM_MAX / range;
        const int v = static_cast<int>(std::max(0L, std::min(static_cast<long>(NORM_MAX), scaled)));
        normalizedOut[i] = _whiteLine ? NORM_MAX - v : v;
    }
}

float Calibration::weightedPosition(const int* normalized) const {
    long sum = 0;
    long weightedSum = 0;
    const int halfRange = (_count - 1) * static_cast<int>(POSITION_SCALE) / 2;

    for (int i = 0; i < _count; i++) {
        const int pos = i * static_cast<int>(POSITION_SCALE) - halfRange;
        sum += normalized[i];
        weightedSum += static_cast<long>(pos) * normalized[i];
    }

    if (sum < LINE_LOST_THRESHOLD) {
        _lineLost = true;
        return _lastPosition;
    }

    _lineLost = false;
    _lastPosition = static_cast<float>(weightedSum) / static_cast<float>(sum);
    return _lastPosition;
}

bool Calibration::isValid(uint8_t* badChannels, int* badCount) const {
    int n = 0;
    for (int i = 0; i < _count; i++) {
        if (_range(i) < MIN_VALID_RANGE) {
            if (badChannels) badChannels[n] = static_cast<uint8_t>(i);
            n++;
        }
    }
    if (badCount) *badCount = n;
    return n == 0;
}

static int16_t toInt16(int v) {
    return static_cast<int16_t>(std::max(-32768, std::min(32767, v)));
}

CalibrationData Calibration::exportData() const {
    CalibrationData d{};
    for (int i = 0; i < _count; i++) {
        if (_max[i] > _min[i]) {
            d.min[i] = toInt16(_min[i]);
            d.max[i] = toInt16(_max[i]);
        }
    }
    return d;
}

void Calibration::importData(const CalibrationData& data) {
    for (int i = 0; i < _count; i++) {
        if (data.max[i] > data.min[i]) {
            _min[i] = data.min[i];
            _max[i] = data.max[i];
        } else {
            _min[i] = INT_MAX;
            _max[i] = INT_MIN;
        }
    }
}
