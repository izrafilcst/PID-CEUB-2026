// src/sensors/VelocityEstimator.cpp
#include "sensors/VelocityEstimator.h"
#include "config.h"
#include <cstdio>
#include <cmath>
#include <cstring>

// Faixa do PPR igual à do RobotConfig (pprL/pprR): rejeita NaN, Inf e valores fora dela.
// Um PPR fora da faixa faria todo save falhar e seria descartado no boot (A-WP1-04).
static constexpr float PPR_MIN_SANE = ENCODER_PPR_X4_MIN;
static constexpr float PPR_MAX_SANE = ENCODER_PPR_X4_MAX;

// dt máximo aceito antes de descartar a janela (jitter, primeira chamada, stall).
static constexpr uint32_t DT_MAX_US = 500000;

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

void VelocityEstimator::update(uint32_t dtUs) {
    // Sempre drena delta — evita acumulação que distorceria a próxima janela.
    const int32_t delta = _enc.getDelta();
    // Durante calibração: usuário gira a roda manualmente; suprimir o IIR
    // evita emitir RPM mentiroso para o PID externo (Wave 3 MED-1).
    if (_calibrating) return;
    if (dtUs == 0 || dtUs > DT_MAX_US || _pprX4 <= 0.0f) return;
    // contagens/µs → rotações/min: × 60 000 000 / pprX4
    const float rpmRaw = (static_cast<float>(delta) * 60000000.0f)
                       / (static_cast<float>(dtUs) * _pprX4);
    _rpmFiltered = _alpha * rpmRaw + (1.0f - _alpha) * _rpmFiltered;
}

bool VelocityEstimator::setPprX4(float pprX4) {
    if (!std::isfinite(pprX4) || pprX4 < PPR_MIN_SANE || pprX4 > PPR_MAX_SANE) return false;
    _pprX4 = pprX4;
    return true;
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

void VelocityEstimator::cancelCalibration() {
    if (!_calibrating) return;
    _calibrating = false;
    _rpmFiltered = 0.0f;
    _enc.getDelta();  // as voltas da calibração cancelada não entram no próximo RPM
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
    const float ppr = absCount / static_cast<float>(rotations);
    if (ppr < PPR_MIN_SANE || ppr > PPR_MAX_SANE) {
        // Fora da faixa: recusa, mantém o PPR atual e não grava nada
        _calibrating = false;
        _enc.getDelta();
        return false;
    }
    _pprX4 = ppr;
    _store.putFloat(_key, _pprX4);
    _rpmFiltered = 0.0f;
    _calibrating = false;
    // Sincroniza baseline de delta para a próxima update() começar do zero.
    // Sem isso, contagens da calibração vazariam no 1º RPM pós-cal.
    _enc.getDelta();
    return true;
}
