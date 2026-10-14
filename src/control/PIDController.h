#pragma once

/**
 * PID genérico com derivada na medição.
 *
 * - dt real em segundos a cada chamada (F-07).
 * - Filtro passa-baixa de 1ª ordem no termo D: alpha = 1 desliga o filtro.
 * - reset(medida) alinha a memória da derivada à medida atual (F-12).
 * - Anti-windup: integração condicional (não integra enquanto a saída já está saturada no
 *   mesmo sentido do erro) + clamp da integral.
 */
class PIDController {
public:
    PIDController(float kp, float ki, float kd, float outMin, float outMax);

    /** Computes the output. Non-finite inputs return 0; dtS <= 0 freezes I and D. */
    float compute(float setpoint, float measurement, float dtS);

    /** Clears integral and D filter; aligns derivative memory to `measurement`. */
    void reset(float measurement = 0.0f);

    void setTunings(float kp, float ki, float kd);
    /** alpha in (0, 1]; 1 = no filtering. Out-of-range values are clamped. */
    void setDerivativeFilter(float alpha);
    void setOutputLimits(float outMin, float outMax);

    float getKp() const { return _kp; }
    float getKi() const { return _ki; }
    float getKd() const { return _kd; }

private:
    float _kp, _ki, _kd;
    float _outMin, _outMax;
    float _alpha = 1.0f;
    float _integral = 0.0f;
    float _prevMeasurement = 0.0f;
    float _dFiltered = 0.0f;

    static float _clamp(float v, float lo, float hi);
};
