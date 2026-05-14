#pragma once
#include <cstdint>

class LapTimer {
public:
    LapTimer();

    void start(uint32_t nowMs);
    void stop();
    void notifyLineCrossing(uint32_t nowMs);

    uint32_t getLastLapMs() const;
    uint32_t getBestLapMs() const;
    int      getLapCount()  const;
    bool     isRunning()    const;

    void reset();

private:
    bool     _running;
    uint32_t _lapStartMs;
    uint32_t _lastLapMs;
    uint32_t _bestLapMs;
    int      _lapCount;
};
