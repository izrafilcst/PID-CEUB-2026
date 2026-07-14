#pragma once
#include "control/PIDController.h"

/**
 * Controlador em cascata para diferencial duplo:
 *
 *   correction (do PID externo de posição)
 *        ↓
 *   tradução: setpointL = base + correction,  setpointR = base − correction
 *        ↓
 *   2× PID interno (compute(setpoint, actualRpm)) → PWM L/R
 *
 * O cliente é responsável por medir `actualRpmL/R` (ex: VelocityEstimator)
 * e passar via `compute()`. Isso desacopla a classe do hardware e facilita
 * testes determinísticos.
 *
 * Saturação:
 *  - setpoint clamped em [−maxRpm, +maxRpm] antes de entrar no PID interno
 *  - PWM já é clamped pelos limites do PIDController interno
 *
 * Convenções de sinal:
 *  - correction > 0  → curva para a direita (motor L mais rápido que R)
 *  - PWM > 0         → frente; PWM < 0 → ré (MotorDriver decide direção)
 */
class CascadeController {
public:
    CascadeController(PIDController& innerLeft, PIDController& innerRight,
                      float maxRpm, int maxPwm);

    // correction      : saída do PID externo (em unidade de RPM diferencial)
    // targetBaseRpm   : alvo de velocidade linear do robô (em RPM)
    // actualRpmL/R    : RPM real medido (ex: VelocityEstimator::getRPM())
    // pwmL/pwmR       : saídas (referência), com sinal
    void compute(float correction, float targetBaseRpm,
                 float actualRpmL, float actualRpmR,
                 int& pwmL, int& pwmR, float dtSec = 0.002f);

    // Aplica os mesmos ganhos em ambos os PIDs internos (uso típico via BLE).
    void setInnerGains(float kp, float ki, float kd);

    void setMaxRpm(float maxRpm);
    void setMaxPwm(int   maxPwm);

private:
    PIDController& _pidL;
    PIDController& _pidR;
    float _maxRpm;
    int   _maxPwm;

    float _clampF(float v, float lo, float hi) const;
};
