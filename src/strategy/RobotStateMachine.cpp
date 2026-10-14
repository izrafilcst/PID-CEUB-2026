#include "strategy/RobotStateMachine.h"

RobotStateMachine::RobotStateMachine(bool calibrationValidAtBoot, uint32_t nowMs,
                                     NvsArbiter* nvs)
    : _state(calibrationValidAtBoot ? RobotState::READY : RobotState::IDLE),
      _enteredAt(nowMs), _nvs(nvs) {}

bool RobotStateMachine::_enter(RobotState s, ErrorCode e, uint32_t nowMs) {
    if (s == _state && e == _error) return false;
    const bool wasActive = isActive();
    const bool willBeActive = isActiveState(static_cast<uint8_t>(s));
    // NVS em uso pela comm: não arma (fica no estado atual) — spec §4.4, A-WP1-05
    if (!wasActive && willBeActive && _nvs && !_nvs->tryArm()) return false;
    if (wasActive && !willBeActive && _nvs) _nvs->disarm();
    _state = s;
    _error = e;
    _enteredAt = nowMs;
    return true;
}

bool RobotStateMachine::handle(SmEvent ev, const SmInputs& in, uint32_t nowMs) {
    using S = RobotState;
    using E = SmEvent;
    if (ev == E::CMD_RESET) return _enter(S::IDLE, ErrorCode::NONE, nowMs);

    // Botão (E-3): o aperto para o carro e consome o gesto
    if (ev == E::BTN_PRESS) {
        _gestureConsumed = false;
        if (!isActive()) return false;
        _gestureConsumed = true;
        return _enter(S::READY, ErrorCode::NONE, nowMs);
    }
    if ((ev == E::BTN_SHORT || ev == E::BTN_LONG) && _gestureConsumed) return false;

    switch (_state) {
        case S::IDLE:
            if (ev == E::BTN_LONG || ev == E::CMD_SENSOR_CAL) {
                return _enter(S::CALIBRATING, ErrorCode::NONE, nowMs);
            }
            if (ev == E::BTN_SHORT && in.calibrationValid) {
                return _enter(S::READY, ErrorCode::NONE, nowMs);
            }
            break;
        case S::CALIBRATING:
            if (ev == E::CAL_DONE_OK) return _enter(S::READY, ErrorCode::NONE, nowMs);
            if (ev == E::CAL_DONE_FAIL) return _enter(S::ERROR, ErrorCode::CAL, nowMs);
            break;
        case S::READY:
            if (ev == E::BTN_SHORT || ev == E::CMD_START) {
                return in.batteryOk ? _enter(S::ARMED, ErrorCode::NONE, nowMs)
                                    : _enter(S::ERROR, ErrorCode::BAT, nowMs);
            }
            if (ev == E::BTN_LONG || ev == E::CMD_SENSOR_CAL) {
                return _enter(S::CALIBRATING, ErrorCode::NONE, nowMs);
            }
            break;
        case S::ARMED:
            if (ev == E::CMD_STOP) return _enter(S::READY, ErrorCode::NONE, nowMs);
            break;
        case S::RUNNING:
            if (ev == E::CMD_STOP) return _enter(S::READY, ErrorCode::NONE, nowMs);
            if (ev == E::LINE_LOST_TIMEOUT) return _enter(S::ERROR, ErrorCode::LINE, nowMs);
            if (ev == E::BATTERY_LOW) return _enter(S::ERROR, ErrorCode::BAT, nowMs);
            if (ev == E::LOOP_OVERRUN) return _enter(S::ERROR, ErrorCode::OVR, nowMs);
            break;
        case S::ERROR:
            if (ev == E::BTN_SHORT) {
                return _enter(in.calibrationValid ? S::READY : S::IDLE, ErrorCode::NONE, nowMs);
            }
            break;
    }
    return false;
}

bool RobotStateMachine::tick(uint32_t nowMs, uint16_t startMs) {
    if (_state == RobotState::ARMED && nowMs - _enteredAt >= startMs) {
        return _enter(RobotState::RUNNING, ErrorCode::NONE, nowMs);
    }
    return false;
}

bool RobotStateMachine::calibrationWindowOver(uint32_t nowMs) const {
    return _state == RobotState::CALIBRATING && nowMs - _enteredAt >= CAL_DURATION_MS;
}
