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

// Injeta N períodos de quadratura para frente (4 contagens por período).
static void injectPeriods(Encoder& e, int periods) {
    for (int i = 0; i < periods; i++) {
        e._simulatePulse(false, true);
        e._simulatePulse(true,  true);
        e._simulatePulse(true,  false);
        e._simulatePulse(false, false);
    }
}

// ─── 1. RPM zero quando não há pulsos ─────────────────────────────────────
void test_velocity_zero_when_no_pulses() {
    ve->update(10000);  // 10ms passaram, zero pulsos
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ve->getRPM());
}

// ─── 2. Conversão pulsos→RPM correta com PPR padrão ──────────────────────
void test_velocity_converts_pulses_to_rpm() {
    // PPR_x4 default = 28. Em 100ms (0.1s), 28 pulsos = 1 rotação = 10 rps = 600 RPM
    ve->setFilterAlpha(1.0f);  // desabilita filtro para teste determinístico
    simulateRotations(*enc, 1);
    ve->update(100000);  // 100 ms
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 600.0f, ve->getRPM());
}

// ─── 3. Filtro low-pass IIR suaviza spikes ────────────────────────────────
void test_velocity_low_pass_smooths_spikes() {
    ve->setFilterAlpha(0.3f);  // peso 30% no novo valor
    // Primeiro update: estabilize em 600 RPM
    for (int i = 0; i < 20; i++) {
        simulateRotations(*enc, 1);
        ve->update(100000);
    }
    float steady = ve->getRPM();
    TEST_ASSERT_FLOAT_WITHIN(20.0f, 600.0f, steady);
    // Spike: 5× a velocidade em 1 update
    simulateRotations(*enc, 5);
    ve->update(100000);
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
    ve->update(100000);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, -600.0f, ve->getRPM());
}

// ─── Pulsos NÃO são perdidos em ticks menores que a janela mínima ─────────
void test_velocity_accumulates_pulses_across_subwindow_ticks() {
    ve->setFilterAlpha(1.0f);   // sem IIR — resultado determinístico
    ve->setMinWindowUs(6000);   // janela mínima = 6 ms
    injectPeriods(*enc, 4);     // 16 contagens
    ve->update(4000);           // 4 ms < 6 ms → acumula, NÃO estima ainda
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ve->getRPM());
    injectPeriods(*enc, 3);     // +12 contagens = 28 no total
    ve->update(4000);           // acumUs=8 ms ≥ 6 ms → estima sobre 8 ms, 28 contagens
    // 28 * 60e6 / (8000 µs * 28 PPR) = 7500 RPM
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 7500.0f, ve->getRPM());
}

// ─── Abaixo da janela mínima, mantém o RPM anterior ───────────────────────
void test_velocity_holds_rpm_below_min_window() {
    ve->setFilterAlpha(1.0f);
    ve->setMinWindowUs(6000);
    injectPeriods(*enc, 7);     // 28 contagens
    ve->update(2000);           // 2 ms < 6 ms → não estima
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ve->getRPM());
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
    ve->update(100000);
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
    RUN_TEST(test_velocity_accumulates_pulses_across_subwindow_ticks);
    RUN_TEST(test_velocity_holds_rpm_below_min_window);
    RUN_TEST(test_velocity_handles_zero_dt_safely);
    RUN_TEST(test_velocity_signed_with_direction);
    RUN_TEST(test_velocity_default_ppr_when_no_calibration);
    RUN_TEST(test_velocity_start_calibration_resets_count);
    RUN_TEST(test_velocity_finish_calibration_stores_ppr);
    RUN_TEST(test_velocity_uses_calibrated_ppr);
    RUN_TEST(test_velocity_begin_loads_persisted_ppr);
    return UNITY_END();
}
