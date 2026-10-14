#include <unity.h>
#include <cmath>
#include "control/PIDController.h"

void setUp() {}
void tearDown() {}

static constexpr float DT = 0.01f;

void test_pid_zero_error_zero_output() {
    PIDController pid(1.0f, 0.0f, 0.0f, -255.0f, 255.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, pid.compute(0.0f, 0.0f, DT));
}

void test_pid_proportional_scales_linearly() {
    PIDController a(2.0f, 0.0f, 0.0f, -1000.0f, 1000.0f);
    PIDController b(2.0f, 0.0f, 0.0f, -1000.0f, 1000.0f);
    const float out1 = a.compute(10.0f, 0.0f, DT);
    const float out2 = b.compute(20.0f, 0.0f, DT);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, out1 * 2.0f, out2);
}

void test_pid_derivative_on_measurement_not_error() {
    PIDController pid(1.0f, 0.0f, 10.0f, -1000.0f, 1000.0f);
    pid.compute(0.0f, 0.0f, DT);
    const float before = pid.compute(0.0f, 0.0f, DT);
    const float after = pid.compute(100.0f, 0.0f, DT);  // setpoint salta, medida parada
    TEST_ASSERT_FLOAT_WITHIN(0.001f, before + 100.0f, after);
}

void test_pid_derivative_reduces_overshoot() {
    PIDController pid(1.0f, 0.0f, 5.0f, -1000.0f, 1000.0f);
    const float out1 = pid.compute(100.0f, 0.0f, DT);
    const float out2 = pid.compute(100.0f, 90.0f, DT);
    TEST_ASSERT_TRUE(out2 < out1);
}

void test_pid_integral_accumulates_over_time() {
    PIDController pid(0.0f, 1.0f, 0.0f, -10000.0f, 10000.0f);
    const float out1 = pid.compute(10.0f, 0.0f, DT);
    const float out2 = pid.compute(10.0f, 0.0f, DT);
    const float out3 = pid.compute(10.0f, 0.0f, DT);
    TEST_ASSERT_TRUE(out3 > out2);
    TEST_ASSERT_TRUE(out2 > out1);
}

void test_pid_integral_windup_clamp() {
    PIDController pid(0.0f, 1.0f, 0.0f, -100.0f, 100.0f);
    for (int i = 0; i < 1000; i++) pid.compute(10.0f, 0.0f, DT);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, pid.compute(10.0f, 0.0f, DT));
}

void test_pid_output_clamped() {
    PIDController pid(100.0f, 0.0f, 0.0f, -50.0f, 50.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, pid.compute(100.0f, 0.0f, DT));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -50.0f, pid.compute(-100.0f, 0.0f, DT));
}

void test_pid_reset_clears_state() {
    PIDController pid(0.0f, 1.0f, 1.0f, -1000.0f, 1000.0f);
    pid.compute(10.0f, 5.0f, DT);
    pid.compute(10.0f, 6.0f, DT);
    pid.reset();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, pid.compute(0.0f, 0.0f, DT));
}

void test_pid_symmetry() {
    PIDController a(2.0f, 0.1f, 5.0f, -1000.0f, 1000.0f);
    PIDController b(2.0f, 0.1f, 5.0f, -1000.0f, 1000.0f);
    const float pos = a.compute(0.0f, -10.0f, DT);
    const float neg = b.compute(0.0f, 10.0f, DT);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -pos, neg);
}

// F-12: reset alinhado à medida atual não gera pico de derivada no 1º ciclo
void test_pid_reset_with_measurement_avoids_derivative_kick() {
    PIDController pid(0.0f, 0.0f, 1.0f, -1000.0f, 1000.0f);
    pid.reset(0.5f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, pid.compute(0.0f, 0.5f, 0.002f));
}

void test_pid_derivative_filter_halves_first_step() {
    PIDController pid(0.0f, 0.0f, 1.0f, -1000.0f, 1000.0f);
    pid.setDerivativeFilter(0.5f);
    pid.reset(0.0f);
    // derivada bruta = -(0,01 - 0) / 0,01 = -1; filtrada com alpha 0,5 = -0,5
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -0.5f, pid.compute(0.0f, 0.01f, 0.01f));
}

// F-07: o termo D usa o dt informado em cada chamada
void test_pid_derivative_uses_given_dt() {
    PIDController fast(0.0f, 0.0f, 1.0f, -1000.0f, 1000.0f);
    PIDController slow(0.0f, 0.0f, 1.0f, -1000.0f, 1000.0f);
    fast.reset(0.0f);
    slow.reset(0.0f);
    const float outFast = fast.compute(0.0f, 0.1f, 0.002f);
    const float outSlow = slow.compute(0.0f, 0.1f, 0.004f);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 2.0f * outSlow, outFast);
}

void test_pid_nonpositive_dt_freezes_integral_and_derivative() {
    PIDController pid(1.0f, 1.0f, 1.0f, -100.0f, 100.0f);
    pid.reset(0.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, pid.compute(1.0f, 0.0f, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, pid.compute(1.0f, 0.0f, -0.01f));
}

void test_pid_non_finite_input_returns_zero() {
    PIDController pid(1.0f, 0.0f, 0.0f, -100.0f, 100.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, pid.compute(NAN, 0.0f, DT));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, pid.compute(0.0f, INFINITY, DT));
}

void test_pid_conditional_integration_stops_when_saturated() {
    PIDController pid(10.0f, 1.0f, 0.0f, -1.0f, 1.0f);
    pid.reset(0.0f);
    for (int i = 0; i < 100; i++) pid.compute(1.0f, 0.0f, DT);  // P = 10 satura
    // sem integral acumulada, a saída acompanha o P assim que o erro troca de sinal
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.5f, pid.compute(-0.05f, 0.0f, DT));
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
    RUN_TEST(test_pid_reset_with_measurement_avoids_derivative_kick);
    RUN_TEST(test_pid_derivative_filter_halves_first_step);
    RUN_TEST(test_pid_derivative_uses_given_dt);
    RUN_TEST(test_pid_nonpositive_dt_freezes_integral_and_derivative);
    RUN_TEST(test_pid_non_finite_input_returns_zero);
    RUN_TEST(test_pid_conditional_integration_stops_when_saturated);
    return UNITY_END();
}
