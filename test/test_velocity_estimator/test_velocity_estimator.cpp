#include <unity.h>
#include <cmath>
#include "config.h"
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
    ve->begin();
}

void tearDown() {
    delete ve;    ve = nullptr;
    delete store; store = nullptr;
    delete enc;   enc = nullptr;
}

static void forwardPeriods(Encoder& e, int periods) {
    for (int i = 0; i < periods; i++) {
        e._simulatePulse(false, true);
        e._simulatePulse(true,  true);
        e._simulatePulse(true,  false);
        e._simulatePulse(false, false);
    }
}

// Uma volta do eixo de saída = countsPerRotation contagens (4 por período de quadratura)
static void simulateRotations(Encoder& e, int rotations,
                              int countsPerRotation = static_cast<int>(ENCODER_DEFAULT_PPR_X4)) {
    forwardPeriods(e, rotations * countsPerRotation / 4);
}

void test_velocity_zero_when_no_pulses() {
    ve->update(10000);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ve->getRPM());
}

void test_velocity_converts_pulses_to_rpm() {
    ve->setFilterAlpha(1.0f);
    simulateRotations(*enc, 1);
    ve->update(100000);  // 1 volta em 100 ms = 600 RPM
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 600.0f, ve->getRPM());
}

void test_velocity_low_pass_smooths_spikes() {
    ve->setFilterAlpha(0.3f);
    for (int i = 0; i < 20; i++) {
        simulateRotations(*enc, 1);
        ve->update(100000);
    }
    const float steady = ve->getRPM();
    TEST_ASSERT_FLOAT_WITHIN(20.0f, 600.0f, steady);
    simulateRotations(*enc, 5);
    ve->update(100000);
    const float afterSpike = ve->getRPM();
    TEST_ASSERT_TRUE(afterSpike < 3000.0f);
    TEST_ASSERT_TRUE(afterSpike > steady);
}

void test_velocity_handles_zero_dt_safely() {
    simulateRotations(*enc, 1);
    ve->update(0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ve->getRPM());
}

void test_velocity_signed_with_direction() {
    ve->setFilterAlpha(1.0f);
    for (int i = 0; i < 150; i++) {  // 150 períodos para trás = -600 contagens = -1 volta
        enc->_simulatePulse(true,  false);
        enc->_simulatePulse(true,  true);
        enc->_simulatePulse(false, true);
        enc->_simulatePulse(false, false);
    }
    ve->update(100000);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, -600.0f, ve->getRPM());
}

// F-17: o padrão considera a redução (12 CPR × 50:1)
void test_velocity_default_ppr_when_no_calibration() {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 600.0f, ve->getEffectivePPR());
}

void test_velocity_start_calibration_resets_count() {
    simulateRotations(*enc, 3);
    ve->startCalibration();
    TEST_ASSERT_EQUAL_INT32(0, enc->getCount());
}

void test_velocity_finish_calibration_stores_ppr() {
    ve->startCalibration();
    simulateRotations(*enc, 10, 40);
    TEST_ASSERT_TRUE(ve->finishCalibration(10));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 40.0f, ve->getEffectivePPR());
    TEST_ASSERT_TRUE(store->exists("left_ppr"));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 40.0f, store->getFloat("left_ppr", 0.0f));
}

void test_velocity_uses_calibrated_ppr() {
    ve->startCalibration();
    simulateRotations(*enc, 10, 40);
    ve->finishCalibration(10);
    ve->setFilterAlpha(1.0f);
    simulateRotations(*enc, 1, 40);
    ve->update(100000);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 600.0f, ve->getRPM());
}

void test_velocity_begin_loads_persisted_ppr() {
    store->putFloat("left_ppr", 56.0f);
    VelocityEstimator ve2(*enc, *store, "left");
    ve2.begin();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 56.0f, ve2.getEffectivePPR());
}

// F-06: com laço de 0,5 ms o RPM continua sendo atualizado
void test_velocity_sub_millisecond_dt_updates_rpm() {
    ve->setFilterAlpha(1.0f);
    forwardPeriods(*enc, 1);  // 4 contagens
    ve->update(500);          // 4 × 60e6 / (500 × 600) = 800 RPM
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 800.0f, ve->getRPM());
}

void test_velocity_discards_window_longer_than_limit() {
    ve->setFilterAlpha(1.0f);
    simulateRotations(*enc, 1);
    ve->update(600000);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ve->getRPM());
}

void test_velocity_set_ppr_validates() {
    TEST_ASSERT_FALSE(ve->setPprX4(0.0f));
    TEST_ASSERT_FALSE(ve->setPprX4(NAN));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 600.0f, ve->getEffectivePPR());
    TEST_ASSERT_TRUE(ve->setPprX4(1400.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1400.0f, ve->getEffectivePPR());
    TEST_ASSERT_FALSE(store->exists("left_ppr"));  // não persiste
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
    RUN_TEST(test_velocity_sub_millisecond_dt_updates_rpm);
    RUN_TEST(test_velocity_discards_window_longer_than_limit);
    RUN_TEST(test_velocity_set_ppr_validates);
    return UNITY_END();
}
