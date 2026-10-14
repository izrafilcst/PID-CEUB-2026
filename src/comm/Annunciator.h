#pragma once
#include <cstdint>
#include "strategy/RobotStateMachine.h"

struct Indication {
    bool led;
    bool buzzer;
};

/**
 * Padrões de LED e buzzer por estado (spec §4.6), calculados a partir do tempo — nada
 * bloqueia (F-11). onState() deve ser chamado a cada mudança de estado.
 */
class Annunciator {
public:
    void onState(RobotState state, ErrorCode error, uint32_t nowMs);
    void beep(uint8_t count, uint16_t onMs, uint16_t offMs, uint32_t nowMs);
    Indication update(uint32_t nowMs) const;

private:
    bool _led(uint32_t nowMs) const;
    bool _sequenceOn(uint32_t nowMs) const;

    RobotState _state = RobotState::IDLE;
    ErrorCode  _error = ErrorCode::NONE;
    uint32_t   _since = 0;
    uint8_t    _beepCount = 0;
    uint16_t   _beepOn = 0;
    uint16_t   _beepOff = 0;
    uint32_t   _beepStart = 0;
};
