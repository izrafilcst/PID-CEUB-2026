#include "control/PIDController.h"
#include <algorithm>
#include <cmath>

PIDController::PIDController(float kp, float ki, float kd, float outMin, float outMax)
    : _kp(kp), _ki(ki), _kd(kd), _outMin(outMin), _outMax(outMax) {}

float PIDController::compute(float setpoint, float measurement, float dtS) {
    if (!std::isfinite(setpoint) || !std::isfinite(measurement)) return 0.0f;
    const bool dtOk = std::isfinite(dtS) && dtS > 0.0f;
    const float error = setpoint - measurement;

    // Derivada na medição, filtrada — não reage a saltos de setpoint
    if (dtOk) {
        const float dRaw = -(measurement - _prevMeasurement) / dtS;
        _dFiltered = _alpha * dRaw + (1.0f - _alpha) * _dFiltered;
    }
    _prevMeasurement = measurement;

    const float p = _kp * error;
    const float d = _kd * _dFiltered;

    if (_ki > 0.0f && dtOk) {
        const float unsat = p + _ki * _integral + d;
        const bool pushesHigh = unsat >= _outMax && error > 0.0f;
        const bool pushesLow  = unsat <= _outMin && error < 0.0f;
        if (!pushesHigh && !pushesLow) _integral += error * dtS;
        _integral = _clamp(_integral, _outMin / _ki, _outMax / _ki);
    }

    return _clamp(p + _ki * _integral + d, _outMin, _outMax);
}

void PIDController::reset(float measurement) {
    _integral = 0.0f;
    _dFiltered = 0.0f;
    _prevMeasurement = std::isfinite(measurement) ? measurement : 0.0f;
}

void PIDController::setTunings(float kp, float ki, float kd) {
    if (ki <= 0.0f) _integral = 0.0f;  // integral velha não vale com Ki novo nulo
    _kp = kp;
    _ki = ki;
    _kd = kd;
}

void PIDController::setDerivativeFilter(float alpha) {
    if (!std::isfinite(alpha)) return;
    _alpha = _clamp(alpha, 0.01f, 1.0f);
}

void PIDController::setOutputLimits(float outMin, float outMax) {
    _outMin = outMin;
    _outMax = outMax;
}

float PIDController::_clamp(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}
