#include <unity.h>
#include "motors/DifferentialDrive.h"

void setUp() {}
void tearDown() {}

void test_zero_correction_equal_speeds() {
    DifferentialDrive drive(160, 60, 255);
    int left, right;
    drive.compute(0.0f, 160, left, right);
    TEST_ASSERT_EQUAL(left, right);
    TEST_ASSERT_EQUAL(160, left);
}

void test_positive_correction_turns_right() {
    // Correção positiva → robô precisa virar à direita
    // → motor direito mais lento (ou reverso), motor esquerdo mais rápido
    DifferentialDrive drive(160, 60, 255);
    int left, right;
    drive.compute(50.0f, 160, left, right);
    TEST_ASSERT_TRUE(left > right);
}

void test_negative_correction_turns_left() {
    DifferentialDrive drive(160, 60, 255);
    int left, right;
    drive.compute(-50.0f, 160, left, right);
    TEST_ASSERT_TRUE(right > left);
}

void test_clamps_to_pwm_range() {
    DifferentialDrive drive(160, 60, 255);
    int left, right;
    drive.compute(500.0f, 160, left, right);
    TEST_ASSERT_TRUE(left <= 255 && left >= -255);
    TEST_ASSERT_TRUE(right <= 255 && right >= -255);
}

void test_allows_reverse_in_tight_curve() {
    // Curva fechada: motor interno pode ir a negativo
    DifferentialDrive drive(160, 60, 255);
    int left, right;
    drive.compute(300.0f, 160, left, right);
    // Pelo menos um motor deve poder ser < 0 ou a diferença deve ser grande
    TEST_ASSERT_TRUE(left - right > 100);
}

void test_zero_base_spin_in_place() {
    DifferentialDrive drive(160, 60, 255);
    int left, right;
    drive.compute(100.0f, 0, left, right);
    TEST_ASSERT_TRUE(left > 0 && right < 0);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_zero_correction_equal_speeds);
    RUN_TEST(test_positive_correction_turns_right);
    RUN_TEST(test_negative_correction_turns_left);
    RUN_TEST(test_clamps_to_pwm_range);
    RUN_TEST(test_allows_reverse_in_tight_curve);
    RUN_TEST(test_zero_base_spin_in_place);
    return UNITY_END();
}
