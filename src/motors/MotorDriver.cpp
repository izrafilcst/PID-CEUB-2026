#include "motors/MotorDriver.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>

MotorDriver::MotorDriver(const Config& configA, const Config& configB, uint8_t stbyPin)
    : _cfgA(configA), _cfgB(configB), _stbyPin(stbyPin), _initialized(false) {}

void MotorDriver::begin() {
    pinMode(_stbyPin, OUTPUT);
    digitalWrite(_stbyPin, HIGH);  // sai do standby

    auto initChannel = [](const Config& cfg) {
        ledcSetup(cfg.ledcChannel, cfg.pwmFreq, cfg.pwmResolution);
        ledcAttachPin(cfg.pwmPin, cfg.ledcChannel);
        pinMode(cfg.in1Pin, OUTPUT);
        pinMode(cfg.in2Pin, OUTPUT);
    };
    initChannel(_cfgA);
    initChannel(_cfgB);
    _initialized = true;
    stop();
}

void MotorDriver::_writePwm(const Config& cfg, uint32_t duty) {
    ledcWrite(cfg.ledcChannel, duty);
}

void MotorDriver::_applyMotor(const Config& cfg, int pwm) {
    if (pwm > 0) {
        digitalWrite(cfg.in1Pin, HIGH);
        digitalWrite(cfg.in2Pin, LOW);
        _writePwm(cfg, static_cast<uint32_t>(pwm));
    } else if (pwm < 0) {
        digitalWrite(cfg.in1Pin, LOW);
        digitalWrite(cfg.in2Pin, HIGH);
        _writePwm(cfg, static_cast<uint32_t>(-pwm));
    } else {
        // pwm == 0: brake
        digitalWrite(cfg.in1Pin, HIGH);
        digitalWrite(cfg.in2Pin, HIGH);
        _writePwm(cfg, 0);
    }
}

void MotorDriver::setSpeed(MotorId motor, int pwm) {
    // clamp
    if (pwm >  1023) pwm =  1023;
    if (pwm < -1023) pwm = -1023;
    _applyMotor(motor == MotorId::A ? _cfgA : _cfgB, pwm);
}

void MotorDriver::brake(MotorId motor) {
    const Config& cfg = (motor == MotorId::A) ? _cfgA : _cfgB;
    digitalWrite(cfg.in1Pin, HIGH);
    digitalWrite(cfg.in2Pin, HIGH);
    _writePwm(cfg, 0);
}

void MotorDriver::coast(MotorId motor) {
    const Config& cfg = (motor == MotorId::A) ? _cfgA : _cfgB;
    digitalWrite(cfg.in1Pin, LOW);
    digitalWrite(cfg.in2Pin, LOW);
    _writePwm(cfg, 0);
}

void MotorDriver::standby(bool enable) {
    digitalWrite(_stbyPin, enable ? LOW : HIGH);
}

void MotorDriver::stop() {
    brake(MotorId::A);
    brake(MotorId::B);
}

#else
// Stubs para compilação native (testes)
MotorDriver::MotorDriver(const Config& a, const Config& b, uint8_t s)
    : _cfgA(a), _cfgB(b), _stbyPin(s), _initialized(false) {}
void MotorDriver::begin() {}
void MotorDriver::setSpeed(MotorId, int) {}
void MotorDriver::brake(MotorId) {}
void MotorDriver::coast(MotorId) {}
void MotorDriver::standby(bool) {}
void MotorDriver::stop() {}
void MotorDriver::_applyMotor(const Config&, int) {}
void MotorDriver::_writePwm(const Config&, uint32_t) {}
#endif
