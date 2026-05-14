#pragma once

class SpeedProfile {
public:
    SpeedProfile(int minSpeed, int baseSpeed, int maxSpeed, float errorThreshold);

    int compute(float normalizedError) const;
    void setParams(int minSpeed, int baseSpeed, int maxSpeed, float errorThreshold);

private:
    int _minSpeed;
    int _baseSpeed;
    int _maxSpeed;
    float _threshold;
};
