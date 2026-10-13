#include "comm/CommandQueue.h"
#include <cstring>

bool CommandQueue::push(const char* msg, size_t len) {
    if (!msg || len == 0 || len >= MAX_LEN) return false;
    SpinGuard guard(_lock);
    if (_count == CAPACITY) return false;
    const size_t idx = (_head + _count) % CAPACITY;
    std::memcpy(_items[idx], msg, len);
    _items[idx][len] = '\0';
    _count++;
    return true;
}

bool CommandQueue::pop(char* out, size_t outLen) {
    if (!out || outLen == 0) return false;
    SpinGuard guard(_lock);
    if (_count == 0) return false;
    std::strncpy(out, _items[_head], outLen - 1);
    out[outLen - 1] = '\0';
    _head = (_head + 1) % CAPACITY;
    _count--;
    return true;
}

size_t CommandQueue::size() const {
    SpinGuard guard(_lock);
    return _count;
}
