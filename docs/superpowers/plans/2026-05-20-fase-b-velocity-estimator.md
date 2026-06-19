# Fase B — VelocityEstimator + Auto-Calibração NVS Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Converter contagem de pulsos do Encoder em RPM com filtragem low-pass IIR, persistir o PPR efetivo (medido por auto-calibração manual) em NVS para sobreviver a reboots, e expor procedimento de calibração (`startCalibration() / finishCalibration(rotations)`).

**Architecture:** `VelocityEstimator` recebe um `Encoder&` (DI) + `IPersistentStore&` (abstração sobre NVS para testabilidade). A cada chamada `update(dtMs)`, lê `getDelta()` do encoder, calcula RPM cru, aplica filtro IIR e expõe `getRPM()`. Auto-calibração: zerar contagem → usuário gira N voltas com a mão → `finishCalibration(N)` divide count por N para obter PPR efetivo (4× já incluso porque encoder usa quadratura 4×) e salva no store. No native build, `InMemoryStore` substitui NVS; nos testes injeta-se diretamente.

**Tech Stack:** C++17, ESP32 `Preferences` library (NVS wrapper), Unity Test Framework.

---

## File Map

| File | Action | Purpose |
|------|--------|---------|
| `src/storage/IPersistentStore.h` | Create | Interface abstrata para persistência (testável) |
| `src/storage/NvsStore.h` | Create | Implementação ESP32 usando Preferences |
| `src/storage/NvsStore.cpp` | Create | begin/get/put/end com Preferences |
| `src/storage/InMemoryStore.h` | Create | Implementação native para testes |
| `src/sensors/VelocityEstimator.h` | Create | Interface (RPM + calibração) |
| `src/sensors/VelocityEstimator.cpp` | Create | Implementação compartilhada (sem ESP32-isms) |
| `test/test_velocity_estimator/test_velocity_estimator.cpp` | Create | 10 testes Unity nativos |
| `src/config.h` | Modify | `ENCODER_DEFAULT_PPR_X4`, `VELOCITY_FILTER_ALPHA` |
| `platformio.ini` | No change | Preferences já vem com Arduino-ESP32 |

---

## Task B1: Criar interface IPersistentStore + InMemoryStore

**Files:**
- Create: `src/storage/IPersistentStore.h`
- Create: `src/storage/InMemoryStore.h`

- [ ] **Step 1: Criar diretório e IPersistentStore.h**

```cpp
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
```

- [ ] **Step 2: Criar InMemoryStore.h (header-only, simples)**

```cpp
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
```

- [ ] **Step 3: Verificar que compila no ambiente native**

```bash
pio run -e native 2>&1 | tail -3
```

Expected: nenhum erro (arquivos são header-only, ainda não usados).

- [ ] **Step 4: Commit**

```bash
git add src/storage/IPersistentStore.h src/storage/InMemoryStore.h
git commit -m "feat(storage): add persistent store interface and in-memory fake"
```

---

## Task B2: Criar arquivo de teste RED com 10 stubs

**Files:**
- Create: `test/test_velocity_estimator/test_velocity_estimator.cpp`

- [ ] **Step 1: Criar arquivo de teste com 10 testes em RED**

