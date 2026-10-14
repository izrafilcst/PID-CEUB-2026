#pragma once
#include "control/PIDController.h"

/** Comando por roda produzido pela cascata. Frações em [-1, +1]; setpoints em RPM. */
struct WheelCommand {
    float uL;
    float uR;
    float spL;
    float spR;
};

/**
 * Malha interna de velocidade, um PID por motor.
 *
 *   base, u (fração) → mixSteering → fL, fR → setpoint = f × rpmMax
 *   saída = feedforward f + PID(setpoint − rpm medido)
 *
 * O feedforward entrega o duty de regime; o PID só corrige o erro (F-08). Os limites do PID
 * são deslocados pelo feedforward para o anti-windup enxergar a saturação total.
 */
class CascadeController {
public:
    CascadeController(PIDController& innerLeft, PIDController& innerRight);

    WheelCommand compute(float base, float u, float rpmMax,
                         float rpmL, float rpmR, float dtS);

    void setInnerGains(float kp, float ki, float kd);

    /** Clears both inner loops, aligning derivative memory to the measured RPMs. */
    void reset(float rpmL, float rpmR);

private:
    static float _wheel(PIDController& pid, float f, float rpmMax, float rpm,
                        float dtS, float& setpoint);

    PIDController& _pidL;
    PIDController& _pidR;
};
