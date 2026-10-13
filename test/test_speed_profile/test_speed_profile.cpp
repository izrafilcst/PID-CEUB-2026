#include <unity.h>
#include <cmath>
#include "control/SpeedProfile.h"

static SpeedProfile sp;

void setUp() {
    sp = SpeedProfile();
    sp.setParams(0.25f, 0.5f, 0.6f, 2.0f);
    sp.reset(0.0f);
}
void tearDown() {}

void test_speed_profile_zero_error_returns_vmax() {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, sp.target(0.0f));
}

void test_speed_profile_error_at_threshold_returns_vmin() {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.25f, sp.target(0.6f));
}

void test_speed_profile_error_beyond_threshold_returns_vmin() {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.25f, sp.target(1.0f));
}

void test_speed_profile_midpoint_interpolates() {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.375f, sp.target(0.3f));
}

void test_speed_profile_negative_error_uses_magnitude() {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.375f, sp.target(-0.3f));
}

void test_speed_profile_nan_error_returns_vmin() {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.25f, sp.target(NAN));
}

void test_speed_profile_ramp_limits_rise() {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.2f, sp.step(0.0f, 0.1f));  // 2,0/s × 0,1 s
}

void test_speed_profile_ramp_reaches_target_and_holds() {
    for (int i = 0; i < 10; i++) sp.step(0.0f, 0.1f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, sp.current());
}

void test_speed_profile_fall_is_immediate() {
    sp.reset(0.5f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.25f, sp.step(0.6f, 0.002f));
}

void test_speed_profile_zero_dt_does_not_rise() {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, sp.step(0.0f, 0.0f));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_speed_profile_zero_error_returns_vmax);
    RUN_TEST(test_speed_profile_error_at_threshold_returns_vmin);
    RUN_TEST(test_speed_profile_error_beyond_threshold_returns_vmin);
    RUN_TEST(test_speed_profile_midpoint_interpolates);
    RUN_TEST(test_speed_profile_negative_error_uses_magnitude);
    RUN_TEST(test_speed_profile_nan_error_returns_vmin);
    RUN_TEST(test_speed_profile_ramp_limits_rise);
    RUN_TEST(test_speed_profile_ramp_reaches_target_and_holds);
    RUN_TEST(test_speed_profile_fall_is_immediate);
    RUN_TEST(test_speed_profile_zero_dt_does_not_rise);
    return UNITY_END();
}
