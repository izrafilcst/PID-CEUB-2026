// src/storage/InMemoryStore.h
#pragma once
#include "storage/IPersistentStore.h"
#include <cstring>
#include <string>
#include <map>
#include <vector>

class InMemoryStore : public IPersistentStore {
public:
    bool begin(const char* /*nspc*/) override { return true; }
    void end() override {}

    void putFloat(const char* key, float value) override {
        _data[std::string(key)] = value;
    }

    float getFloat(const char* key, float defaultValue) override {
        auto it = _data.find(std::string(key));
        return (it == _data.end()) ? defaultValue : it->second;
    }

    bool putBlob(const char* key, const void* data, size_t len) override {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        _blobs[std::string(key)] = std::vector<uint8_t>(p, p + len);
        return true;
    }

    size_t getBlob(const char* key, void* out, size_t maxLen) override {
        auto it = _blobs.find(std::string(key));
        if (it == _blobs.end()) return 0;
        if (it->second.size() <= maxLen) std::memcpy(out, it->second.data(), it->second.size());
        return it->second.size();
    }

    bool exists(const char* key) const override {
        const std::string k(key);
        return _data.find(k) != _data.end() || _blobs.find(k) != _blobs.end();
    }

    void clear() override {
        _data.clear();
        _blobs.clear();
    }

private:
    std::map<std::string, float> _data;
    std::map<std::string, std::vector<uint8_t>> _blobs;
};