```cpp
// test/test_velocity_estimator/test_velocity_estimator.cpp
#include <unity.h>
#include "sensors/Encoder.h"
#include "sensors/VelocityEstimator.h"
#include "storage/InMemoryStore.h"

static Encoder*           enc   = nullptr;
static InMemoryStore*     store = nullptr;
static VelocityEstimator* ve    = nullptr;

void setUp() {
    enc = new Encoder(0, 1);
    enc->begin();
    store = new InMemoryStore();
    store->begin("vetest");
    ve = new VelocityEstimator(*enc, *store, "left");
    ve->begin();  // carrega PPR salvo ou default
}

void tearDown() {
    delete ve;    ve = nullptr;
    delete store; store = nullptr;
    delete enc;   enc = nullptr;
}

// Helper: simula uma volta completa do shaft de saída.
// Com PPR_x4 = 28 (default 7 PPR no motor × 4 quadratura),
// cada volta = 28 contagens. Dividimos em 7 períodos completos para frente.
static void simulateRotations(Encoder& e, int rotations, int countsPerRotation = 28) {
    int periods = (rotations * countsPerRotation) / 4;
    for (int i = 0; i < periods; i++) {
        e._simulatePulse(false, true);
        e._simulatePulse(true,  true);
        e._simulatePulse(true,  false);
        e._simulatePulse(false, false);
    }
}

// ─── 1. RPM zero quando não há pulsos ─────────────────────────────────────
void test_velocity_zero_when_no_pulses() {
    ve->update(10);  // 10ms passaram, zero pulsos
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ve->getRPM());
}

// ─── 2. Conversão pulsos→RPM correta com PPR padrão ──────────────────────
void test_velocity_converts_pulses_to_rpm() {
    // PPR_x4 default = 28. Em 100ms (0.1s), 28 pulsos = 1 rotação = 10 rps = 600 RPM
    ve->setFilterAlpha(1.0f);  // desabilita filtro para teste determinístico
    simulateRotations(*enc, 1);
    ve->update(100);  // 100 ms
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 600.0f, ve->getRPM());
}

// ─── 3. Filtro low-pass IIR suaviza spikes ────────────────────────────────
void test_velocity_low_pass_smooths_spikes() {
    ve->setFilterAlpha(0.3f);  // peso 30% no novo valor
    // Primeiro update: estabilize em 600 RPM
    for (int i = 0; i < 20; i++) {
        simulateRotations(*enc, 1);
        ve->update(100);
    }
    float steady = ve->getRPM();
    TEST_ASSERT_FLOAT_WITHIN(20.0f, 600.0f, steady);
    // Spike: 5× a velocidade em 1 update
    simulateRotations(*enc, 5);
    ve->update(100);
    float afterSpike = ve->getRPM();
    // Filtro deve atenuar — afterSpike < 3000 (RPM cru) e > steady
    TEST_ASSERT_TRUE(afterSpike < 3000.0f);
    TEST_ASSERT_TRUE(afterSpike > steady);
}

// ─── 4. dt=0 não causa divisão por zero ───────────────────────────────────
void test_velocity_handles_zero_dt_safely() {
    simulateRotations(*enc, 1);
    ve->update(0);  // dt=0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ve->getRPM());  // retorna 0 com segurança
}

// ─── 5. Direção reversa produz RPM negativo ───────────────────────────────
void test_velocity_signed_with_direction() {
    ve->setFilterAlpha(1.0f);
    // 7 períodos completos para TRÁS = -28 pulsos = -1 rotação
    for (int i = 0; i < 7; i++) {
        enc->_simulatePulse(true,  false);
        enc->_simulatePulse(true,  true);
        enc->_simulatePulse(false, true);
        enc->_simulatePulse(false, false);
    }
    ve->update(100);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, -600.0f, ve->getRPM());
}

// ─── 6. PPR default carregado quando não há valor salvo ───────────────────
void test_velocity_default_ppr_when_no_calibration() {
    // store está vazio (InMemoryStore criado novo no setUp)
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 28.0f, ve->getEffectivePPR());
}

// ─── 7. startCalibration zera contagem do encoder ─────────────────────────
void test_velocity_start_calibration_resets_count() {
    simulateRotations(*enc, 3);  // gira 3 voltas antes
    ve->startCalibration();
    TEST_ASSERT_EQUAL_INT32(0, enc->getCount());
}

// ─── 8. finishCalibration grava PPR efetivo no store ──────────────────────
void test_velocity_finish_calibration_stores_ppr() {
    ve->startCalibration();
    simulateRotations(*enc, 10, 40);  // 10 voltas a 40 counts/volta = 400 counts
    bool ok = ve->finishCalibration(10);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 40.0f, ve->getEffectivePPR());
    TEST_ASSERT_TRUE(store->exists("left_ppr"));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 40.0f, store->getFloat("left_ppr", 0.0f));
}

// ─── 9. Após calibração, RPM usa novo PPR ─────────────────────────────────
void test_velocity_uses_calibrated_ppr() {
    ve->startCalibration();
    simulateRotations(*enc, 10, 40);
    ve->finishCalibration(10);
    ve->setFilterAlpha(1.0f);
    // Após calibração: 40 PPR_x4 efetivo. 40 pulsos em 100ms = 1 volta = 600 RPM.
    simulateRotations(*enc, 1, 40);
    ve->update(100);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 600.0f, ve->getRPM());
}

// ─── 10. begin() carrega PPR salvo previamente em store ───────────────────
void test_velocity_begin_loads_persisted_ppr() {
    store->putFloat("left_ppr", 56.0f);  // simula reboot com PPR salvo
    VelocityEstimator ve2(*enc, *store, "left");
    ve2.begin();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 56.0f, ve2.getEffectivePPR());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_velocity_zero_when_no_pulses);
    RUN_TEST(test_velocity_converts_pulses_to_rpm);
    RUN_TEST(test_velocity_low_pass_smooths_spikes);
    RUN_TEST(test_velocity_handles_zero_dt_safely);
    RUN_TEST(test_velocity_signed_with_direction);
    RUN_TEST(test_velocity_default_ppr_when_no_calibration);
    RUN_TEST(test_velocity_start_calibration_resets_count);
    RUN_TEST(test_velocity_finish_calibration_stores_ppr);
    RUN_TEST(test_velocity_uses_calibrated_ppr);
    RUN_TEST(test_velocity_begin_loads_persisted_ppr);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar — deve falhar no link**

```bash
pio test -e native -f test_velocity_estimator 2>&1 | tail -10
```

Expected: erros de link reclamando que `sensors/VelocityEstimator.h` não existe.

- [ ] **Step 3: Commit (RED state)**

```bash
git add test/test_velocity_estimator/test_velocity_estimator.cpp
git commit -m "test(velocity): add 10 failing tests for RPM + auto-calibration"
```

---

## Task B3: Criar VelocityEstimator.h (interface)

**Files:**
- Create: `src/sensors/VelocityEstimator.h`

- [ ] **Step 1: Escrever interface**

```cpp
// src/sensors/VelocityEstimator.h
#pragma once
#include <cstdint>
#include "sensors/Encoder.h"
#include "storage/IPersistentStore.h"

