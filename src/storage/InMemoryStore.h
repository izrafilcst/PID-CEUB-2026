// src/storage/InMemoryStore.h
#pragma once
#include "storage/IPersistentStore.h"
#include <cstring>
#include <string>
#include <map>

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

    bool exists(const char* key) const override {
        return _data.find(std::string(key)) != _data.end();
    }

    void clear() override { _data.clear(); }

private:
    std::map<std::string, float> _data;
};
