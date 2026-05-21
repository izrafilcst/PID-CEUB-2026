// src/sensors/VelocityEstimator.cpp
#include "sensors/VelocityEstimator.h"
#include "config.h"
#include <cstdio>
#include <cmath>
#include <cstring>

// Sanity range para PPR persistido — rejeita NaN, Inf, ≤0 e valores absurdos.
static constexpr float PPR_MIN_SANE = 0.5f;
static constexpr float PPR_MAX_SANE = 100000.0f;

// dtMs máximo aceito antes de descartar a janela (jitter, scheduler hiccup,
// primeira chamada). Acima disso o cálculo de RPM perde sentido físico.
static constexpr uint32_t DT_MAX_MS = 500;

// Limite ESP-IDF NVS: chaves ≤ 15 chars. Reservamos "_ppr" (4) → prefix ≤ 11.
static constexpr size_t PREFIX_MAX_LEN = 11;

VelocityEstimator::VelocityEstimator(Encoder& enc, IPersistentStore& store, const char* keyPrefix)
    : _enc(enc), _store(store),
      _keyPrefix(keyPrefix ? keyPrefix : "vel"),  // null-guard
      _pprX4(ENCODER_DEFAULT_PPR_X4),
      _alpha(VELOCITY_FILTER_ALPHA),
      _rpmFiltered(0.0f),
      _calibrating(false)
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
    // Descarta delta acumulado antes do primeiro uso — intencional.
    // Sem isso, contagens stale antes do begin() corrompem o 1º RPM.
    // Mantenedores futuros: NÃO remover esta chamada.
    _enc.getDelta();
}

void VelocityEstimator::update(uint32_t dtMs) {
    // Sempre drena delta — evita acumulação que distorceria a próxima janela.
    int32_t delta = _enc.getDelta();
    // Durante calibração: usuário gira a roda manualmente; suprimir o IIR
    // evita emitir RPM mentiroso para o PID externo (Wave 3 MED-1).
    if (_calibrating) return;
    if (dtMs == 0 || dtMs > DT_MAX_MS || _pprX4 <= 0.0f) {
        // dt inválido (jitter, primeira chamada, stall) — descarta janela,
        // mantém o RPM filtrado anterior.
        return;
    }
    // pulsos/ms → rotações/min: ×60_000 / pprX4
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
    // NaN/Inf falham < e > silenciosamente — rejeita explicitamente.
    if (!std::isfinite(alpha)) return;
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    _alpha = alpha;
}

void VelocityEstimator::startCalibration() {
    _enc.reset();
    _rpmFiltered = 0.0f;
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
    _calibrating = false;
    // Sincroniza baseline de delta para a próxima update() começar do zero.
    // Sem isso, contagens da calibração vazariam no 1º RPM pós-cal.
    _enc.getDelta();
    return true;
}