/**
 * Converte contagem do Encoder em RPM filtrado.
 *
 * Auto-calibração: o usuário gira a roda N voltas manualmente; o estimador
 * conta os pulsos no shaft de saída (incluindo os 4× da quadratura) e divide
 * por N para descobrir o PPR efetivo. Resultado é gravado no IPersistentStore
 * sob a chave "<keyPrefix>_ppr" e sobrevive a reboots.
 *
 * Filtro: IIR low-pass de 1ª ordem (alpha em [0, 1]; menor = mais suave).
 *
 * Uso típico:
 *   VelocityEstimator velL(encL, nvs, "left");
 *   velL.begin();             // carrega PPR salvo (ou default ENCODER_DEFAULT_PPR_X4)
 *   ... loop ...
 *   velL.update(dtMs);        // chamar periodicamente (10–20 ms)
 *   float rpm = velL.getRPM();
 */
class VelocityEstimator {
public:
    VelocityEstimator(Encoder& enc, IPersistentStore& store, const char* keyPrefix);

    // Carrega PPR persistido (ou usa default). Chamar uma vez no setup.
    void begin();

    // Atualiza RPM dado intervalo desde última chamada (ms).
    void update(uint32_t dtMs);

    // RPM filtrado (com sinal — negativo = sentido reverso).
    float getRPM() const;

    // PPR efetivo em quadratura 4× (contagens por volta do shaft de saída).
    float getEffectivePPR() const;

    // Ajusta α do filtro IIR. α=1.0 desabilita filtro (uso em testes).
    void setFilterAlpha(float alpha);

    // ── Auto-calibração ─────────────────────────────────────────────────
    // 1. startCalibration()  — zera contagem.
    // 2. Usuário gira N voltas manualmente.
    // 3. finishCalibration(N) — calcula PPR = count/N e grava no store.
    void startCalibration();
    bool finishCalibration(int rotations);

private:
    Encoder&          _enc;
    IPersistentStore& _store;
    const char*       _keyPrefix;
    char              _key[24];  // "<prefix>_ppr"
    float             _pprX4;
    float             _alpha;
    float             _rpmFiltered;
};
```

- [ ] **Step 2: Adicionar constantes default em config.h**

Localizar a seção `// ═══ SENSORES ═══` em `src/config.h` (~linha 38) e adicionar **logo após** o bloco de SENSOR_*:

```cpp
// ═══ ENCODER / VELOCITY (Fase B) ═══════════════════════════════════════════
// PPR_X4 default = 4 × PPR motor. N20 com encoder Hall típico = 7 PPR → 28 X4.
// Auto-calibração via BLE sobrescreve esse valor e salva em NVS.
#define ENCODER_DEFAULT_PPR_X4   28.0f
#define VELOCITY_FILTER_ALPHA    0.3f  // 0 < α ≤ 1 — menor = mais suave, mais delay
```

