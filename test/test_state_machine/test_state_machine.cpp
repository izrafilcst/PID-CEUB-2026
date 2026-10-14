#include <unity.h>
#include "strategy/RobotStateMachine.h"
#include "storage/NvsArbiter.h"
#include "sensors/ButtonInput.h"

static const SmInputs OK_IN{true, true};
static const SmInputs NO_CAL{false, true};
static const SmInputs LOW_BAT{true, false};
static const SmInputs ENC_OPEN{true, true, true};

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

// E-3: em ARMED/RUNNING o botão age no aperto (BTN_PRESS)
void test_state_machine_press_cancels_armed() {
    RobotStateMachine sm(true, 0);
    sm.handle(SmEvent::BTN_SHORT, OK_IN, 100);
    TEST_ASSERT_EQUAL(RobotState::ARMED, sm.state());
    TEST_ASSERT_TRUE(sm.handle(SmEvent::BTN_PRESS, OK_IN, 500));
    TEST_ASSERT_EQUAL(RobotState::READY, sm.state());
}

void test_state_machine_press_stops_running() {
    RobotStateMachine a = running();
    TEST_ASSERT_TRUE(a.handle(SmEvent::BTN_PRESS, OK_IN, 2000));
    TEST_ASSERT_EQUAL(RobotState::READY, a.state());
    TEST_ASSERT_FALSE(a.motorsEnabled());
    RobotStateMachine b = running();
    b.handle(SmEvent::CMD_STOP, OK_IN, 2000);
    TEST_ASSERT_EQUAL(RobotState::READY, b.state());
}

void test_state_machine_short_has_no_effect_while_active() {
    RobotStateMachine a = running();
    TEST_ASSERT_FALSE(a.handle(SmEvent::BTN_SHORT, OK_IN, 2000));
    TEST_ASSERT_EQUAL(RobotState::RUNNING, a.state());
    RobotStateMachine b(true, 0);
    b.handle(SmEvent::CMD_START, OK_IN, 10);
    TEST_ASSERT_FALSE(b.handle(SmEvent::BTN_SHORT, OK_IN, 20));
    TEST_ASSERT_EQUAL(RobotState::ARMED, b.state());
}

// E-3: o aperto que parou o carro consome o gesto (soltar não arma, segurar não calibra)
void test_state_machine_press_gesture_is_consumed() {
    RobotStateMachine sm = running();
    sm.handle(SmEvent::BTN_PRESS, OK_IN, 2000);
    TEST_ASSERT_EQUAL(RobotState::READY, sm.state());
    TEST_ASSERT_FALSE(sm.handle(SmEvent::BTN_SHORT, OK_IN, 2200));
    TEST_ASSERT_EQUAL(RobotState::READY, sm.state());
    TEST_ASSERT_FALSE(sm.handle(SmEvent::BTN_LONG, OK_IN, 3500));
    TEST_ASSERT_EQUAL(RobotState::READY, sm.state());
}

void test_state_machine_next_press_starts_new_gesture() {
    RobotStateMachine sm = running();
    sm.handle(SmEvent::BTN_PRESS, OK_IN, 2000);
    sm.handle(SmEvent::BTN_SHORT, OK_IN, 2200);  // descartado
    TEST_ASSERT_FALSE(sm.handle(SmEvent::BTN_PRESS, OK_IN, 4000));  // em READY não faz nada
    TEST_ASSERT_TRUE(sm.handle(SmEvent::BTN_SHORT, OK_IN, 4200));
    TEST_ASSERT_EQUAL(RobotState::ARMED, sm.state());
}

void test_state_machine_press_outside_active_has_no_effect() {
    RobotStateMachine idle(false, 0);
    TEST_ASSERT_FALSE(idle.handle(SmEvent::BTN_PRESS, NO_CAL, 10));
    TEST_ASSERT_EQUAL(RobotState::IDLE, idle.state());
    RobotStateMachine ready(true, 0);
    TEST_ASSERT_FALSE(ready.handle(SmEvent::BTN_PRESS, OK_IN, 10));
    TEST_ASSERT_TRUE(ready.handle(SmEvent::BTN_LONG, OK_IN, 1510));  // gesto não consumido
    TEST_ASSERT_EQUAL(RobotState::CALIBRATING, ready.state());
}

