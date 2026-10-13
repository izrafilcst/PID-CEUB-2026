#include <unity.h>
#include <cmath>
#include "control/CascadeController.h"
#include "control/Mixer.h"
#include "control/PIDController.h"

static PIDController*     innerL = nullptr;
static PIDController*     innerR = nullptr;
static CascadeController* cc     = nullptr;
static constexpr float RPM_MAX = 1000.0f;
static constexpr float DT = 0.002f;

void setUp() {
    innerL = new PIDController(0.002f, 0.0f, 0.0f, -1.0f, 1.0f);
    innerR = new PIDController(0.002f, 0.0f, 0.0f, -1.0f, 1.0f);
    cc = new CascadeController(*innerL, *innerR);
}

void tearDown() {
    delete cc;     cc = nullptr;
    delete innerR; innerR = nullptr;
    delete innerL; innerL = nullptr;
}

// F-08: no setpoint, o feedforward sozinho mantém o duty
void test_cascade_feedforward_holds_duty_at_setpoint() {
    const WheelCommand c = cc->compute(0.5f, 0.0f, RPM_MAX, 500.0f, 500.0f, DT);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.5f, c.uL);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.5f, c.uR);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 500.0f, c.spL);
}

void test_cascade_slow_motor_gets_more_duty() {
    const WheelCommand c = cc->compute(0.5f, 0.0f, RPM_MAX, 300.0f, 500.0f, DT);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.9f, c.uL);  // 0,5 + 0,002 × 200
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.5f, c.uR);
}

void test_cascade_positive_u_splits_setpoints() {
    const WheelCommand c = cc->compute(0.5f, 0.2f, RPM_MAX, 0.0f, 0.0f, DT);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 700.0f, c.spL);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 300.0f, c.spR);
}

void test_cascade_steering_has_priority_over_speed() {
    const WheelCommand c = cc->compute(0.9f, 0.3f, RPM_MAX, 0.0f, 0.0f, DT);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1000.0f, c.spL);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 400.0f, c.spR);
}

void test_cascade_negative_setpoint_drives_reverse() {
    const WheelCommand c = cc->compute(0.1f, 0.5f, RPM_MAX, 0.0f, 0.0f, DT);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, -400.0f, c.spR);
    TEST_ASSERT_TRUE(c.uR < 0.0f);
}

void test_cascade_output_clamped_to_unit() {
    PIDController bigL(10.0f, 0.0f, 0.0f, -1.0f, 1.0f);
    PIDController bigR(10.0f, 0.0f, 0.0f, -1.0f, 1.0f);
    CascadeController big(bigL, bigR);
    const WheelCommand c = big.compute(0.5f, 0.0f, RPM_MAX, 0.0f, 0.0f, DT);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, c.uL);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, c.uR);
}

void test_cascade_no_windup_when_feedforward_saturates() {
    PIDController iL(0.0f, 1.0f, 0.0f, -1.0f, 1.0f);
    PIDController iR(0.0f, 1.0f, 0.0f, -1.0f, 1.0f);
    CascadeController w(iL, iR);
    for (int i = 0; i < 1000; i++) w.compute(1.0f, 0.0f, RPM_MAX, 0.0f, 0.0f, DT);
    const WheelCommand c = w.compute(0.5f, 0.0f, RPM_MAX, 500.0f, 500.0f, DT);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, c.uL);
}

void test_cascade_nan_inputs_yield_zero_command() {
    const WheelCommand c = cc->compute(NAN, NAN, RPM_MAX, NAN, NAN, DT);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, c.uL);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, c.uR);
}

void test_cascade_invalid_rpm_max_yields_zero() {
    const WheelCommand c = cc->compute(0.5f, 0.0f, 0.0f, 0.0f, 0.0f, DT);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, c.uL);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, c.spL);
}

void test_cascade_set_inner_gains_updates_both_pids() {
    cc->setInnerGains(0.01f, 0.1f, 0.001f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.01f, innerL->getKp());
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.01f, innerR->getKp());
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.1f, innerL->getKi());
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.001f, innerR->getKd());
}

void test_mixer_keeps_difference_when_saturating() {
    float l = 0.0f, r = 0.0f;
    mixSteering(0.8f, 0.5f, l, r);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, l);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, r);
}

void test_mixer_shifts_up_when_below_minus_one() {
    float l = 0.0f, r = 0.0f;
    mixSteering(-0.8f, 0.5f, l, r);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, l);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -1.0f, r);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_cascade_feedforward_holds_duty_at_setpoint);
    RUN_TEST(test_cascade_slow_motor_gets_more_duty);
    RUN_TEST(test_cascade_positive_u_splits_setpoints);
    RUN_TEST(test_cascade_steering_has_priority_over_speed);
    RUN_TEST(test_cascade_negative_setpoint_drives_reverse);
    RUN_TEST(test_cascade_output_clamped_to_unit);
    RUN_TEST(test_cascade_no_windup_when_feedforward_saturates);
    RUN_TEST(test_cascade_nan_inputs_yield_zero_command);
    RUN_TEST(test_cascade_invalid_rpm_max_yields_zero);
    RUN_TEST(test_cascade_set_inner_gains_updates_both_pids);
    RUN_TEST(test_mixer_keeps_difference_when_saturating);
    RUN_TEST(test_mixer_shifts_up_when_below_minus_one);
    return UNITY_END();
}
