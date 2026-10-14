#include "comm/Annunciator.h"

void Annunciator::onState(RobotState state, ErrorCode error, uint32_t nowMs) {
    _state = state;
    _error = error;
    _since = nowMs;
    switch (state) {
        case RobotState::CALIBRATING:
        case RobotState::READY:
            beep(1, 80, 0, nowMs);
            break;
        case RobotState::ERROR:
            beep(static_cast<uint8_t>(error), 120, 120, nowMs);
            break;
        default:
            break;
    }
}

void Annunciator::beep(uint8_t count, uint16_t onMs, uint16_t offMs, uint32_t nowMs) {
    _beepCount = count;
    _beepOn = onMs;
    _beepOff = offMs;
    _beepStart = nowMs;
}

bool Annunciator::_sequenceOn(uint32_t nowMs) const {
    if (_beepCount == 0 || _beepOn == 0) return false;
    const uint32_t t = nowMs - _beepStart;
    const uint32_t period = static_cast<uint32_t>(_beepOn) + _beepOff;
    if (t >= period * _beepCount) return false;
    return (t % period) < _beepOn;
}

bool Annunciator::_led(uint32_t nowMs) const {
    const uint32_t t = nowMs - _since;
    switch (_state) {
        case RobotState::IDLE:        return (t / 500) % 2 == 0;
        case RobotState::CALIBRATING: return (t / 100) % 2 == 0;
        case RobotState::READY:       return true;
        case RobotState::ARMED:       return (t / 50) % 2 == 0;
        case RobotState::RUNNING:     return false;
        case RobotState::ERROR: {
            const uint32_t n = static_cast<uint32_t>(_error);
            const uint32_t phase = t % (n * 400 + 1200);
            return phase < n * 400 && (phase % 400) < 200;
        }
    }
    return false;
}

Indication Annunciator::update(uint32_t nowMs) const {
    const bool armedTick = _state == RobotState::ARMED && ((nowMs - _since) % 250) < 40;
    return Indication{_led(nowMs), _sequenceOn(nowMs) || armedTick};
}
