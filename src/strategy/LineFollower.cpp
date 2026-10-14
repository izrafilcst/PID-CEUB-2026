#include "strategy/LineFollower.h"
#include <algorithm>

LineFollower::LineFollower(Calibration& cal, PIDController& pid)
    : _cal(cal), _pid(pid),
      _positionMax((cal.count() - 1) * Calibration::POSITION_SCALE / 2.0f) {}

float LineFollower::_errorFrom(const int* rawSensors, bool& lost) {
    _cal.normalize(rawSensors, _normalized);
    const float position = _cal.weightedPosition(_normalized);
    lost = _cal.isLineLost();
    if (lost) {
        if (_lastValidError > 0.0f) return 1.0f;
        if (_lastValidError < 0.0f) return -1.0f;
        return 0.0f;
    }
    float e = (_positionMax > 0.0f) ? position / _positionMax : 0.0f;
    if (_sensorInvert) e = -e;
    e = std::max(-1.0f, std::min(1.0f, e));
    _lastValidError = e;
    return e;
}

LineReading LineFollower::update(const int* rawSensors, float dtS) {
    bool lost = false;
    const float e = _errorFrom(rawSensors, lost);
    // Setpoint 0: o PID devolve -kp·e; o sinal é invertido para que u > 0 vire à direita
    const float u = -_pid.compute(0.0f, e, dtS);
    return LineReading{e, u, lost};
}

float LineFollower::peekError(const int* rawSensors) {
    bool lost = false;
    return _errorFrom(rawSensors, lost);
}

void LineFollower::reset(float currentError) {
    _pid.reset(currentError);
    _lastValidError = currentError;
}
