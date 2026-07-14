// test/test_cascade/test_cascade.cpp
#include <unity.h>
#include "control/CascadeController.h"
#include "control/PIDController.h"

// PIDs internos para os testes — Kp=1, Ki=0, Kd=0 → erro direto vira PWM
// (controlador puramente proporcional simplifica verificações)
static PIDController* innerL = nullptr;
static PIDController* innerR = nullptr;
static CascadeController* cc = nullptr;

void setUp() {
    innerL = new PIDController(1.0f, 0.0f, 0.0f, -1000.0f, 1000.0f, 0.01f);
    innerR = new PIDController(1.0f, 0.0f, 0.0f, -1000.0f, 1000.0f, 0.01f);
    // maxRpm=1000, maxPwm=1023 (10-bit LEDC do ESP32)
    cc = new CascadeController(*innerL, *innerR, 1000.0f, 1023);
}

void tearDown() {
    delete cc;     cc = nullptr;
    delete innerR; innerR = nullptr;
    delete innerL; innerL = nullptr;
}

// ─── 1. Linha reta (correction=0) → setpoints iguais ─────────────────────
void test_cascade_straight_line_equal_setpoints() {
    int pwmL = 0, pwmR = 0;
    // correction=0, base=500, ambos motores parados (rpm=0) → erro=500 cada
    cc->compute(0.0f, 500.0f, 0.0f, 0.0f, pwmL, pwmR);
    TEST_ASSERT_EQUAL_INT(pwmL, pwmR);
    TEST_ASSERT_EQUAL_INT(500, pwmL);
}

// ─── 2. Correção positiva → motor L acelera, motor R desacelera ──────────
void test_cascade_positive_correction_differentiates_motors() {
    int pwmL = 0, pwmR = 0;
    // correction=+100, base=500 → setL=600, setR=400 (ambos rpm=0)
    cc->compute(100.0f, 500.0f, 0.0f, 0.0f, pwmL, pwmR);
    TEST_ASSERT_TRUE(pwmL > pwmR);
    TEST_ASSERT_EQUAL_INT(600, pwmL);
    TEST_ASSERT_EQUAL_INT(400, pwmR);
}

// ─── 3. Motor atingiu setpoint → PWM cai a zero ──────────────────────────
void test_cascade_motor_at_setpoint_outputs_zero() {
    int pwmL = 0, pwmR = 0;
    // setpoint=500, actualRpm=500 → erro=0 → pwm=0
    cc->compute(0.0f, 500.0f, 500.0f, 500.0f, pwmL, pwmR);
    TEST_ASSERT_EQUAL_INT(0, pwmL);
    TEST_ASSERT_EQUAL_INT(0, pwmR);
}

// ─── 4. Motor mais lento que setpoint → PWM positivo ─────────────────────
void test_cascade_slow_motor_gets_positive_pwm() {
    int pwmL = 0, pwmR = 0;
    // setpoint=500, actual=200 → erro=300 → pwm=300
    cc->compute(0.0f, 500.0f, 200.0f, 500.0f, pwmL, pwmR);
    TEST_ASSERT_EQUAL_INT(300, pwmL);
    TEST_ASSERT_EQUAL_INT(0, pwmR);
}

// ─── 5. Setpoint > maxRpm → clamp em maxRpm ──────────────────────────────
void test_cascade_setpoint_clamped_to_max_rpm() {
    int pwmL = 0, pwmR = 0;
    // base=1500 (> maxRpm=1000), correction=0 → setpoint clamped em 1000
    cc->compute(0.0f, 1500.0f, 0.0f, 0.0f, pwmL, pwmR);
    TEST_ASSERT_EQUAL_INT(1000, pwmL);
    TEST_ASSERT_EQUAL_INT(1000, pwmR);
}

// ─── 6. Setpoint negativo → motor em reverso ─────────────────────────────
void test_cascade_negative_setpoint_drives_reverse() {
    int pwmL = 0, pwmR = 0;
    // base=300, correction=+500 → setL=800, setR=-200 (R inverte)
    cc->compute(500.0f, 300.0f, 0.0f, 0.0f, pwmL, pwmR);
    TEST_ASSERT_EQUAL_INT(800, pwmL);
    TEST_ASSERT_EQUAL_INT(-200, pwmR);
}

// ─── 7. PWM saturado em ±maxPwm quando erro extremo ──────────────────────
void test_cascade_pwm_clamped_to_max() {
    // PID interno com Kp=10 — erro=200 → output 2000, clamp em 1023
    PIDController bigL(10.0f, 0.0f, 0.0f, -1023.0f, 1023.0f, 0.01f);
    PIDController bigR(10.0f, 0.0f, 0.0f, -1023.0f, 1023.0f, 0.01f);
    CascadeController bigCC(bigL, bigR, 1000.0f, 1023);
    int pwmL = 0, pwmR = 0;
    bigCC.compute(0.0f, 200.0f, 0.0f, 0.0f, pwmL, pwmR);
    TEST_ASSERT_EQUAL_INT(1023, pwmL);
    TEST_ASSERT_EQUAL_INT(1023, pwmR);
}

// ─── 8. setInnerGains atualiza ambos PIDs internos ───────────────────────
void test_cascade_set_inner_gains_updates_both_pids() {
    cc->setInnerGains(5.0f, 0.1f, 0.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, innerL->getKp());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, innerR->getKp());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f, innerL->getKi());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, innerR->getKd());
}

// ─── 9. compute() usa o dt fornecido no termo derivativo ─────────────────
void test_cascade_uses_provided_dt_for_derivative() {
    // PIDs internos só com Kd (derivada no processo). Kp=Ki=0.
    PIDController dL(0.0f, 0.0f, 0.001f, -1023.0f, 1023.0f, 0.01f);
    PIDController dR(0.0f, 0.0f, 0.001f, -1023.0f, 1023.0f, 0.01f);
    CascadeController dcc(dL, dR, 1000.0f, 1023);
    int pwmL = 0, pwmR = 0;

    // 1º compute estabelece _prevMeasurement (=0). dt grande.
    dcc.compute(0.0f, 0.0f, 0.0f, 0.0f, pwmL, pwmR, 0.01f);
    // measurement salta p/ 100: derivada = -(100-0)/dt; Kd=0.001
    dcc.compute(0.0f, 0.0f, 100.0f, 100.0f, pwmL, pwmR, 0.01f);
    int atDtBig = pwmL;   // -0.001 * 100/0.01 = -10

    dL.reset(); dR.reset();
    dcc.compute(0.0f, 0.0f, 0.0f, 0.0f, pwmL, pwmR, 0.002f);
    dcc.compute(0.0f, 0.0f, 100.0f, 100.0f, pwmL, pwmR, 0.002f);
    int atDtSmall = pwmL; // -0.001 * 100/0.002 = -50

    TEST_ASSERT_EQUAL_INT(-10, atDtBig);
    TEST_ASSERT_EQUAL_INT(-50, atDtSmall);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_cascade_straight_line_equal_setpoints);
    RUN_TEST(test_cascade_positive_correction_differentiates_motors);
    RUN_TEST(test_cascade_motor_at_setpoint_outputs_zero);
    RUN_TEST(test_cascade_slow_motor_gets_positive_pwm);
    RUN_TEST(test_cascade_setpoint_clamped_to_max_rpm);
    RUN_TEST(test_cascade_negative_setpoint_drives_reverse);
    RUN_TEST(test_cascade_pwm_clamped_to_max);
    RUN_TEST(test_cascade_set_inner_gains_updates_both_pids);
    RUN_TEST(test_cascade_uses_provided_dt_for_derivative);
    return UNITY_END();
}
