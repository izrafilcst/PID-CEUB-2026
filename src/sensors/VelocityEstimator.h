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
 *   velL.update(dtUs);        // chamar periodicamente, dt REAL em microssegundos
 *   float rpm = velL.getRPM();
 */
class VelocityEstimator {
public:
    VelocityEstimator(Encoder& enc, IPersistentStore& store, const char* keyPrefix);

    // Carrega PPR persistido (ou usa default). Chamar uma vez no setup.
    void begin();

    // Atualiza RPM dado o intervalo REAL desde a última chamada (microssegundos).
    // dtUs==0 reseta a janela de acumulação (drena delta, não toca no IIR).
    void update(uint32_t dtUs);

    // RPM filtrado (com sinal — negativo = sentido reverso).
    float getRPM() const;

    // PPR efetivo em quadratura 4× (contagens por volta do shaft de saída).
    float getEffectivePPR() const;

    // Ajusta α do filtro IIR. α=1.0 desabilita filtro (uso em testes).
    void setFilterAlpha(float alpha);

    // Janela mínima (µs) de tempo real acumulado antes de estimar RPM.
    void setMinWindowUs(uint32_t us);

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
    // _key holds "<keyPrefix>_ppr" (≤ 15 chars + null per ESP-IDF NVS limit).
    // Construtor faz null-guard e clamp do prefix em 11 chars (15 − len("_ppr")).
    char              _key[16];
    float             _pprX4;
    float             _alpha;
    float             _rpmFiltered;
    bool              _calibrating;  // true entre startCalibration/finishCalibration
    int32_t           _accumCounts;  // pulsos acumulados na janela corrente
    uint32_t          _accumUs;      // tempo real acumulado na janela corrente (µs)
    uint32_t          _minWindowUs;  // janela mínima antes de estimar
};
