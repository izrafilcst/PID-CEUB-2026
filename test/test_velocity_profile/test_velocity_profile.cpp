#include <unity.h>
#include "control/VelocityProfile.h"

void setUp() {}
void tearDown() {}

// Parâmetros: min=60, base=160, max=230, threshold=0.6, curvatureWeight=0.4

void test_velocity_profile_zero_position_returns_max_speed() {
    VelocityProfile vp(60, 160, 230, 0.6f);
    for (int i = 0; i < 4; i++) {
        vp.update(0.0f, static_cast<uint32_t>(i * 50));
    }
    TEST_ASSERT_EQUAL(230, vp.computeSpeed());
}

void test_velocity_profile_large_error_returns_min_speed() {
    VelocityProfile vp(60, 160, 230, 0.6f);
    for (int i = 0; i < 4; i++) {
        vp.update(3500.0f, static_cast<uint32_t>(i * 50));  // posição máxima
    }
    TEST_ASSERT_EQUAL(60, vp.computeSpeed());
}

void test_velocity_profile_curvature_reduces_max_speed() {
    // Robô na linha (posição pequena) mas posição cresce rapidamente → curva à frente
    // Resultado: velocidade menor que maxSpeed mesmo com pouco erro atual
    VelocityProfile vp(60, 160, 230, 0.6f);
    vp.update(0.0f,   0u);
    vp.update(100.0f, 50u);
    vp.update(200.0f, 100u);
    vp.update(300.0f, 150u);  // erro = 300/3500 = 0.086 → errorSpeed ≈ 206
    int speed = vp.computeSpeed();
    // Curvatura reduz effectiveMax abaixo de 230
    TEST_ASSERT_LESS_THAN(230, speed);
    TEST_ASSERT_GREATER_OR_EQUAL(60, speed);
}

void test_velocity_profile_buffer_wraps_without_crash() {
    VelocityProfile vp(60, 160, 230, 0.6f);
    // Alimentar mais que BUFFER_SIZE amostras
    for (int i = 0; i < 12; i++) {
        vp.update(0.0f, static_cast<uint32_t>(i * 20));
    }
    int speed = vp.computeSpeed();
    TEST_ASSERT_GREATER_OR_EQUAL(60, speed);
    TEST_ASSERT_LESS_OR_EQUAL(230, speed);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_velocity_profile_zero_position_returns_max_speed);
    RUN_TEST(test_velocity_profile_large_error_returns_min_speed);
    RUN_TEST(test_velocity_profile_curvature_reduces_max_speed);
    RUN_TEST(test_velocity_profile_buffer_wraps_without_crash);
    return UNITY_END();
}
