#pragma once
#include <cstdint>
#include "comm/SpinLock.h"

/**
 * Último valor publicado, com número de versão. Um núcleo publica (post), o outro busca
 * (fetch) só quando muda. A seção crítica é só a cópia do valor.
 */
template <typename T>
class Mailbox {
public:
    void post(const T& value) {
        SpinGuard guard(_lock);
        _value = value;
        _version++;
    }

    /** Copies the value when it changed since `seen`; updates `seen`. */
    bool fetch(T& out, uint32_t& seen) {
        SpinGuard guard(_lock);
        if (_version == seen) return false;
        out = _value;
        seen = _version;
        return true;
    }

private:
    T        _value{};
    uint32_t _version = 0;
    SpinLock _lock;
};
