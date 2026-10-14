#pragma once
#include "sensors/Calibration.h"
#include "control/PIDController.h"

/** Resultado de um ciclo da malha de posição. */
struct LineReading {
    float error;  ///< posição normalizada em [-1, +1]; > 0 = linha à direita
    float u;      ///< saída do PID externo em [-1, +1]; > 0 = virar à direita
    bool  lost;   ///< nenhum sensor vê a linha
};

/**
 * Malha de posição: leitura bruta → erro normalizado → PID externo.
 *
 * Não altera a calibração em nenhuma hipótese (F-01). Com a linha perdida, o erro satura
 * para o lado onde ela foi vista por último (F-14).
 */
class LineFollower {
public:
    LineFollower(Calibration& cal, PIDController& pid);

    LineReading update(const int* rawSensors, float dtS);

    /** Error for `rawSensors` without running the PID (used to align resets). */
    float peekError(const int* rawSensors);

    /** Clears the PID aligned to `currentError` and seeds the lost-line memory. */
    void reset(float currentError = 0.0f);

    void setSensorOrderInverted(bool inverted) { _sensorInvert = inverted; }

private:
    float _errorFrom(const int* rawSensors, bool& lost);

    Calibration&   _cal;
    PIDController& _pid;
    float _positionMax;
    bool  _sensorInvert = false;
    float _lastValidError = 0.0f;
    int   _normalized[Calibration::MAX_SENSORS];
};
