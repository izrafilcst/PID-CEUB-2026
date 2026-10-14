#pragma once
#include "config/RobotConfig.h"

/** Duty com sinal por canal do TB6612 (A, B), em [-pwmMax, +pwmMax]. */
struct MotorDuties {
    int a;
    int b;
};

/**
 * Última etapa do laço (spec §4.7, passo 6): compensação de tensão (× vref/vbat), saturação
 * em [-1, +1], inversão por lado, troca de canal e conversão para duty inteiro.
 * Leitura de bateria abaixo de MIN_VALID_VBAT é tratada como falha: sem compensação.
 */
class OutputStage {
public:
    static constexpr float MIN_VALID_VBAT = 3.0f;

    explicit OutputStage(int pwmMax) : _pwmMax(pwmMax) {}

    MotorDuties compute(float uLeft, float uRight, float vbat,
                        const HardwareConfig& hw, const TuningProfile& tune) const;

private:
    int _pwmMax;
};
