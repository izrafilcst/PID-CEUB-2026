// test/test_ble_tuner/test_ble_tuner.cpp
#include <unity.h>
#include "comm/BLETuner.h"

// Em NATIVE_BUILD, _onWrite() não existe (é privado e parte do bloco ESP32).
// Estes testes focam no contrato de consumers — em native build o estado
// começa com flags = false e structs com defaults. Smoke tests apenas.

static BLETuner* t = nullptr;
void setUp()    { t = new BLETuner(); }
void tearDown() { delete t; t = nullptr; }

void test_ble_default_velocity_pid_returns_zero_flag() {
    TEST_ASSERT_FALSE(t->hasNewVelocityPID());
}

void test_ble_default_rpm_returns_zero_flag() {
    TEST_ASSERT_FALSE(t->hasNewRpm());
}

void test_ble_default_calibration_returns_none() {
    TEST_ASSERT_FALSE(t->hasNewCalibration());
    CalibrationCmd c = t->getCalibration();
    TEST_ASSERT_TRUE(c.op == CalibrationCmd::Op::NONE);
}

void test_ble_velocity_pid_defaults_are_safe() {
    VelocityPIDParams p = t->getVelocityPID();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.8f,  p.kp);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f,  p.ki);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.05f, p.kd);
}

void test_ble_rpm_defaults_are_safe() {
    RpmParams r = t->getRpm();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1200.0f, r.maxRpm);
    TEST_ASSERT_FLOAT_WITHIN(0.001f,  600.0f, r.baseRpm);
}

void test_ble_calibration_default_rotations_zero() {
    CalibrationCmd c = t->getCalibration();
    TEST_ASSERT_EQUAL_INT(0, c.rotations);
}

void test_ble_calibration_default_left_side_true() {
    CalibrationCmd c = t->getCalibration();
    TEST_ASSERT_TRUE(c.leftSide);
}

void test_ble_existing_pid_consumer_still_works() {
    // Garantia de não-regressão
    TEST_ASSERT_FALSE(t->hasNewPID());
    PIDParams p = t->getPID();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f,  p.kp);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.0f, p.kd);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_ble_default_velocity_pid_returns_zero_flag);
    RUN_TEST(test_ble_default_rpm_returns_zero_flag);
    RUN_TEST(test_ble_default_calibration_returns_none);
    RUN_TEST(test_ble_velocity_pid_defaults_are_safe);
    RUN_TEST(test_ble_rpm_defaults_are_safe);
    RUN_TEST(test_ble_calibration_default_rotations_zero);
    RUN_TEST(test_ble_calibration_default_left_side_true);
    RUN_TEST(test_ble_existing_pid_consumer_still_works);
    return UNITY_END();
}
