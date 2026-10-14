#pragma once

/**
 * Velocidade-base adaptativa, em fração de escala [0, 1] (spec §4.3).
 *   |erro| = 0     → vmax (reta)
 *   |erro| >= thr  → vmin (curva)
 *   entre os dois  → interpolação linear
 * step() limita a subida a `acc` fração/s; a descida é imediata.
 */
class SpeedProfile {
public:
    SpeedProfile() = default;

    void setParams(float vmin, float vmax, float thr, float acc);
    float target(float absError) const;
    float step(float absError, float dtS);
    void reset(float initial = 0.0f) { _current = initial; }
    float current() const { return _current; }

private:
    float _vmin = 0.0f;
    float _vmax = 0.0f;
    float _thr = 1.0f;
    float _acc = 1.0f;
    float _current = 0.0f;
};
