#include "strategy/ControlLoop.h"
#include <cmath>
#include <cstdlib>
#include "config.h"
#include "control/Mixer.h"

ControlLoop::ControlLoop(Calibration& cal, int pwmMax)
    : _cal(cal),
      _outer(1.0f, 0.0f, 0.0f, -1.0f, 1.0f),
      _innerL(0.0f, 0.0f, 0.0f, -1.0f, 1.0f),
      _innerR(0.0f, 0.0f, 0.0f, -1.0f, 1.0f),
      _line(cal, _outer),
      _cascade(_innerL, _innerR),
      _output(pwmMax),
      _cfg(robotcfg::factoryConfig()),
      _pwmMax(pwmMax) {
    applyConfig(_cfg);
}

void ControlLoop::applyConfig(const RobotConfig& cfg) {
    _cfg = cfg;
    _cal.setWhiteLine(cfg.hw.pol == 1);
    _line.setSensorOrderInverted(cfg.hw.sinv == 1);
    _outer.setTunings(cfg.tune.kp, cfg.tune.ki, cfg.tune.kd);
    _outer.setDerivativeFilter(cfg.tune.dlp);
    _cascade.setInnerGains(cfg.tune.kpv, cfg.tune.kiv, cfg.tune.kdv);
    _speed.setParams(cfg.tune.vmin, cfg.tune.vmax, cfg.tune.thr, cfg.tune.acc);
}

void ControlLoop::start(const int* raw, float rpmL, float rpmR) {
    _line.reset(_line.peekError(raw));
    _speed.reset(0.0f);
    _cascade.reset(_signed(rpmL, _cfg.hw.einvL), _signed(rpmR, _cfg.hw.einvR));
    _lostUs = 0;
    _encStallUs = 0;
}

bool ControlLoop::_encoderStalled(const ControlInputs& in, const MotorDuties& d) const {
    if (_cfg.tune.mode != 1) return false;
    // duties vêm por canal do TB6612: com swap, o canal A é a roda direita
    const int a = std::abs(d.a);
    const int b = std::abs(d.b);
    const int dutyL = _cfg.hw.swap ? b : a;
    const int dutyR = _cfg.hw.swap ? a : b;
    const float dutyMin = ENC_FAULT_DUTY * static_cast<float>(_pwmMax);
    const float rpmMin = ENC_FAULT_RPM_FRAC * _cfg.tune.rpmMax;
    const bool left = static_cast<float>(dutyL) >= dutyMin && std::fabs(in.rpmL) < rpmMin;
    const bool right = static_cast<float>(dutyR) >= dutyMin && std::fabs(in.rpmR) < rpmMin;
    return left || right;
}

ControlOutputs ControlLoop::step(const ControlInputs& in) {
    ControlOutputs out;
    const float dt = (std::isfinite(in.dtS) && in.dtS > 0.0f) ? in.dtS : 0.0f;

    const LineReading r = _line.update(in.raw, dt);
    out.error = r.error;
    out.u = r.u;
    out.lost = r.lost;

    // Tempo de linha perdida em µs inteiros: comparação exata com lostMs
    _lostUs = r.lost ? _lostUs + static_cast<uint32_t>(std::lround(dt * 1e6f)) : 0;
    out.lostTimeout = _lostUs >= static_cast<uint32_t>(_cfg.tune.lostMs) * 1000u;

    // Linha perdida conta como erro máximo: anda em vmin enquanto procura (Review Focus 5)
    out.base = _speed.step(r.lost ? 1.0f : std::fabs(r.error), dt);

    if (_cfg.tune.mode == 0) {
        mixSteering(out.base, r.u, out.uL, out.uR);
    } else {
        const WheelCommand wc = _cascade.compute(
            out.base, r.u, _cfg.tune.rpmMax,
            _signed(in.rpmL, _cfg.hw.einvL), _signed(in.rpmR, _cfg.hw.einvR), dt);
        out.uL = wc.uL;
        out.uR = wc.uR;
        out.spL = wc.spL;
        out.spR = wc.spR;
    }

    out.duties = _output.compute(out.uL, out.uR, in.vbat, _cfg.hw, _cfg.tune);

    // E-4: sem interrupção por ENC_FAULT_MS; zera quando a condição cai e no start()
    const uint32_t dtUs = static_cast<uint32_t>(std::lround(dt * 1e6f));
    _encStallUs = _encoderStalled(in, out.duties) ? _encStallUs + dtUs : 0;
    out.encoderFault = _encStallUs >= static_cast<uint32_t>(ENC_FAULT_MS) * 1000u;
    return out;
}