void test_state_machine_stop_on_stop_command() {
    RobotStateMachine a = running();
    a.handle(SmEvent::CMD_STOP, OK_IN, 2000);
    TEST_ASSERT_EQUAL(RobotState::READY, a.state());
    RobotStateMachine b(true, 0);
    b.handle(SmEvent::CMD_START, OK_IN, 10);
    b.handle(SmEvent::CMD_STOP, OK_IN, 20);
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

// A-WP1-02 de ponta a ponta: ButtonInput + máquina, botão segurado por 300, 900 e 2000 ms
static RobotState holdInRunning(uint32_t holdMs) {
    RobotStateMachine sm = running();
    ButtonInput b;
    bool sawCalibrating = false;
    for (uint32_t t = 2000; t < 2000 + holdMs + 200; t++) {
        const bool pressed = t < 2000 + holdMs;
        const ButtonEvent e = b.update(pressed, t);
        if (e == ButtonEvent::PRESS) sm.handle(SmEvent::BTN_PRESS, OK_IN, t);
        if (e == ButtonEvent::SHORT) sm.handle(SmEvent::BTN_SHORT, OK_IN, t);
        if (e == ButtonEvent::LONG) sm.handle(SmEvent::BTN_LONG, OK_IN, t);
        if (sm.state() == RobotState::CALIBRATING) sawCalibrating = true;
    }
    TEST_ASSERT_FALSE(sawCalibrating);
    return sm.state();
}

void test_state_machine_button_held_in_running_always_stops() {
    TEST_ASSERT_EQUAL(RobotState::READY, holdInRunning(300));
    TEST_ASSERT_EQUAL(RobotState::READY, holdInRunning(900));
    TEST_ASSERT_EQUAL(RobotState::READY, holdInRunning(2000));
}

// E-4: START recusado (fica em READY, sem erro) com a calibração de encoder aberta
void test_state_machine_start_refused_while_encoder_cal_open() {
    RobotStateMachine sm(true, 0);
    TEST_ASSERT_FALSE(sm.handle(SmEvent::BTN_SHORT, ENC_OPEN, 10));
    TEST_ASSERT_FALSE(sm.handle(SmEvent::CMD_START, ENC_OPEN, 20));
    TEST_ASSERT_EQUAL(RobotState::READY, sm.state());
    TEST_ASSERT_EQUAL(ErrorCode::NONE, sm.error());
    TEST_ASSERT_TRUE(sm.handle(SmEvent::CMD_START, OK_IN, 30));
    TEST_ASSERT_EQUAL(RobotState::ARMED, sm.state());
}

// E-4: encoder sem leitura no modo cascata → ERROR(ENC) = 5, motores param
void test_state_machine_encoder_fault_stops_with_code_5() {
    TEST_ASSERT_EQUAL_UINT8(5, static_cast<uint8_t>(ErrorCode::ENC));
    RobotStateMachine sm = running();
    TEST_ASSERT_TRUE(sm.handle(SmEvent::ENCODER_FAULT, OK_IN, 2000));
    TEST_ASSERT_EQUAL(RobotState::ERROR, sm.state());
    TEST_ASSERT_EQUAL(ErrorCode::ENC, sm.error());
    TEST_ASSERT_FALSE(sm.motorsEnabled());
    RobotStateMachine ready(true, 0);
    TEST_ASSERT_FALSE(ready.handle(SmEvent::ENCODER_FAULT, OK_IN, 10));
    TEST_ASSERT_EQUAL(RobotState::READY, ready.state());
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
    RUN_TEST(test_state_machine_press_cancels_armed);
    RUN_TEST(test_state_machine_press_stops_running);
    RUN_TEST(test_state_machine_short_has_no_effect_while_active);
    RUN_TEST(test_state_machine_press_gesture_is_consumed);
    RUN_TEST(test_state_machine_next_press_starts_new_gesture);
    RUN_TEST(test_state_machine_press_outside_active_has_no_effect);
    RUN_TEST(test_state_machine_stop_on_stop_command);
    RUN_TEST(test_state_machine_button_held_in_running_always_stops);
    RUN_TEST(test_state_machine_start_refused_while_encoder_cal_open);
    RUN_TEST(test_state_machine_encoder_fault_stops_with_code_5);
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
