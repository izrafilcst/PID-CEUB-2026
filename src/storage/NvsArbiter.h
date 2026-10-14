#pragma once
#include <atomic>
#include <cstdint>

/**
 * Exclusão mútua entre acesso à NVS (task comm: save/load/prof) e carro armado ou correndo
 * (task control), A-WP1-05 / spec §4.4. Um único estado atômico, trocado por compare-and-swap:
 * quem chega primeiro ganha, sem janela entre "verificar" e "agir".
 *   FREE → NVS    tryBeginWrite() (comm); falha se o carro está armado
 *   FREE → ACTIVE tryArm()        (control, ao entrar em ARMED); falha durante a escrita
 * Cada dono só libera o próprio estado.
 */
class NvsArbiter {
public:
    bool tryBeginWrite() { return _swap(FREE, NVS); }
    void endWrite() { _swap(NVS, FREE); }
    bool tryArm() { return _swap(FREE, ACTIVE); }
    void disarm() { _swap(ACTIVE, FREE); }

    bool writeInProgress() const { return _owner.load() == NVS; }
    bool armed() const { return _owner.load() == ACTIVE; }

private:
    static constexpr uint32_t FREE = 0;
    static constexpr uint32_t NVS = 1;
    static constexpr uint32_t ACTIVE = 2;

    bool _swap(uint32_t from, uint32_t to) {
        uint32_t expected = from;
        return _owner.compare_exchange_strong(expected, to);
    }

    std::atomic<uint32_t> _owner{FREE};
};

/** RAII da escrita: adquire no construtor, libera no destrutor. Sem árbitro, sempre adquire. */
class NvsWriteGuard {
public:
    explicit NvsWriteGuard(NvsArbiter* arbiter)
        : _arbiter(arbiter), _acquired(!arbiter || arbiter->tryBeginWrite()) {}
    ~NvsWriteGuard() {
        if (_arbiter && _acquired) _arbiter->endWrite();
    }
    NvsWriteGuard(const NvsWriteGuard&) = delete;
    NvsWriteGuard& operator=(const NvsWriteGuard&) = delete;

    bool acquired() const { return _acquired; }

private:
    NvsArbiter* _arbiter;
    bool        _acquired;
};
