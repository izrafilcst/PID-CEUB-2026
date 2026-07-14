#pragma once
#include <cassert>
#include <climits>

class Calibration {
public:
    static constexpr int MAX_SENSORS = 8;
    static constexpr float POSITION_SCALE = 1000.0f;

    explicit Calibration(int sensorCount);

    void update(const int* rawValues);
    void normalize(const int* rawValues, int* normalizedOut) const;
    float weightedPosition(const int* normalized) const;
    void reset();

    int getMin(int idx) const;
    int getMax(int idx) const;
    bool isLineLost() const { return _lineLost; }
    bool isCrossing() const { return _crossing; }

private:
    Calibration(const Calibration&) = delete;
    Calibration& operator=(const Calibration&) = delete;
    Calibration(Calibration&&) = delete;
    Calibration& operator=(Calibration&&) = delete;

    int _count;
    int _min[MAX_SENSORS];
    int _max[MAX_SENSORS];
    mutable float _lastPosition;
    mutable bool _lineLost;
    mutable bool _crossing;

    static constexpr int NORM_MAX = 1000;
    static constexpr int LINE_LOST_THRESHOLD = 200;

    // Cruzamento: linha perpendicular acende muitos sensores ao mesmo tempo.
    static constexpr int CROSSING_ACTIVE_LEVEL = 700;  // normalizado [0..1000]
    static constexpr int CROSSING_MIN_ACTIVE   = 6;    // nº mínimo de sensores
};
