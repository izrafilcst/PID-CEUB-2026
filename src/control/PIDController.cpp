#include "control/PIDController.h"
#include <algorithm>

PIDController::PIDController(float kp, float ki, float kd, float outMin, float outMax, float dt)
    : _kp(kp), _ki(ki), _kd(kd), _outMin(outMin), _outMax(outMax), _dt(dt),
      _integral(0.0f), _prevMeasurement(0.0f) {}

float PIDController::compute(float setpoint, float measurement) {
    return compute(setpoint, measurement, _dt);
}

float PIDController::compute(float setpoint, float measurement, float dt) {
    float error = setpoint - measurement;

    // Integral com anti-windup por clamping
    _integral += error * dt;
    if (_ki != 0.0f) {
        _integral = _clamp(_integral, _outMin / _ki, _outMax / _ki);
    }

    // Derivada no processo — evita spike quando setpoint muda abruptamente
    float derivative = (dt > 0.0f) ? -(measurement - _prevMeasurement) / dt : 0.0f;
    _prevMeasurement = measurement;

    float output = _kp * error + _ki * _integral + _kd * derivative;
    return _clamp(output, _outMin, _outMax);
}

void PIDController::reset() {
    _integral = 0.0f;
    _prevMeasurement = 0.0f;
}

void PIDController::setTunings(float kp, float ki, float kd) {
    _kp = kp; _ki = ki; _kd = kd;
}

void PIDController::setOutputLimits(float min, float max) {
    _outMin = min; _outMax = max;
}

float PIDController::_clamp(float value, float min, float max) {
    return std::max(min, std::min(max, value));
}
