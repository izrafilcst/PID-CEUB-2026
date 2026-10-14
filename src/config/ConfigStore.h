#pragma once
#include <cstddef>
#include <cstdint>
#include "config/RobotConfig.h"
#include "sensors/Calibration.h"
#include "storage/IPersistentStore.h"

/**
 * Persistência da configuração (spec §4.4). Cada bloco é um blob com magic, versão,
 * tamanho e CRC32. Qualquer falha de leitura devolve os valores de fábrica daquele bloco.
 * Chaves: "hw", "prof0".."prof2", "prof_act", "cal"; PPR em "left_ppr"/"right_ppr"
 * (as mesmas do VelocityEstimator, que continuam sendo a fonte da verdade do PPR).
 */
class ConfigStore {
public:
    struct LoadReport {
        bool hardwareOk;
        bool profileOk;
    };

    explicit ConfigStore(IPersistentStore& store);

    LoadReport load(RobotConfig& cfg);
    bool loadHardware(HardwareConfig& hw);
    /** False when absent/invalid; `out` then gets factoryProfile(index). */
    bool loadProfile(uint8_t index, TuningProfile& out);
    uint8_t loadActiveIndex();
    /** Writes hw + the active profile + the active index. Refuses invalid configs. */
    bool saveActive(const RobotConfig& cfg);
    bool saveCalibration(const CalibrationData& data);
    bool loadCalibration(CalibrationData& data);

    static uint32_t crc32(const uint8_t* data, size_t len);

private:
    bool _write(const char* key, uint16_t version, const void* payload, uint16_t size);
    bool _read(const char* key, uint16_t version, void* payload, uint16_t size);

    IPersistentStore& _store;
};
