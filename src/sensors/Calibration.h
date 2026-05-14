#pragma once
#include <climits>

class Calibration {
public:
    explicit Calibration(int sensorCount);
    ~Calibration();

    void update(const int* rawValues);
    void normalize(const int* rawValues, int* normalizedOut) const;
    float weightedPosition(const int* normalized) const;
    void reset();

    int getMin(int idx) const;
    int getMax(int idx) const;
    bool isLineLost() const { return _lineLost; }

    static constexpr float POSITION_SCALE = 1000.0f;

private:
    int _count;
    int* _min;
    int* _max;
    mutable float _lastPosition;
    mutable bool _lineLost;

    static constexpr int NORM_MAX = 1000;
    static constexpr int LINE_LOST_THRESHOLD = 200;
};
