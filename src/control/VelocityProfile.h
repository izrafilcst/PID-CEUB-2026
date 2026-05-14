#pragma once
#include <cstdint>

class VelocityProfile {
public:
    static constexpr int BUFFER_SIZE = 8;

    VelocityProfile(int minSpeed, int baseSpeed, int maxSpeed,
                    float errorThreshold, float curvatureWeight = 0.4f);

    void update(float position, uint32_t timestampMs);
    int  computeSpeed() const;
    void setParams(int minSpeed, int baseSpeed, int maxSpeed, float errorThreshold);
    float getCurvatureScore() const;

private:
    struct Sample { float pos; uint32_t ts; };

    Sample   _buf[BUFFER_SIZE];
    int      _head;
    int      _count;

    int   _minSpeed;
    int   _baseSpeed;
    int   _maxSpeed;
    float _threshold;
    float _curvatureWeight;

    static constexpr float POSITION_MAX    = 3500.0f;
    static constexpr float CURVATURE_NORM  = 35.0f;   // units/ms at max curvature
};
