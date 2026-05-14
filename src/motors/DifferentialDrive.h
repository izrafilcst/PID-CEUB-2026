#pragma once

class DifferentialDrive {
public:
    DifferentialDrive(int baseSpeed, int minSpeed, int maxPwm);

    // Converte correção PID e velocidade base em PWM L/R
    // correction: saída do PID (positivo = virar direita)
    // baseSpeed: velocidade linear desejada
    // leftPwm/rightPwm: saída [-maxPwm, +maxPwm]
    void compute(float correction, int baseSpeed, int& leftPwm, int& rightPwm) const;

    void setParams(int baseSpeed, int minSpeed, int maxPwm);

private:
    int _baseSpeed;
    int _minSpeed;
    int _maxPwm;

    int _clamp(int value, int min, int max) const;
};
