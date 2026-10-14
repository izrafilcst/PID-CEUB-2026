#pragma once
#include <cstddef>
#include <cstdint>

/**
 * Configuração em runtime (spec §4.3). Os inicializadores dos membros SÃO os valores de
 * fábrica. HardwareConfig é global; TuningProfile existe em PROFILE_COUNT perfis.
 */
struct HardwareConfig {
    uint8_t invL  = 0;       ///< inverte o motor esquerdo
    uint8_t invR  = 0;       ///< inverte o motor direito
    uint8_t swap  = 0;       ///< troca os canais A/B do TB6612
    uint8_t einvL = 0;       ///< inverte a contagem do encoder esquerdo
    uint8_t einvR = 0;       ///< inverte a contagem do encoder direito
    uint8_t pol   = 1;       ///< 0 = linha preta, 1 = linha branca
    uint8_t sinv  = 0;       ///< barra de sensores montada espelhada
    uint8_t chem  = 1;       ///< 0 = LiPo, 1 = Li-ion
    uint8_t lock  = 0;       ///< modo competição (regra RoboCore 5.9)
    float   vcut  = 6.4f;    ///< V — corte da bateria
    float   vbk   = 1.0f;    ///< correção da leitura de tensão
    float   pprL  = 600.0f;  ///< contagens ×4 por volta do eixo de saída
    float   pprR  = 600.0f;
};

struct TuningProfile {
    uint8_t  mode    = 0;       ///< 0 = PWM direto, 1 = cascata
    float    kp      = 1.0f;
    float    ki      = 0.0f;
    float    kd      = 0.03f;
    float    dlp     = 0.5f;
    float    kpv     = 0.002f;
    float    kiv     = 0.005f;
    float    kdv     = 0.0f;
    float    vmax    = 0.35f;
    float    vmin    = 0.25f;
    float    thr     = 0.6f;
    float    acc     = 2.0f;
    float    rpmMax  = 700.0f;
    float    vref    = 7.4f;
    uint8_t  vcomp   = 1;
    uint16_t lostMs  = 300;
    uint16_t startMs = 1000;
};

constexpr uint8_t PROFILE_COUNT = 3;

struct RobotConfig {
    HardwareConfig hw;
    TuningProfile  tune;
    uint8_t        profile = 0;
};

enum class SetResult : uint8_t { OK, UNKNOWN_KEY, OUT_OF_RANGE, LOCKED_WHILE_RUNNING };

namespace robotcfg {

enum class FieldType : uint8_t { U8, U16, F32 };

struct KeyInfo {
    const char* key;
    const char* group;      ///< "hw" | "pid" | "vel" | "spd" | "misc"
    FieldType   type;
    float       min;
    float       max;
    bool        hardware;   ///< true → HardwareConfig; false → TuningProfile
    bool        liveInRun;  ///< false → refused while ARMED/RUNNING
    size_t      offset;     ///< offsetof inside its struct
};

constexpr const char* GROUPS[] = {"hw", "pid", "vel", "spd", "misc"};
constexpr size_t GROUP_COUNT = 5;

HardwareConfig factoryHardware();
/** 0 "seguro" (vmax 0.35), 1 "treino" (0.50), 2 "corrida" (0.70). Others → profile 0. */
TuningProfile factoryProfile(uint8_t index);
RobotConfig factoryConfig();

const KeyInfo* keyTable(size_t& count);
const KeyInfo* findKey(const char* key);

/** Validates `value` for `key` and applies it. `running` = state is ARMED or RUNNING. */
SetResult set(RobotConfig& cfg, const char* key, float value, bool running);
/** Reads one key as float. False for unknown keys. */
bool get(const RobotConfig& cfg, const char* key, float& value);
/** True when every key is inside its range, vmin ≤ vmax and profile < PROFILE_COUNT. */
bool validate(const RobotConfig& cfg);

}  // namespace robotcfg