- [ ] **Step 3: Verificar compilação**

```bash
pio run -e native 2>&1 | tail -3
```

Expected: ainda erros de link de testes, mas sem erro de compilação dos headers.

- [ ] **Step 4: Commit**

```bash
git add src/sensors/VelocityEstimator.h src/config.h
git commit -m "feat(velocity): add interface and config constants"
```

---

## Task B4: Implementar VelocityEstimator.cpp

**Files:**
- Create: `src/sensors/VelocityEstimator.cpp`

- [ ] **Step 1: Escrever implementação completa**

```cpp
// src/sensors/VelocityEstimator.cpp
#include "sensors/VelocityEstimator.h"
#include "config.h"
#include <cstdio>
#include <cmath>

VelocityEstimator::VelocityEstimator(Encoder& enc, IPersistentStore& store, const char* keyPrefix)
    : _enc(enc), _store(store), _keyPrefix(keyPrefix),
      _pprX4(ENCODER_DEFAULT_PPR_X4),
      _alpha(VELOCITY_FILTER_ALPHA),
      _rpmFiltered(0.0f)
{
    snprintf(_key, sizeof(_key), "%s_ppr", _keyPrefix);
}

void VelocityEstimator::begin() {
    if (_store.exists(_key)) {
        _pprX4 = _store.getFloat(_key, ENCODER_DEFAULT_PPR_X4);
    } else {
        _pprX4 = ENCODER_DEFAULT_PPR_X4;
    }
    _rpmFiltered = 0.0f;
    _enc.getDelta();  // descarta delta acumulado antes do início
}

void VelocityEstimator::update(uint32_t dtMs) {
    if (dtMs == 0 || _pprX4 <= 0.0f) {
        // Sem dt válido — mantém valor filtrado anterior, sem novo cálculo
        return;
    }
    int32_t delta = _enc.getDelta();
    // pulsos/ms → rotações/ms → rotações/min: ×60_000 / pprX4
    float rpmRaw = (static_cast<float>(delta) * 60000.0f)
                 / (static_cast<float>(dtMs) * _pprX4);
    _rpmFiltered = _alpha * rpmRaw + (1.0f - _alpha) * _rpmFiltered;
}

float VelocityEstimator::getRPM() const {
    return _rpmFiltered;
}

float VelocityEstimator::getEffectivePPR() const {
    return _pprX4;
}

void VelocityEstimator::setFilterAlpha(float alpha) {
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    _alpha = alpha;
}

void VelocityEstimator::startCalibration() {
    _enc.reset();
    _rpmFiltered = 0.0f;
}

bool VelocityEstimator::finishCalibration(int rotations) {
    if (rotations <= 0) return false;
    int32_t count = _enc.getCount();
    if (count == 0) return false;
    float absCount = static_cast<float>(std::abs(count));
    _pprX4 = absCount / static_cast<float>(rotations);
    _store.putFloat(_key, _pprX4);
    _rpmFiltered = 0.0f;
    return true;
}
```

- [ ] **Step 2: Rodar testes — TODOS devem passar**

```bash
pio test -e native -f test_velocity_estimator
```

Expected:
```
10 Tests 0 Failures 0 Ignored
OK
```

- [ ] **Step 3: Commit (GREEN state)**

```bash
git add src/sensors/VelocityEstimator.cpp
git commit -m "feat(velocity): implement RPM calculation + IIR filter + auto-calibration"
```

---

## Task B5: Implementar NvsStore para ESP32

**Files:**
- Create: `src/storage/NvsStore.h`
- Create: `src/storage/NvsStore.cpp`

- [ ] **Step 1: Criar NvsStore.h**

```cpp
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

private:
#ifndef NATIVE_BUILD
    Preferences _prefs;
#endif
    bool _open = false;
};
```

- [ ] **Step 2: Criar NvsStore.cpp**

```cpp
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
    // Preferences::isKey é const-incompatível em algumas versões — cast seguro
    return const_cast<Preferences&>(_prefs).isKey(key);
}

void NvsStore::clear() {
    if (_open) _prefs.clear();
}

#else  // NATIVE_BUILD — stubs (use InMemoryStore nos testes)

bool  NvsStore::begin(const char*) { _open = false; return false; }
void  NvsStore::end() {}
void  NvsStore::putFloat(const char*, float) {}
float NvsStore::getFloat(const char*, float def) { return def; }
bool  NvsStore::exists(const char*) const { return false; }
void  NvsStore::clear() {}

#endif
```

