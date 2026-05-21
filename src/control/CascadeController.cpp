// src/control/CascadeController.cpp
#include "control/CascadeController.h"
#include <algorithm>

CascadeController::CascadeController(PIDController& innerLeft, PIDController& innerRight,
                                     float maxRpm, int maxPwm)
    : _pidL(innerLeft), _pidR(innerRight), _maxRpm(maxRpm), _maxPwm(maxPwm) {}

void CascadeController::compute(float correction, float targetBaseRpm,
                                float actualRpmL, float actualRpmR,
                                int& pwmL, int& pwmR) {
    // 1. Traduz correction + base em setpoints por motor.
    float setpointL = targetBaseRpm + correction;
    float setpointR = targetBaseRpm - correction;

    // 2. Clamp dos setpoints (proteção contra over-PID externo).
    setpointL = _clampF(setpointL, -_maxRpm, +_maxRpm);
    setpointR = _clampF(setpointR, -_maxRpm, +_maxRpm);

    // 3. PID interno: erro = setpoint − measurement → saída em PWM.
    float outL = _pidL.compute(setpointL, actualRpmL);
    float outR = _pidR.compute(setpointR, actualRpmR);

    // 4. Saturação final em PWM (já vem clamped pelo PIDController, mas garantimos
    //    cast seguro para int e respeito ao limite externo se setMaxPwm() mudou).
    outL = _clampF(outL, -static_cast<float>(_maxPwm), +static_cast<float>(_maxPwm));
    outR = _clampF(outR, -static_cast<float>(_maxPwm), +static_cast<float>(_maxPwm));
    pwmL = static_cast<int>(outL);
    pwmR = static_cast<int>(outR);
}

void CascadeController::setInnerGains(float kp, float ki, float kd) {
    _pidL.setTunings(kp, ki, kd);
    _pidR.setTunings(kp, ki, kd);
}

void CascadeController::setMaxRpm(float maxRpm) { _maxRpm = maxRpm; }
void CascadeController::setMaxPwm(int   maxPwm) { _maxPwm = maxPwm; }

float CascadeController::_clampF(float v, float lo, float hi) const {
    return std::max(lo, std::min(hi, v));
}
