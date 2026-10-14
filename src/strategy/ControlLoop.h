#pragma once
#include <cstdint>
#include "config/RobotConfig.h"
#include "control/CascadeController.h"
#include "control/PIDController.h"
#include "control/SpeedProfile.h"
#include "motors/OutputStage.h"
#include "sensors/Calibration.h"
#include "strategy/LineFollower.h"

struct ControlInputs {
    const int* raw = nullptr;  ///< leituras brutas, Calibration::count() valores
    float rpmL = 0.0f;         ///< RPM medido (sinal do encoder ainda não corrigido)
    float rpmR = 0.0f;
    float vbat = 0.0f;         ///< V filtrado
    float dtS = 0.0f;          ///< dt real do ciclo, s
};

struct ControlOutputs {
    MotorDuties duties{0, 0};
    float error = 0.0f;
    float u = 0.0f;
    float base = 0.0f;
    float uL = 0.0f;
    float uR = 0.0f;
    float spL = 0.0f;   ///< RPM pedido (0 no modo PWM direto)
    float spR = 0.0f;
    bool  lost = false;
    bool  lostTimeout = false;
    bool  encoderFault = false;  ///< mode 1: duty alto com RPM ≈ 0 por ENC_FAULT_MS (E-4)
};

/**
 * Um passo do controle (spec §4.7): sensores → erro → PID externo → base com rampa →
 * mistura (PWM direto ou cascata) → OutputStage. Sem hardware: testável em native.
 */
class ControlLoop {
public:
    ControlLoop(Calibration& cal, int pwmMax);

    void applyConfig(const RobotConfig& cfg);
    void start(const int* raw, float rpmL, float rpmR);
    ControlOutputs step(const ControlInputs& in);

private:
    static float _signed(float rpm, uint8_t inverted) { return inverted ? -rpm : rpm; }
    bool _encoderStalled(const ControlInputs& in, const MotorDuties& d) const;

    Calibration&      _cal;
    PIDController     _outer;
    PIDController     _innerL;
    PIDController     _innerR;
    LineFollower      _line;
    SpeedProfile      _speed;
    CascadeController _cascade;
    OutputStage       _output;
    RobotConfig       _cfg;
    int               _pwmMax;
    uint32_t          _lostUs = 0;
    uint32_t          _encStallUs = 0;
};
