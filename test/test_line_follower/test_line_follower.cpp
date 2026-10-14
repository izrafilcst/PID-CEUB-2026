#include <unity.h>
#include "strategy/LineFollower.h"
#include "sensors/Calibration.h"
#include "control/PIDController.h"

static Calibration*   cal = nullptr;
static PIDController* pid = nullptr;
static LineFollower*  lf  = nullptr;
static constexpr float DT = 0.002f;

static void calibrateFlat(Calibration& c) {
    int lo[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int hi[8] = {1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000};
    c.update(lo);
    c.update(hi);
}

void setUp() {
    cal = new Calibration(8);
    pid = new PIDController(1.0f, 0.0f, 0.0f, -1.0f, 1.0f);
    lf  = new LineFollower(*cal, *pid);
    calibrateFlat(*cal);
}

void tearDown() {
    delete lf;
    delete pid;
    delete cal;
}

void test_line_follower_centered_line_zero_error() {
    int raw[8] = {0, 0, 0, 700, 700, 0, 0, 0};
    const LineReading r = lf->update(raw, DT);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, r.error);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, r.u);
    TEST_ASSERT_FALSE(r.lost);
}

void test_line_follower_line_right_turns_right() {
    int raw[8] = {0, 0, 0, 0, 0, 700, 800, 0};
    const LineReading r = lf->update(raw, DT);
    TEST_ASSERT_TRUE(r.error > 0.0f);
    TEST_ASSERT_TRUE(r.u > 0.0f);
}

void test_line_follower_line_left_turns_left() {
    int raw[8] = {0, 800, 700, 0, 0, 0, 0, 0};
    const LineReading r = lf->update(raw, DT);
    TEST_ASSERT_TRUE(r.error < 0.0f);
    TEST_ASSERT_TRUE(r.u < 0.0f);
}

void test_line_follower_error_normalized_to_unit_range() {
    int raw[8] = {0, 0, 0, 0, 0, 0, 0, 1000};
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, lf->update(raw, DT).error);
}

// F-14: linha perdida satura para o lado onde foi vista por último
void test_line_follower_lost_saturates_to_last_side() {
    int right[8] = {0, 0, 0, 0, 0, 0, 300, 700};
    lf->update(right, DT);
    int none[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    const LineReading r = lf->update(none, DT);
    TEST_ASSERT_TRUE(r.lost);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, r.error);
    TEST_ASSERT_TRUE(r.u > 0.0f);
}

void test_line_follower_lost_without_history_goes_straight() {
    int none[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    const LineReading r = lf->update(none, DT);
    TEST_ASSERT_TRUE(r.lost);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, r.error);
}

// F-01: reset do seguidor não apaga a calibração
void test_line_follower_reset_keeps_calibration() {
    lf->reset(0.0f);
    TEST_ASSERT_TRUE(cal->isValid());
    int raw[8] = {0, 0, 0, 0, 0, 700, 800, 0};
    TEST_ASSERT_TRUE(lf->update(raw, DT).error > 0.0f);
}

void test_line_follower_sensor_order_inverted_flips_error() {
    lf->setSensorOrderInverted(true);
    int raw[8] = {0, 0, 0, 0, 0, 700, 800, 0};
    TEST_ASSERT_TRUE(lf->update(raw, DT).error < 0.0f);
}

void test_line_follower_white_line_follows_low_reading() {
    cal->setWhiteLine(true);
    int raw[8] = {1000, 1000, 1000, 1000, 1000, 300, 200, 1000};
    TEST_ASSERT_TRUE(lf->update(raw, DT).error > 0.0f);
}

void test_line_follower_reset_with_error_avoids_derivative_kick() {
    PIDController dOnly(0.0f, 0.0f, 1.0f, -1.0f, 1.0f);
    LineFollower f(*cal, dOnly);
    int raw[8] = {0, 0, 0, 0, 0, 700, 800, 0};
    f.reset(f.peekError(raw));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, f.update(raw, DT).u);
}

void test_line_follower_derivative_scales_with_dt() {
    PIDController pidA(0.0f, 0.0f, 0.001f, -1.0f, 1.0f);
    PIDController pidB(0.0f, 0.0f, 0.001f, -1.0f, 1.0f);
    LineFollower a(*cal, pidA);
    LineFollower b(*cal, pidB);
    int center[8] = {0, 0, 0, 700, 700, 0, 0, 0};
    int right[8] = {0, 0, 0, 0, 700, 700, 0, 0};
    a.reset(0.0f);
    b.reset(0.0f);
    a.update(center, 0.002f);
    b.update(center, 0.004f);
    const float uA = a.update(right, 0.002f).u;
    const float uB = b.update(right, 0.004f).u;
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 2.0f * uB, uA);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_line_follower_centered_line_zero_error);
    RUN_TEST(test_line_follower_line_right_turns_right);
    RUN_TEST(test_line_follower_line_left_turns_left);
    RUN_TEST(test_line_follower_error_normalized_to_unit_range);
    RUN_TEST(test_line_follower_lost_saturates_to_last_side);
    RUN_TEST(test_line_follower_lost_without_history_goes_straight);
    RUN_TEST(test_line_follower_reset_keeps_calibration);
    RUN_TEST(test_line_follower_sensor_order_inverted_flips_error);
    RUN_TEST(test_line_follower_white_line_follows_low_reading);
    RUN_TEST(test_line_follower_reset_with_error_avoids_derivative_kick);
    RUN_TEST(test_line_follower_derivative_scales_with_dt);
    return UNITY_END();
}