- [ ] **Step 3: Compilar para esp32dev**

```bash
pio run -e esp32dev 2>&1 | tail -5
```

Expected: build successful.

- [ ] **Step 4: Verificar que testes nativos não regrediram**

```bash
pio test -e native -f test_velocity_estimator 2>&1 | tail -3
```

Expected: `10 Tests 0 Failures 0 Ignored`

- [ ] **Step 5: Commit**

```bash
git add src/storage/NvsStore.h src/storage/NvsStore.cpp
git commit -m "feat(storage): add ESP32 NVS-backed persistent store"
```

---

## Task B6: Smoke test ESP32 — calibrar e ler RPM

**Files:**
- Create: `examples/velocity_calibration_test.cpp`

- [ ] **Step 1: Criar exemplo**

```cpp
// examples/velocity_calibration_test.cpp
// Procedimento:
//   1. Flash neste arquivo (descomentar no platformio se aplicável)
//   2. Abrir Serial Monitor a 115200
//   3. Quando aparecer "START CAL — gire a roda esquerda 10 voltas e digite f":
//      gire a roda 10 voltas manualmente, depois envie 'f' no Serial.
//   4. Repita para a roda direita.
//   5. Tente girar lentamente e veja RPM no Serial.
#ifndef NATIVE_BUILD
#include <Arduino.h>
#include "config.h"
#include "sensors/Encoder.h"
#include "sensors/VelocityEstimator.h"
#include "storage/NvsStore.h"

static Encoder encL(PIN_ENC_LEFT_A,  PIN_ENC_LEFT_B);
static Encoder encR(PIN_ENC_RIGHT_A, PIN_ENC_RIGHT_B);
static NvsStore nvs;
static VelocityEstimator velL(encL, nvs, "left");
static VelocityEstimator velR(encR, nvs, "right");

static const int ROTATIONS = 10;

static void calibrateAxis(const char* label, VelocityEstimator& ve) {
    Serial.printf("START CAL %s — gire %d voltas e envie 'f'\n", label, ROTATIONS);
    ve.startCalibration();
    while (true) {
        if (Serial.available()) {
            int c = Serial.read();
            if (c == 'f' || c == 'F') break;
        }
        delay(50);
    }
    bool ok = ve.finishCalibration(ROTATIONS);
    Serial.printf("CAL %s: ok=%d, PPR_x4=%.2f\n", label, (int)ok, ve.getEffectivePPR());
}

void setup() {
    Serial.begin(115200);
    delay(500);
    encL.begin();
    encR.begin();
    nvs.begin("lfr");
    velL.begin();
    velR.begin();
    Serial.printf("Loaded PPR L=%.2f R=%.2f\n",
        velL.getEffectivePPR(), velR.getEffectivePPR());
    calibrateAxis("LEFT",  velL);
    calibrateAxis("RIGHT", velR);
    Serial.println("RUN — gire as rodas, leitura a cada 200ms");
}

void loop() {
    static uint32_t last = 0;
    uint32_t now = millis();
    uint32_t dt = now - last;
    last = now;
    velL.update(dt);
    velR.update(dt);
    Serial.printf("rpmL=%.1f  rpmR=%.1f\n", velL.getRPM(), velR.getRPM());
    delay(200);
}
#endif
```

- [ ] **Step 2: Compilar para ESP32**

```bash
pio run -e esp32dev 2>&1 | tail -5
```

Expected: build successful.

- [ ] **Step 3: Commit**

```bash
git add examples/velocity_calibration_test.cpp
git commit -m "test(velocity): add bench smoke test for calibration + live RPM"
```

---

## Critério de "Done" — Fase B

- [ ] `pio test -e native -f test_velocity_estimator` → 10/10 PASSED
- [ ] `pio run -e esp32dev` → SUCCESS
- [ ] PPR salvo em NVS sobrevive a reboot do ESP32 (validar com smoke test em bench)
- [ ] InMemoryStore funciona em testes; NvsStore usado em produção
- [ ] Sem regressão em testes existentes:

```bash
pio test -e native 2>&1 | tail -3
```

Expected: todos os suites passam (test_encoder + test_velocity_estimator + suites pré-existentes).
