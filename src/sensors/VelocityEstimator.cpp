// src/sensors/VelocityEstimator.cpp
#include "sensors/VelocityEstimator.h"
#include "config.h"
#include <cstdio>
#include <cmath>
#include <cstring>

// Sanity range para PPR persistido — rejeita NaN, Inf, ≤0 e valores absurdos.
static constexpr float PPR_MIN_SANE = 0.5f;
static constexpr float PPR_MAX_SANE = 100000.0f;

// dtUs acumulado máximo antes de descartar a janela (stall / scheduler hiccup).
static constexpr uint32_t DT_MAX_US = 500000;  // 0,5 s

// Limite ESP-IDF NVS: chaves ≤ 15 chars. Reservamos "_ppr" (4) → prefix ≤ 11.
static constexpr size_t PREFIX_MAX_LEN = 11;

VelocityEstimator::VelocityEstimator(Encoder& enc, IPersistentStore& store, const char* keyPrefix)
    : _enc(enc), _store(store),
      _keyPrefix(keyPrefix ? keyPrefix : "vel"),  // null-guard
      _pprX4(ENCODER_DEFAULT_PPR_X4),
      _alpha(VELOCITY_FILTER_ALPHA),
      _rpmFiltered(0.0f),
      _calibrating(false),
      _accumCounts(0),
      _accumUs(0),
      _minWindowUs(VELOCITY_MIN_WINDOW_US)
{
    // Clamp prefix em 11 chars antes de compor a chave NVS.
    // Sem isso, prefixes longos seriam silenciosamente rejeitados pelo
    // Preferences::putFloat e a calibração nunca persistiria.
    char clamped[PREFIX_MAX_LEN + 1];
    size_t plen = strnlen(_keyPrefix, PREFIX_MAX_LEN + 1);
    const char* safe = _keyPrefix;
    if (plen > PREFIX_MAX_LEN) {
        memcpy(clamped, _keyPrefix, PREFIX_MAX_LEN);
        clamped[PREFIX_MAX_LEN] = '\0';
        safe = clamped;
    }
    snprintf(_key, sizeof(_key), "%s_ppr", safe);
}

void VelocityEstimator::begin() {
    // Carrega PPR persistido COM validação de sanidade.
    // NVS pode retornar NaN/Inf/0/negativo após corrupção de página — esses
    // valores envenenariam o RPM (NaN × qualquer = NaN). Falha → default.
    float loaded = ENCODER_DEFAULT_PPR_X4;
    if (_store.exists(_key)) {
        loaded = _store.getFloat(_key, ENCODER_DEFAULT_PPR_X4);
    }
    if (std::isfinite(loaded) && loaded >= PPR_MIN_SANE && loaded <= PPR_MAX_SANE) {
        _pprX4 = loaded;
    } else {
        _pprX4 = ENCODER_DEFAULT_PPR_X4;
    }
    _rpmFiltered = 0.0f;
    _calibrating = false;
    _accumCounts = 0;
    _accumUs     = 0;
    // Descarta delta acumulado antes do primeiro uso — intencional.
    // Sem isso, contagens stale antes do begin() corrompem o 1º RPM.
    // Mantenedores futuros: NÃO remover esta chamada.
    _enc.getDelta();
}

void VelocityEstimator::update(uint32_t dtUs) {
    // Sempre drena o delta do encoder para manter os contadores vivos.
    int32_t delta = _enc.getDelta();

    // Durante calibração o usuário gira a roda à mão; não alimentar o IIR.
    if (_calibrating) {
        _accumCounts = 0;
        _accumUs     = 0;
        return;
    }

    // dtUs==0 = sinal de reset (1ª chamada / re-entry): zera a janela.
    if (dtUs == 0) {
        _accumCounts = 0;
        _accumUs     = 0;
        return;
    }

    // Acumula este tick na janela corrente — pulsos NUNCA são descartados.
    _accumCounts += delta;
    _accumUs     += dtUs;

    // Stall: a janela cresceu demais (loop travou) → descarta, mantém RPM.
    if (_accumUs > DT_MAX_US) {
        _accumCounts = 0;
        _accumUs     = 0;
        return;
    }

    // Tempo real insuficiente para estimativa confiável → segue acumulando.
    if (_accumUs < _minWindowUs) return;

    // Estima sobre o dt REAL da janela (variável), então reinicia a janela.
    if (_pprX4 > 0.0f) {
        float rpmRaw = (static_cast<float>(_accumCounts) * 60000000.0f)
                     / (static_cast<float>(_accumUs) * _pprX4);
        _rpmFiltered = _alpha * rpmRaw + (1.0f - _alpha) * _rpmFiltered;
    }
    _accumCounts = 0;
    _accumUs     = 0;
}

void VelocityEstimator::setMinWindowUs(uint32_t us) {
    _minWindowUs = us;
}

float VelocityEstimator::getRPM() const {
    return _rpmFiltered;
}

float VelocityEstimator::getEffectivePPR() const {
    return _pprX4;
}

void VelocityEstimator::setFilterAlpha(float alpha) {
    // NaN/Inf falham < e > silenciosamente — rejeita explicitamente.
    if (!std::isfinite(alpha)) return;
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    _alpha = alpha;
}

void VelocityEstimator::startCalibration() {
    _enc.reset();
    _rpmFiltered = 0.0f;
    _accumCounts = 0;
    _accumUs     = 0;
    _calibrating = true;
}

bool VelocityEstimator::finishCalibration(int rotations) {
    if (rotations <= 0) {
        _calibrating = false;  // ainda libera o flag mesmo em falha
        return false;
    }
    int32_t count = _enc.getCount();
    if (count == 0) {
        _calibrating = false;
        return false;
    }
    // std::abs(INT32_MIN) é UB em C++ (overflow signed) — cast antes p/ float
    // garante valor absoluto seguro para qualquer entrada de int32_t.
    float absCount = std::fabs(static_cast<float>(count));
    _pprX4 = absCount / static_cast<float>(rotations);
    _store.putFloat(_key, _pprX4);
    _rpmFiltered = 0.0f;
    _accumCounts = 0;
    _accumUs     = 0;
    _calibrating = false;
    // Sincroniza baseline de delta para a próxima update() começar do zero.
    // Sem isso, contagens da calibração vazariam no 1º RPM pós-cal.
    _enc.getDelta();
    return true;
}
