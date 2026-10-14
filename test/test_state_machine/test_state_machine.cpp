#include <unity.h>
#include "strategy/RobotStateMachine.h"
#include "storage/NvsArbiter.h"

static const SmInputs OK_IN{true, true};
static const SmInputs NO_CAL{false, true};
static const SmInputs LOW_BAT{true, false};

void setUp() {}
void tearDown() {}

static RobotStateMachine running() {
    RobotStateMachine sm(true, 0);
    sm.handle(SmEvent::BTN_SHORT, OK_IN, 10);
    sm.tick(1010, 1000);
    return sm;
}

void test_state_machine_boot_with_calibration_is_ready() {
    RobotStateMachine sm(true);
    TEST_ASSERT_EQUAL(RobotState::READY, sm.state());
}

void test_state_machine_boot_without_calibration_is_idle() {
    RobotStateMachine sm(false);
    TEST_ASSERT_EQUAL(RobotState::IDLE, sm.state());
}

void test_state_machine_idle_long_press_calibrates() {
    RobotStateMachine sm(false);
    TEST_ASSERT_TRUE(sm.handle(SmEvent::BTN_LONG, NO_CAL, 100));
    TEST_ASSERT_EQUAL(RobotState::CALIBRATING, sm.state());
    TEST_ASSERT_FALSE(sm.calibrationWindowOver(5099));
    TEST_ASSERT_TRUE(sm.calibrationWindowOver(5100));
}

void test_state_machine_idle_short_press_needs_valid_calibration() {
    RobotStateMachine sm(false);
    TEST_ASSERT_FALSE(sm.handle(SmEvent::BTN_SHORT, NO_CAL, 0));
    TEST_ASSERT_EQUAL(RobotState::IDLE, sm.state());
    TEST_ASSERT_TRUE(sm.handle(SmEvent::BTN_SHORT, OK_IN, 0));
    TEST_ASSERT_EQUAL(RobotState::READY, sm.state());
}

void test_state_machine_calibration_result() {
    RobotStateMachine sm(false);
    sm.handle(SmEvent::CMD_SENSOR_CAL, NO_CAL, 0);
    sm.handle(SmEvent::CAL_DONE_OK, OK_IN, 5000);
    TEST_ASSERT_EQUAL(RobotState::READY, sm.state());
    sm.handle(SmEvent::BTN_LONG, OK_IN, 6000);
    sm.handle(SmEvent::CAL_DONE_FAIL, NO_CAL, 11000);
    TEST_ASSERT_EQUAL(RobotState::ERROR, sm.state());
    TEST_ASSERT_EQUAL(ErrorCode::CAL, sm.error());
}

void test_state_machine_start_arms_then_runs_after_delay() {
    RobotStateMachine sm(true, 0);
    sm.handle(SmEvent::CMD_START, OK_IN, 100);
    TEST_ASSERT_EQUAL(RobotState::ARMED, sm.state());
    TEST_ASSERT_FALSE(sm.motorsEnabled());
    TEST_ASSERT_TRUE(sm.isActive());
    TEST_ASSERT_FALSE(sm.tick(1099, 1000));
    TEST_ASSERT_TRUE(sm.tick(1100, 1000));
    TEST_ASSERT_EQUAL(RobotState::RUNNING, sm.state());
    TEST_ASSERT_TRUE(sm.motorsEnabled());
}

// Review Focus 2
void test_state_machine_start_with_low_battery_errors() {
    RobotStateMachine sm(true, 0);
    sm.handle(SmEvent::BTN_SHORT, LOW_BAT, 100);
    TEST_ASSERT_EQUAL(RobotState::ERROR, sm.state());
    TEST_ASSERT_EQUAL(ErrorCode::BAT, sm.error());
}

void test_state_machine_armed_can_be_cancelled() {
    RobotStateMachine sm(true, 0);
    sm.handle(SmEvent::BTN_SHORT, OK_IN, 100);
    sm.handle(SmEvent::BTN_SHORT, OK_IN, 500);
    TEST_ASSERT_EQUAL(RobotState::READY, sm.state());
}

void test_state_machine_running_stops_on_short_press_or_stop() {
    RobotStateMachine a = running();
    a.handle(SmEvent::BTN_SHORT, OK_IN, 2000);
    TEST_ASSERT_EQUAL(RobotState::READY, a.state());
    RobotStateMachine b = running();
    b.handle(SmEvent::CMD_STOP, OK_IN, 2000);
    TEST_ASSERT_EQUAL(RobotState::READY, b.state());
}

void test_state_machine_running_faults() {
    RobotStateMachine a = running();
    a.handle(SmEvent::LINE_LOST_TIMEOUT, OK_IN, 2000);
    TEST_ASSERT_EQUAL(ErrorCode::LINE, a.error());
    RobotStateMachine b = running();
    b.handle(SmEvent::BATTERY_LOW, OK_IN, 2000);
    TEST_ASSERT_EQUAL(ErrorCode::BAT, b.error());
    RobotStateMachine c = running();
    c.handle(SmEvent::LOOP_OVERRUN, OK_IN, 2000);
    TEST_ASSERT_EQUAL(ErrorCode::OVR, c.error());
    TEST_ASSERT_FALSE(c.motorsEnabled());
}

