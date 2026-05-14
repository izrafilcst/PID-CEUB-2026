#include <unity.h>
#include "control/PIDController.h"

void setUp() {}
void tearDown() {}

void test_pid_zero_error_zero_output() {
    PIDController pid(1.0f, 0.0f, 0.0f, -255.0f, 255.0f);
    float out = pid.compute(0.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, out);
}

void test_pid_proportional_scales_linearly() {
    PIDController pid(2.0f, 0.0f, 0.0f, -1000.0f, 1000.0f);
    float out1 = pid.compute(10.0f, 0.0f);
    PIDController pid2(2.0f, 0.0f, 0.0f, -1000.0f, 1000.0f);
    float out2 = pid2.compute(20.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, out2, out1 * 2.0f);
}

void test_pid_derivative_on_measurement_not_error() {
    // Setpoint muda abruptamente: output NAO deve ter spike de derivativo
    PIDController pid(1.0f, 0.0f, 10.0f, -1000.0f, 1000.0f);
    pid.compute(0.0f, 0.0f);  // aquece
    float before = pid.compute(0.0f, 0.0f);
    float after = pid.compute(100.0f, 0.0f);  // setpoint muda, measurement nao
    // Se derivada fosse no erro, after seria enorme. No processo, measurement=0->0, derivada=0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, before + 100.0f, after);  // so P muda
}

void test_pid_derivative_reduces_overshoot() {
    // Com Kd alto, output cai quando measurement se aproxima do setpoint
    PIDController pid(1.0f, 0.0f, 5.0f, -1000.0f, 1000.0f);
    float out1 = pid.compute(100.0f, 0.0f);    // measurement longe
    float out2 = pid.compute(100.0f, 90.0f);   // measurement chegando
    // out2 deve ser menor que out1 (D freia a aproximacao)
    TEST_ASSERT_TRUE(out2 < out1);
}

void test_pid_integral_accumulates_over_time() {
    PIDController pid(0.0f, 1.0f, 0.0f, -10000.0f, 10000.0f);
    float dt = 0.01f;
    float out1 = pid.compute(10.0f, 0.0f, dt);
    float out2 = pid.compute(10.0f, 0.0f, dt);
    float out3 = pid.compute(10.0f, 0.0f, dt);
    TEST_ASSERT_TRUE(out3 > out2);
    TEST_ASSERT_TRUE(out2 > out1);
}

void test_pid_integral_windup_clamp() {
    PIDController pid(0.0f, 1.0f, 0.0f, -100.0f, 100.0f);
    for (int i = 0; i < 1000; i++) {
        pid.compute(10.0f, 0.0f, 0.01f);
    }
    float out = pid.compute(10.0f, 0.0f, 0.01f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, out);
}

void test_pid_output_clamped() {
    PIDController pid(100.0f, 0.0f, 0.0f, -50.0f, 50.0f);
    float out = pid.compute(100.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, out);
    float out2 = pid.compute(-100.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -50.0f, out2);
}

void test_pid_reset_clears_state() {
    PIDController pid(0.0f, 1.0f, 1.0f, -1000.0f, 1000.0f);
    pid.compute(10.0f, 5.0f, 0.01f);
    pid.compute(10.0f, 6.0f, 0.01f);
    pid.reset();
    float out = pid.compute(0.0f, 0.0f, 0.01f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, out);
}

void test_pid_symmetry() {
    PIDController pid1(2.0f, 0.1f, 5.0f, -1000.0f, 1000.0f);
    PIDController pid2(2.0f, 0.1f, 5.0f, -1000.0f, 1000.0f);
    float dt = 0.01f;
    float out_pos = pid1.compute(0.0f, -10.0f, dt);
    float out_neg = pid2.compute(0.0f, 10.0f, dt);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -out_pos, out_neg);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_pid_zero_error_zero_output);
    RUN_TEST(test_pid_proportional_scales_linearly);
    RUN_TEST(test_pid_derivative_on_measurement_not_error);
    RUN_TEST(test_pid_derivative_reduces_overshoot);
    RUN_TEST(test_pid_integral_accumulates_over_time);
    RUN_TEST(test_pid_integral_windup_clamp);
    RUN_TEST(test_pid_output_clamped);
    RUN_TEST(test_pid_reset_clears_state);
    RUN_TEST(test_pid_symmetry);
    return UNITY_END();
}
