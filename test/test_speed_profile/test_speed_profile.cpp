#include <unity.h>
#include "control/SpeedProfile.h"

void setUp() {}
void tearDown() {}

void test_zero_error_max_speed() {
    SpeedProfile sp(60, 160, 230, 0.6f);
    TEST_ASSERT_EQUAL(230, sp.compute(0.0f));
}

void test_large_error_min_speed() {
    SpeedProfile sp(60, 160, 230, 0.6f);
    TEST_ASSERT_EQUAL(60, sp.compute(1.0f));
    TEST_ASSERT_EQUAL(60, sp.compute(-1.0f));
    TEST_ASSERT_EQUAL(60, sp.compute(2.0f));
}

void test_threshold_boundary() {
    SpeedProfile sp(60, 160, 230, 0.6f);
    int below = sp.compute(0.59f);
    int above = sp.compute(0.61f);
    TEST_ASSERT_TRUE(below > above);
}

void test_symmetry_positive_negative() {
    SpeedProfile sp(60, 160, 230, 0.6f);
    TEST_ASSERT_EQUAL(sp.compute(0.3f), sp.compute(-0.3f));
}

void test_clamps_to_bounds() {
    SpeedProfile sp(60, 160, 230, 0.6f);
    TEST_ASSERT_TRUE(sp.compute(0.0f) <= 230);
    TEST_ASSERT_TRUE(sp.compute(1.0f) >= 60);
}

void test_params_updateable_runtime() {
    SpeedProfile sp(60, 160, 230, 0.6f);
    sp.setParams(80, 150, 200, 0.5f);
    TEST_ASSERT_EQUAL(200, sp.compute(0.0f));
    TEST_ASSERT_EQUAL(80, sp.compute(1.0f));
}

void test_linear_interpolation() {
    SpeedProfile sp(60, 160, 230, 0.6f);
    int mid = sp.compute(0.3f);
    TEST_ASSERT_TRUE(mid > 60 && mid < 230);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_zero_error_max_speed);
    RUN_TEST(test_large_error_min_speed);
    RUN_TEST(test_threshold_boundary);
    RUN_TEST(test_symmetry_positive_negative);
    RUN_TEST(test_clamps_to_bounds);
    RUN_TEST(test_params_updateable_runtime);
    RUN_TEST(test_linear_interpolation);
    return UNITY_END();
}
