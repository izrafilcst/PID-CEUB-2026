#pragma once
#include <cstdint>
#include "config/RobotConfig.h"

/**
 * Tensão da bateria a partir do ADC (mV já calibrados pelo eFuse, F-10).
 * V = mV/1000 × divisor × vbk, filtrado por um passa-baixa de 1ª ordem (constante tauS).
 * isLow() só fica verdadeiro depois de lowHoldMs contínuos abaixo de vcut.
 */
class BatteryMonitor {
public:
    explicit BatteryMonitor(float dividerRatio, float tauS = 0.5f, uint32_t lowHoldMs = 1000);

    void update(uint32_t adcMilliVolts, float dtS, uint32_t nowMs, const HardwareConfig& hw);

    bool    hasReading() const { return _has; }
    float   volts() const { return _volts; }
    bool    isLow() const { return _low; }
    uint8_t percent(uint8_t chem) const { return percentFor(_volts, chem); }

    /** Piecewise-linear state of charge for a 2S pack. chem: 0 = LiPo, 1 = Li-ion. */
    static uint8_t percentFor(float volts, uint8_t chem);

private:
    float    _ratio;
    float    _tauS;
    uint32_t _lowHoldMs;
    bool     _has = false;
    float    _volts = 0.0f;
    bool     _below = false;
    uint32_t _belowSince = 0;
    bool     _low = false;
};
