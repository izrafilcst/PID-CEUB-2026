#include "strategy/LineFollower.h"
#include <cmath>

LineFollower::LineFollower(Calibration& cal, PIDController& pid,
                           SpeedProfile& speed, DifferentialDrive& drive)
    : _cal(cal), _pid(pid), _speed(speed), _drive(drive) {}

void LineFollower::update(const int* rawSensors, int& leftPwm, int& rightPwm,
                          float* normalizedError, float dtSec) {
    _cal.normalize(rawSensors, _normalized);
    float position = _cal.weightedPosition(_normalized);

    // Normaliza para [-1, +1] com base no range máximo de posição
    float normErr = position / POSITION_MAX;
    if (normErr >  1.0f) normErr =  1.0f;
    if (normErr < -1.0f) normErr = -1.0f;

    if (normalizedError) *normalizedError = normErr;

    // Sinal positivo de correção → virar direita → PID inverte sinal do erro
    float correction = -_pid.compute(0.0f, position, dtSec);
    _lastCorrection = correction;  // expose for cascade integration
    int baseSpeed = _speed.compute(std::abs(normErr));
    _drive.compute(correction, baseSpeed, leftPwm, rightPwm);
}

void LineFollower::setPID(float kp, float ki, float kd) {
    _pid.setTunings(kp, ki, kd);
    _pid.reset();
}

void LineFollower::setSpeed(int minSpeed, int baseSpeed, int maxSpeed, float threshold) {
    _speed.setParams(minSpeed, baseSpeed, maxSpeed, threshold);
}

void LineFollower::reset() {
    _pid.reset();
    _cal.reset();
}
