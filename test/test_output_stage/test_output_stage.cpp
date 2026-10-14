#include <unity.h>
#include <cmath>
#include "config/RobotConfig.h"
#include "motors/OutputStage.h"

static HardwareConfig hw;
static TuningProfile tune;
static const OutputStage out(1023);

void setUp() {
    hw = robotcfg::factoryHardware();
    tune = robotcfg::factoryProfile(0);
}
void tearDown() {}

void test_output_stage_passthrough_without_compensation() {
    tune.vcomp = 0;
    const MotorDuties d = out.compute(0.5f, -0.25f, 8.4f, hw, tune);
    TEST_ASSERT_EQUAL_INT(512, d.a);   // round(0,5 × 1023)
    TEST_ASSERT_EQUAL_INT(-256, d.b);
}

void test_output_stage_compensates_full_battery() {
    const MotorDuties d = out.compute(0.5f, 0.5f, 8.4f, hw, tune);  // × 7,4/8,4
    TEST_ASSERT_EQUAL_INT(451, d.a);
}

void test_output_stage_compensates_sagging_battery() {
    const MotorDuties d = out.compute(0.5f, 0.5f, 6.4f, hw, tune);  // × 7,4/6,4
    TEST_ASSERT_EQUAL_INT(591, d.a);
}

void test_output_stage_saturates_after_compensation() {
    const MotorDuties d = out.compute(1.0f, -1.0f, 6.4f, hw, tune);
    TEST_ASSERT_EQUAL_INT(1023, d.a);
    TEST_ASSERT_EQUAL_INT(-1023, d.b);
}

// Review Focus 2
void test_output_stage_invalid_vbat_disables_compensation() {
    TEST_ASSERT_EQUAL_INT(512, out.compute(0.5f, 0.5f, 0.0f, hw, tune).a);
    TEST_ASSERT_EQUAL_INT(512, out.compute(0.5f, 0.5f, NAN, hw, tune).a);
    TEST_ASSERT_EQUAL_INT(512, out.compute(0.5f, 0.5f, 2.9f, hw, tune).a);
}

void test_output_stage_inverts_each_side() {
    tune.vcomp = 0;
    hw.invL = 1;
    const MotorDuties d = out.compute(0.5f, 0.5f, 7.4f, hw, tune);
    TEST_ASSERT_EQUAL_INT(-512, d.a);
    TEST_ASSERT_EQUAL_INT(512, d.b);
}

void test_output_stage_swaps_channels() {
    tune.vcomp = 0;
    hw.swap = 1;
    const MotorDuties d = out.compute(0.5f, -0.25f, 7.4f, hw, tune);
    TEST_ASSERT_EQUAL_INT(-256, d.a);
    TEST_ASSERT_EQUAL_INT(512, d.b);
}

void test_output_stage_non_finite_command_is_zero() {
    const MotorDuties d = out.compute(NAN, INFINITY, 7.4f, hw, tune);
    TEST_ASSERT_EQUAL_INT(0, d.a);
    TEST_ASSERT_EQUAL_INT(0, d.b);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_output_stage_passthrough_without_compensation);
    RUN_TEST(test_output_stage_compensates_full_battery);
    RUN_TEST(test_output_stage_compensates_sagging_battery);
    RUN_TEST(test_output_stage_saturates_after_compensation);
    RUN_TEST(test_output_stage_invalid_vbat_disables_compensation);
    RUN_TEST(test_output_stage_inverts_each_side);
    RUN_TEST(test_output_stage_swaps_channels);
    RUN_TEST(test_output_stage_non_finite_command_is_zero);
    return UNITY_END();
}
