// src/control/CascadeController.cpp
#include "control/CascadeController.h"
#include <algorithm>
#include <cmath>

CascadeController::CascadeController(PIDController& innerLeft, PIDController& innerRight,
                                     float maxRpm, int maxPwm)
    : _pidL(innerLeft), _pidR(innerRight), _maxRpm(maxRpm), _maxPwm(maxPwm) {}

void CascadeController::compute(float correction, float targetBaseRpm,
                                float actualRpmL, float actualRpmR,
                                int& pwmL, int& pwmR) {
    // 0. Defense-in-depth: NaN/Inf em qualquer entrada vira 0.0f.
    // std::max/std::min com NaN retornam NaN silenciosamente → static_cast<int>(NaN)
    // é UB. Sanitização explícita evita o cast UB mesmo se a chain externa falhar.
    if (!std::isfinite(correction))    correction    = 0.0f;
    if (!std::isfinite(targetBaseRpm)) targetBaseRpm = 0.0f;
    if (!std::isfinite(actualRpmL))    actualRpmL    = 0.0f;
    if (!std::isfinite(actualRpmR))    actualRpmR    = 0.0f;

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
    // Guarda final contra NaN remanescente (PID interno pode produzir se ki=0 e
    // _prevMeasurement vier corrompido) — std::isfinite garante cast seguro.
    if (!std::isfinite(outL)) outL = 0.0f;
    if (!std::isfinite(outR)) outR = 0.0f;
    pwmL = static_cast<int>(outL);
    pwmR = static_cast<int>(outR);
}

void CascadeController::setInnerGains(float kp, float ki, float kd) {
    _pidL.setTunings(kp, ki, kd);
    _pidR.setTunings(kp, ki, kd);
    // MS-2: reset() limpa _prevMeasurement e _integral.
    // Sem isso, BLE retune com motores rodando emite um pico de derivada
    // (Kd × erro stale) que pode descontrolar a malha por algumas iterações.
    _pidL.reset();
    _pidR.reset();
}

void CascadeController::setMaxRpm(float maxRpm) { _maxRpm = maxRpm; }
void CascadeController::setMaxPwm(int   maxPwm) { _maxPwm = maxPwm; }

float CascadeController::_clampF(float v, float lo, float hi) const {
    return std::max(lo, std::min(hi, v));
}
