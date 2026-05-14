#include "motors/DifferentialDrive.h"
#include <algorithm>

DifferentialDrive::DifferentialDrive(int maxPwm)
    : _maxPwm(maxPwm) {}

void DifferentialDrive::compute(float correction, int baseSpeed, int& leftPwm, int& rightPwm) const {
    int corr = static_cast<int>(correction);
    leftPwm  = _clamp(baseSpeed + corr, -_maxPwm, _maxPwm);
    rightPwm = _clamp(baseSpeed - corr, -_maxPwm, _maxPwm);
}

void DifferentialDrive::setMaxPwm(int maxPwm) {
    _maxPwm = maxPwm;
}

int DifferentialDrive::_clamp(int value, int min, int max) const {
    return std::max(min, std::min(max, value));
}
