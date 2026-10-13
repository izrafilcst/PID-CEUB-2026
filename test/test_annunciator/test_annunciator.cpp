#include <unity.h>
#include "comm/Annunciator.h"

void setUp() {}
void tearDown() {}

void test_annunciator_idle_blinks_1hz() {
    Annunciator a;
    a.onState(RobotState::IDLE, ErrorCode::NONE, 0);
    TEST_ASSERT_TRUE(a.update(100).led);
    TEST_ASSERT_FALSE(a.update(600).led);
    TEST_ASSERT_TRUE(a.update(1100).led);
}

void test_annunciator_ready_led_on_running_led_off() {
    Annunciator a;
    a.onState(RobotState::READY, ErrorCode::NONE, 0);
    TEST_ASSERT_TRUE(a.update(5000).led);
    a.onState(RobotState::RUNNING, ErrorCode::NONE, 6000);
    TEST_ASSERT_FALSE(a.update(6500).led);
    TEST_ASSERT_FALSE(a.update(6500).buzzer);
}

void test_annunciator_calibrating_blinks_5hz_and_beeps_on_entry() {
    Annunciator a;
    a.onState(RobotState::CALIBRATING, ErrorCode::NONE, 0);
    TEST_ASSERT_TRUE(a.update(50).led);
    TEST_ASSERT_FALSE(a.update(150).led);
    TEST_ASSERT_TRUE(a.update(10).buzzer);
    TEST_ASSERT_FALSE(a.update(100).buzzer);
}

void test_annunciator_armed_beeps_every_250ms() {
    Annunciator a;
    a.onState(RobotState::ARMED, ErrorCode::NONE, 1000);
    TEST_ASSERT_TRUE(a.update(1010).buzzer);
    TEST_ASSERT_FALSE(a.update(1100).buzzer);
    TEST_ASSERT_TRUE(a.update(1260).buzzer);
}

void test_annunciator_error_blinks_code_count() {
    Annunciator a;
    a.onState(RobotState::ERROR, ErrorCode::LINE, 0);  // código 2: período 2×400 + 1200
    TEST_ASSERT_TRUE(a.update(100).led);
    TEST_ASSERT_FALSE(a.update(300).led);
    TEST_ASSERT_TRUE(a.update(500).led);
    TEST_ASSERT_FALSE(a.update(900).led);
    TEST_ASSERT_TRUE(a.update(2100).led);  // novo ciclo
}

void test_annunciator_error_beeps_code_count_on_entry() {
    Annunciator a;
    a.onState(RobotState::ERROR, ErrorCode::BAT, 0);  // 3 bipes de 120 ms, pausa 120 ms
    int rising = 0;
    bool prev = false;
    for (uint32_t t = 0; t < 2000; t += 5) {
        const bool now = a.update(t).buzzer;
        if (now && !prev) rising++;
        prev = now;
    }
    TEST_ASSERT_EQUAL_INT(3, rising);
}

void test_annunciator_explicit_beep_sequence() {
    Annunciator a;
    a.onState(RobotState::RUNNING, ErrorCode::NONE, 0);
    a.beep(2, 50, 50, 1000);
    TEST_ASSERT_TRUE(a.update(1020).buzzer);
    TEST_ASSERT_FALSE(a.update(1070).buzzer);
    TEST_ASSERT_TRUE(a.update(1120).buzzer);
    TEST_ASSERT_FALSE(a.update(1220).buzzer);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_annunciator_idle_blinks_1hz);
    RUN_TEST(test_annunciator_ready_led_on_running_led_off);
    RUN_TEST(test_annunciator_calibrating_blinks_5hz_and_beeps_on_entry);
    RUN_TEST(test_annunciator_armed_beeps_every_250ms);
    RUN_TEST(test_annunciator_error_blinks_code_count);
    RUN_TEST(test_annunciator_error_beeps_code_count_on_entry);
    RUN_TEST(test_annunciator_explicit_beep_sequence);
    return UNITY_END();
}
