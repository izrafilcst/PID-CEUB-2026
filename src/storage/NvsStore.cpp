// src/storage/NvsStore.cpp
#include "storage/NvsStore.h"

#ifndef NATIVE_BUILD

bool NvsStore::begin(const char* nspc) {
    _open = _prefs.begin(nspc, /*readOnly=*/false);
    return _open;
}

void NvsStore::end() {
    if (_open) {
        _prefs.end();
        _open = false;
    }
}

void NvsStore::putFloat(const char* key, float value) {
    if (_open) _prefs.putFloat(key, value);
}

float NvsStore::getFloat(const char* key, float defaultValue) {
    if (!_open) return defaultValue;
    return _prefs.getFloat(key, defaultValue);
}

bool NvsStore::exists(const char* key) const {
    if (!_open) return false;
    // Preferences::isKey is not const-qualified in older arduino-esp32 versions,
    // so const_cast is necessary here. The cast is safe — no mutable state is
    // modified; isKey() only reads the NVS partition.
    return const_cast<Preferences&>(_prefs).isKey(key);
}

void NvsStore::clear() {
    if (_open) _prefs.clear();
}

bool NvsStore::putBlob(const char* key, const void* data, size_t len) {
    if (!_open) return false;
    return _prefs.putBytes(key, data, len) == len;
}

size_t NvsStore::getBlob(const char* key, void* out, size_t maxLen) {
    if (!_open || !_prefs.isKey(key)) return 0;
    const size_t len = _prefs.getBytesLength(key);
    if (len > 0 && len <= maxLen) _prefs.getBytes(key, out, len);
    return len;
}

#else  // NATIVE_BUILD — stubs (use InMemoryStore nos testes)

bool  NvsStore::begin(const char*) { _open = false; return false; }
void  NvsStore::end() {}
void  NvsStore::putFloat(const char*, float) {}
float NvsStore::getFloat(const char*, float def) { return def; }
bool  NvsStore::exists(const char*) const { return false; }
void  NvsStore::clear() {}
bool   NvsStore::putBlob(const char*, const void*, size_t) { return false; }
size_t NvsStore::getBlob(const char*, void*, size_t) { return 0; }

#endif
