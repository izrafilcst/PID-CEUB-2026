#pragma once
#include "sensors/Calibration.h"
#include "control/PIDController.h"
#include "control/SpeedProfile.h"
#include "motors/DifferentialDrive.h"

class LineFollower {
public:
    LineFollower(Calibration& cal, PIDController& pid,
                 SpeedProfile& speed, DifferentialDrive& drive);

    // Executa um ciclo completo: raw → pwm L/R
    // normalizedError retorna erro em [-1, +1] para diagnóstico
    void update(const int* rawSensors, int& leftPwm, int& rightPwm,
                float* normalizedError = nullptr);

    void setPID(float kp, float ki, float kd);
    void setSpeed(int minSpeed, int baseSpeed, int maxSpeed, float threshold);
    void reset();

    /** Returns the raw PID correction from the last update() call.
     *  Used by CascadeController to recover correction without re-deriving
     *  it from clamped PWM outputs (which would be lossy when motors saturate). */
    float getLastCorrection() const { return _lastCorrection; }

private:
    Calibration&       _cal;
    PIDController&     _pid;
    SpeedProfile&      _speed;
    DifferentialDrive& _drive;

    static constexpr float POSITION_MAX = 3500.0f;
    int   _normalized[8];
    float _lastCorrection = 0.0f;
};
