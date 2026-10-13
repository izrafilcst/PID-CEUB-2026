// src/storage/NvsStore.h
#pragma once
#include "storage/IPersistentStore.h"

#ifndef NATIVE_BUILD
#include <Preferences.h>
#endif

/**
 * Persistência baseada em ESP32 NVS via Preferences (key-value, namespace).
 * Em native build é um no-op vazio (usar InMemoryStore para testes).
 */
class NvsStore : public IPersistentStore {
public:
    bool  begin(const char* nspc) override;
    void  end() override;
    void  putFloat(const char* key, float value) override;
    float getFloat(const char* key, float defaultValue) override;
    bool  exists(const char* key) const override;
    void  clear() override;
    bool   putBlob(const char* key, const void* data, size_t len) override;
    size_t getBlob(const char* key, void* out, size_t maxLen) override;

private:
#ifndef NATIVE_BUILD
    Preferences _prefs;
#endif
    bool _open = false;
};
