#pragma once

class DifferentialDrive {
public:
    explicit DifferentialDrive(int maxPwm);

    // Converte correção PID e velocidade base em PWM L/R
    // correction: saída do PID (positivo = virar direita)
    // baseSpeed: velocidade linear desejada
    // leftPwm/rightPwm: saída [-maxPwm, +maxPwm]
    void compute(float correction, int baseSpeed, int& leftPwm, int& rightPwm) const;

    void setMaxPwm(int maxPwm);

private:
    int _maxPwm;

    int _clamp(int value, int min, int max) const;
};
