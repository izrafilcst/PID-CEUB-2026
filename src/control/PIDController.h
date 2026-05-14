#pragma once

class PIDController {
public:
    PIDController(float kp, float ki, float kd, float outMin, float outMax, float dt = 0.01f);

    float compute(float setpoint, float measurement);
    float compute(float setpoint, float measurement, float dt);
    void reset();
    void setTunings(float kp, float ki, float kd);
    void setOutputLimits(float min, float max);

    float getKp() const { return _kp; }
    float getKi() const { return _ki; }
    float getKd() const { return _kd; }

private:
    float _kp, _ki, _kd;
    float _outMin, _outMax;
    float _dt;
    float _integral;
    float _prevMeasurement;

    float _clamp(float value, float min, float max);
};
