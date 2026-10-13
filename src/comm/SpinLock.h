#pragma once

#ifndef NATIVE_BUILD
#include <freertos/FreeRTOS.h>

/** Spinlock entre núcleos do ESP32 (portMUX). Seções críticas curtas: só cópias. */
class SpinLock {
public:
    void lock() { portENTER_CRITICAL(&_mux); }
    void unlock() { portEXIT_CRITICAL(&_mux); }

private:
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
};
#else
/** Native: os testes rodam numa thread só. */
class SpinLock {
public:
    void lock() {}
    void unlock() {}
};
#endif

class SpinGuard {
public:
    explicit SpinGuard(SpinLock& lock) : _lock(lock) { _lock.lock(); }
    ~SpinGuard() { _lock.unlock(); }
    SpinGuard(const SpinGuard&) = delete;
    SpinGuard& operator=(const SpinGuard&) = delete;

private:
    SpinLock& _lock;
};
