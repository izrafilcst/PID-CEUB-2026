#pragma once
#include <cstddef>
#include "comm/SpinLock.h"

/** Fila fixa de comandos JSON (produtor: callback NimBLE; consumidor: task comm). */
class CommandQueue {
public:
    static constexpr size_t CAPACITY = 4;
    static constexpr size_t MAX_LEN = 192;

    /** False when full, empty or too long (the message is dropped). */
    bool push(const char* msg, size_t len);
    bool pop(char* out, size_t outLen);
    size_t size() const;

private:
    char   _items[CAPACITY][MAX_LEN] = {};
    size_t _head  = 0;
    size_t _count = 0;
    mutable SpinLock _lock;
};
