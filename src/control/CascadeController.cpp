#include "control/CascadeController.h"
#include "control/Mixer.h"
#include <algorithm>
#include <cmath>

static float finiteOrZero(float v) { return std::isfinite(v) ? v : 0.0f; }

CascadeController::CascadeController(PIDController& innerLeft, PIDController& innerRight)
    : _pidL(innerLeft), _pidR(innerRight) {}

float CascadeController::_wheel(PIDController& pid, float f, float rpmMax, float rpm,
                                float dtS, float& setpoint) {
    setpoint = f * rpmMax;
    pid.setOutputLimits(-1.0f - f, 1.0f - f);
    const float out = f + pid.compute(setpoint, rpm, dtS);
    if (!std::isfinite(out)) return 0.0f;
    return std::max(-1.0f, std::min(1.0f, out));
}

WheelCommand CascadeController::compute(float base, float u, float rpmMax,
                                        float rpmL, float rpmR, float dtS) {
    WheelCommand cmd{0.0f, 0.0f, 0.0f, 0.0f};
    if (!std::isfinite(rpmMax) || rpmMax <= 0.0f) return cmd;
    float fL = 0.0f;
    float fR = 0.0f;
    mixSteering(finiteOrZero(base), finiteOrZero(u), fL, fR);
    cmd.uL = _wheel(_pidL, fL, rpmMax, finiteOrZero(rpmL), dtS, cmd.spL);
    cmd.uR = _wheel(_pidR, fR, rpmMax, finiteOrZero(rpmR), dtS, cmd.spR);
    return cmd;
}

void CascadeController::setInnerGains(float kp, float ki, float kd) {
    _pidL.setTunings(kp, ki, kd);
    _pidR.setTunings(kp, ki, kd);
}

void CascadeController::reset(float rpmL, float rpmR) {
    _pidL.reset(finiteOrZero(rpmL));
    _pidR.reset(finiteOrZero(rpmR));
}
