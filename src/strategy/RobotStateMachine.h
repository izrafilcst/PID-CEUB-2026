#pragma once
#include <cstdint>

enum class RobotState : uint8_t { IDLE, CALIBRATING, READY, ARMED, RUNNING, ERROR };
enum class ErrorCode : uint8_t { NONE = 0, OVR = 1, LINE = 2, BAT = 3, CAL = 4 };

enum class SmEvent : uint8_t {
    BTN_SHORT, BTN_LONG, CMD_START, CMD_STOP, CMD_RESET, CMD_SENSOR_CAL,
    CAL_DONE_OK, CAL_DONE_FAIL, LINE_LOST_TIMEOUT, BATTERY_LOW, LOOP_OVERRUN
};

struct SmInputs {
    bool calibrationValid = false;
    bool batteryOk = true;
};

/** ARMED or RUNNING, for code that only sees the published state byte. */
inline bool isActiveState(uint8_t s) {
    return s == static_cast<uint8_t>(RobotState::ARMED) ||
           s == static_cast<uint8_t>(RobotState::RUNNING);
}

/** Máquina de estados do robô (spec §4.6). Pura: o chamador fornece tempo e entradas. */
class RobotStateMachine {
public:
    static constexpr uint32_t CAL_DURATION_MS = 5000;

    explicit RobotStateMachine(bool calibrationValidAtBoot, uint32_t nowMs = 0);

    RobotState state() const { return _state; }
    ErrorCode  error() const { return _error; }
    uint32_t   enteredAtMs() const { return _enteredAt; }

    /** Applies one event. Returns true when the state changed. */
    bool handle(SmEvent ev, const SmInputs& in, uint32_t nowMs);
    /** ARMED → RUNNING once `startMs` elapsed. Returns true on that transition. */
    bool tick(uint32_t nowMs, uint16_t startMs);
    bool calibrationWindowOver(uint32_t nowMs) const;

    bool motorsEnabled() const { return _state == RobotState::RUNNING; }
    bool isActive() const { return isActiveState(static_cast<uint8_t>(_state)); }

private:
    bool _enter(RobotState s, ErrorCode e, uint32_t nowMs);

    RobotState _state;
    ErrorCode  _error = ErrorCode::NONE;
    uint32_t   _enteredAt;
};
