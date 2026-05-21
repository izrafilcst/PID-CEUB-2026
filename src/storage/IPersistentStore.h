// src/storage/IPersistentStore.h
#pragma once
#include <cstdint>

/**
 * Abstração sobre persistência chave-valor.
 *
 * Implementações:
 *  - NvsStore  (src/storage/NvsStore.h, ESP32 via Preferences)
 *  - InMemoryStore (src/storage/InMemoryStore.h, native para testes)
 */
class IPersistentStore {
public:
    virtual ~IPersistentStore() = default;
    virtual bool  begin(const char* nspc) = 0;
    virtual void  end() = 0;
    virtual void  putFloat(const char* key, float value) = 0;
    virtual float getFloat(const char* key, float defaultValue) = 0;
    virtual bool  exists(const char* key) const = 0;
    virtual void  clear() = 0;
};
