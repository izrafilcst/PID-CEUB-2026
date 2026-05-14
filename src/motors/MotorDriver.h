#pragma once
#include <cstdint>

#ifndef NATIVE_BUILD
#include <Arduino.h>
#endif

enum class MotorId { A, B };
enum class MotorDir { FORWARD, REVERSE, BRAKE, COAST };

class MotorDriver {
public:
    struct Config {
        uint8_t pwmPin;
        uint8_t in1Pin;
        uint8_t in2Pin;
        uint8_t ledcChannel;
        uint32_t pwmFreq;
        uint8_t  pwmResolution;
    };

    MotorDriver(const Config& configA, const Config& configB, uint8_t stbyPin);

    void begin();
    void setSpeed(MotorId motor, int pwm);   // pwm: -1023..+1023 (negativo = reverso)
    void brake(MotorId motor);
    void coast(MotorId motor);
    void standby(bool enable);
    void stop();                             // brake em ambos

private:
    Config _cfgA, _cfgB;
    uint8_t _stbyPin;
    bool _initialized;

    void _applyMotor(const Config& cfg, int pwm);
    void _writePwm(const Config& cfg, uint32_t duty);
};