void test_state_machine_faults_ignored_outside_running() {
    RobotStateMachine sm(true, 0);
    TEST_ASSERT_FALSE(sm.handle(SmEvent::LINE_LOST_TIMEOUT, OK_IN, 10));
    TEST_ASSERT_FALSE(sm.handle(SmEvent::LOOP_OVERRUN, OK_IN, 10));
    TEST_ASSERT_EQUAL(RobotState::READY, sm.state());
}

void test_state_machine_error_short_press_recovers() {
    RobotStateMachine a = running();
    a.handle(SmEvent::LINE_LOST_TIMEOUT, OK_IN, 2000);
    a.handle(SmEvent::BTN_SHORT, OK_IN, 3000);
    TEST_ASSERT_EQUAL(RobotState::READY, a.state());
    TEST_ASSERT_EQUAL(ErrorCode::NONE, a.error());
    RobotStateMachine b = running();
    b.handle(SmEvent::LINE_LOST_TIMEOUT, OK_IN, 2000);
    b.handle(SmEvent::BTN_SHORT, NO_CAL, 3000);
    TEST_ASSERT_EQUAL(RobotState::IDLE, b.state());
}

void test_state_machine_reset_from_anywhere() {
    RobotStateMachine sm = running();
    TEST_ASSERT_TRUE(sm.handle(SmEvent::CMD_RESET, OK_IN, 2000));
    TEST_ASSERT_EQUAL(RobotState::IDLE, sm.state());
}

void test_state_machine_ready_long_press_recalibrates() {
    RobotStateMachine sm(true, 0);
    sm.handle(SmEvent::BTN_LONG, OK_IN, 10);
    TEST_ASSERT_EQUAL(RobotState::CALIBRATING, sm.state());
}

void test_state_machine_is_active_state_helper() {
    TEST_ASSERT_TRUE(isActiveState(static_cast<uint8_t>(RobotState::ARMED)));
    TEST_ASSERT_TRUE(isActiveState(static_cast<uint8_t>(RobotState::RUNNING)));
    TEST_ASSERT_FALSE(isActiveState(static_cast<uint8_t>(RobotState::READY)));
}

// A-WP1-05: com a NVS ocupada pela comm, a control não arma
void test_state_machine_does_not_arm_while_nvs_busy() {
    NvsArbiter nvs;
    RobotStateMachine sm(true, 0, &nvs);
    TEST_ASSERT_TRUE(nvs.tryBeginWrite());
    TEST_ASSERT_FALSE(sm.handle(SmEvent::BTN_SHORT, OK_IN, 10));
    TEST_ASSERT_FALSE(sm.handle(SmEvent::CMD_START, OK_IN, 20));
    TEST_ASSERT_EQUAL(RobotState::READY, sm.state());
    nvs.endWrite();
    TEST_ASSERT_TRUE(sm.handle(SmEvent::CMD_START, OK_IN, 30));
    TEST_ASSERT_EQUAL(RobotState::ARMED, sm.state());
    TEST_ASSERT_FALSE(nvs.tryBeginWrite());  // armado: a comm não escreve
}

void test_state_machine_leaving_active_releases_nvs() {
    NvsArbiter nvs;
    RobotStateMachine a(true, 0, &nvs);
    a.handle(SmEvent::CMD_START, OK_IN, 10);
    a.tick(1010, 1000);
    TEST_ASSERT_EQUAL(RobotState::RUNNING, a.state());
    TEST_ASSERT_TRUE(nvs.armed());
    a.handle(SmEvent::LINE_LOST_TIMEOUT, OK_IN, 2000);
    TEST_ASSERT_FALSE(nvs.armed());
    TEST_ASSERT_TRUE(nvs.tryBeginWrite());
    nvs.endWrite();

    RobotStateMachine b(true, 0, &nvs);
    b.handle(SmEvent::CMD_START, OK_IN, 10);
    b.handle(SmEvent::CMD_STOP, OK_IN, 20);  // cancela em ARMED
    TEST_ASSERT_FALSE(nvs.armed());

    RobotStateMachine c(true, 0, &nvs);
    c.handle(SmEvent::CMD_START, OK_IN, 10);
    c.handle(SmEvent::CMD_RESET, OK_IN, 20);
    TEST_ASSERT_FALSE(nvs.armed());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_state_machine_boot_with_calibration_is_ready);
    RUN_TEST(test_state_machine_boot_without_calibration_is_idle);
    RUN_TEST(test_state_machine_idle_long_press_calibrates);
    RUN_TEST(test_state_machine_idle_short_press_needs_valid_calibration);
    RUN_TEST(test_state_machine_calibration_result);
    RUN_TEST(test_state_machine_start_arms_then_runs_after_delay);
    RUN_TEST(test_state_machine_start_with_low_battery_errors);
    RUN_TEST(test_state_machine_armed_can_be_cancelled);
    RUN_TEST(test_state_machine_running_stops_on_short_press_or_stop);
    RUN_TEST(test_state_machine_running_faults);
    RUN_TEST(test_state_machine_faults_ignored_outside_running);
    RUN_TEST(test_state_machine_error_short_press_recovers);
    RUN_TEST(test_state_machine_reset_from_anywhere);
    RUN_TEST(test_state_machine_ready_long_press_recalibrates);
    RUN_TEST(test_state_machine_is_active_state_helper);
    RUN_TEST(test_state_machine_does_not_arm_while_nvs_busy);
    RUN_TEST(test_state_machine_leaving_active_releases_nvs);
    return UNITY_END();
}
