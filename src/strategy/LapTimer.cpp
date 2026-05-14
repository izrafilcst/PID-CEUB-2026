#include "strategy/LapTimer.h"
#include <algorithm>

LapTimer::LapTimer()
    : _running(false), _lapStartMs(0), _lastLapMs(0), _bestLapMs(0), _lapCount(0) {}

void LapTimer::start(uint32_t nowMs) {
    _running    = true;
    _lapStartMs = nowMs;
}

void LapTimer::stop() {
    _running = false;
}

void LapTimer::notifyLineCrossing(uint32_t nowMs) {
    if (!_running) return;

    uint32_t lapTime = nowMs - _lapStartMs;
    _lastLapMs  = lapTime;
    _lapCount++;

    if (_bestLapMs == 0 || lapTime < _bestLapMs) {
        _bestLapMs = lapTime;
    }

    _lapStartMs = nowMs;  // reset timer for next lap
}

uint32_t LapTimer::getLastLapMs() const { return _lastLapMs; }
uint32_t LapTimer::getBestLapMs() const { return _bestLapMs; }
int      LapTimer::getLapCount()  const { return _lapCount;  }
bool     LapTimer::isRunning()    const { return _running;   }

void LapTimer::reset() {
    _running    = false;
    _lapStartMs = 0;
    _lastLapMs  = 0;
    _bestLapMs  = 0;
    _lapCount   = 0;
}
