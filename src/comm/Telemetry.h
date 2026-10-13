#pragma once
#include <cstdint>

/** Frame rápido, 30 Hz (spec §4.5). */
struct TelemetryFrame {
    float    pos  = 0.0f;  ///< erro normalizado [-1, +1]
    float    corr = 0.0f;  ///< saída do PID externo [-1, +1]
    int16_t  vL   = 0;     ///< duty do lado esquerdo (antes de inversão/troca de canal)
    int16_t  vR   = 0;
    uint16_t dtUs = 0;     ///< tempo de execução do ciclo
    int16_t  rpmL = 0;
    int16_t  rpmR = 0;
    int16_t  spL  = 0;
    int16_t  spR  = 0;
    uint8_t  s[8] = {};    ///< sensores normalizados, %
    uint8_t  st   = 0;     ///< RobotState
    uint8_t  lost = 0;
};

/** Frame lento, 2 Hz. */
struct StatusFrame {
    uint8_t  bat     = 0;  ///< %
    float    vb      = 0.0f;
    float    lap     = 0.0f;
    float    laps[5] = {};
    uint8_t  nLaps   = 0;
    uint8_t  err     = 0;  ///< ErrorCode
    uint8_t  prof    = 0;
    uint8_t  dirty   = 0;
    uint16_t ovr     = 0;  ///< maior tempo de execução na janela, µs
};

/** Resultado de uma calibração, do core 1 para o core 0. */
struct CalReport {
    enum class Kind : uint8_t { SENSOR, ENCODER };
    Kind    kind   = Kind::SENSOR;
    bool    ok     = false;
    uint8_t bad[8] = {};
    uint8_t nBad   = 0;
    bool    left   = true;
    float   ppr    = 0.0f;
};
