#pragma once
#include <algorithm>

/**
 * Mistura base ± direção em [-1, +1] preservando a diferença entre os lados.
 * Se um lado passar de ±1, os dois são deslocados juntos: a curva tem prioridade sobre a
 * velocidade. Só quando |u| > 1 a diferença deixa de caber e os lados saturam.
 */
inline void mixSteering(float base, float u, float& left, float& right) {
    left  = base + u;
    right = base - u;
    const float hi = std::max(left, right);
    if (hi > 1.0f) {
        left  -= hi - 1.0f;
        right -= hi - 1.0f;
    }
    const float lo = std::min(left, right);
    if (lo < -1.0f) {
        left  += -1.0f - lo;
        right += -1.0f - lo;
    }
    left  = std::max(-1.0f, std::min(1.0f, left));
    right = std::max(-1.0f, std::min(1.0f, right));
}
