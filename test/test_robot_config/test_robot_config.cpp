#include <unity.h>
#include <cmath>
#include <cstring>
#include "config/RobotConfig.h"

static RobotConfig cfg;

void setUp() { cfg = robotcfg::factoryConfig(); }
void tearDown() {}

void test_robot_config_factory_hardware_matches_spec() {
    const HardwareConfig hw = robotcfg::factoryHardware();
    TEST_ASSERT_EQUAL_UINT8(0, hw.invL);
    TEST_ASSERT_EQUAL_UINT8(0, hw.swap);
    TEST_ASSERT_EQUAL_UINT8(1, hw.pol);
    TEST_ASSERT_EQUAL_UINT8(0, hw.sinv);
    TEST_ASSERT_EQUAL_UINT8(1, hw.chem);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 6.4f, hw.vcut);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, hw.vbk);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 600.0f, hw.pprL);
}

void test_robot_config_factory_profiles_differ_only_in_vmax() {
    const TuningProfile p0 = robotcfg::factoryProfile(0);
    const TuningProfile p1 = robotcfg::factoryProfile(1);
    const TuningProfile p2 = robotcfg::factoryProfile(2);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.35f, p0.vmax);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.50f, p1.vmax);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.70f, p2.vmax);
    TEST_ASSERT_EQUAL_UINT8(0, p2.mode);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, p2.kp);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.03f, p2.kd);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, p2.dlp);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.25f, p2.vmin);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 700.0f, p2.rpmMax);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 7.4f, p2.vref);
    TEST_ASSERT_EQUAL_UINT16(300, p2.lostMs);
    TEST_ASSERT_EQUAL_UINT16(1000, p2.startMs);
}

void test_robot_config_factory_values_are_inside_ranges() {
    size_t n = 0;
    const robotcfg::KeyInfo* keys = robotcfg::keyTable(n);
    TEST_ASSERT_EQUAL_UINT32(30, n);
    for (uint8_t p = 0; p < PROFILE_COUNT; p++) {
        RobotConfig c = robotcfg::factoryConfig();
        c.tune = robotcfg::factoryProfile(p);
        for (size_t i = 0; i < n; i++) {
            float v = 0.0f;
            TEST_ASSERT_TRUE(robotcfg::get(c, keys[i].key, v));
            TEST_ASSERT_TRUE_MESSAGE(v >= keys[i].min && v <= keys[i].max, keys[i].key);
        }
        TEST_ASSERT_TRUE(robotcfg::validate(c));
    }
}

void test_robot_config_keys_fit_command_buffer_and_have_known_group() {
    size_t n = 0;
    const robotcfg::KeyInfo* keys = robotcfg::keyTable(n);
    for (size_t i = 0; i < n; i++) {
        TEST_ASSERT_TRUE_MESSAGE(std::strlen(keys[i].key) <= 7, keys[i].key);
        bool known = false;
        for (size_t g = 0; g < robotcfg::GROUP_COUNT; g++) {
            if (std::strcmp(keys[i].group, robotcfg::GROUPS[g]) == 0) known = true;
        }
        TEST_ASSERT_TRUE_MESSAGE(known, keys[i].key);
    }
}

void test_robot_config_set_applies_valid_value() {
    TEST_ASSERT_EQUAL(SetResult::OK, robotcfg::set(cfg, "kp", 2.5f, false));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 2.5f, cfg.tune.kp);
    TEST_ASSERT_EQUAL(SetResult::OK, robotcfg::set(cfg, "invR", 1.0f, false));
    TEST_ASSERT_EQUAL_UINT8(1, cfg.hw.invR);
    TEST_ASSERT_EQUAL(SetResult::OK, robotcfg::set(cfg, "lostMs", 500.0f, false));
    TEST_ASSERT_EQUAL_UINT16(500, cfg.tune.lostMs);
}

void test_robot_config_unknown_key() {
    TEST_ASSERT_EQUAL(SetResult::UNKNOWN_KEY, robotcfg::set(cfg, "kq", 1.0f, false));
    TEST_ASSERT_EQUAL(SetResult::UNKNOWN_KEY, robotcfg::set(cfg, nullptr, 1.0f, false));
}

void test_robot_config_out_of_range_keeps_value() {
    TEST_ASSERT_EQUAL(SetResult::OUT_OF_RANGE, robotcfg::set(cfg, "kp", 20.1f, false));
    TEST_ASSERT_EQUAL(SetResult::OUT_OF_RANGE, robotcfg::set(cfg, "vcut", 5.5f, false));
    TEST_ASSERT_EQUAL(SetResult::OUT_OF_RANGE, robotcfg::set(cfg, "mode", 2.0f, false));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, cfg.tune.kp);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 6.4f, cfg.hw.vcut);
}

// Review Focus 1
void test_robot_config_rejects_non_finite() {
    TEST_ASSERT_EQUAL(SetResult::OUT_OF_RANGE, robotcfg::set(cfg, "kd", NAN, false));
    TEST_ASSERT_EQUAL(SetResult::OUT_OF_RANGE, robotcfg::set(cfg, "kd", INFINITY, false));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.03f, cfg.tune.kd);
}

void test_robot_config_integer_fields_round() {
    TEST_ASSERT_EQUAL(SetResult::OK, robotcfg::set(cfg, "lostMs", 299.6f, false));
    TEST_ASSERT_EQUAL_UINT16(300, cfg.tune.lostMs);
}

void test_robot_config_hardware_and_mode_locked_while_running() {
    TEST_ASSERT_EQUAL(SetResult::LOCKED_WHILE_RUNNING, robotcfg::set(cfg, "invL", 1, true));
    TEST_ASSERT_EQUAL(SetResult::LOCKED_WHILE_RUNNING, robotcfg::set(cfg, "mode", 1, true));
    TEST_ASSERT_EQUAL(SetResult::OK, robotcfg::set(cfg, "kp", 1.5f, true));
}

void test_robot_config_vmin_cannot_exceed_vmax() {
    TEST_ASSERT_EQUAL(SetResult::OUT_OF_RANGE, robotcfg::set(cfg, "vmin", 0.4f, false));
    TEST_ASSERT_EQUAL(SetResult::OUT_OF_RANGE, robotcfg::set(cfg, "vmax", 0.2f, false));
    TEST_ASSERT_EQUAL(SetResult::OK, robotcfg::set(cfg, "vmin", 0.35f, false));
}

void test_robot_config_get_unknown_returns_false() {
    float v = -1.0f;
    TEST_ASSERT_FALSE(robotcfg::get(cfg, "nope", v));
}

void test_robot_config_validate_detects_bad_values() {
    cfg.tune.kp = 99.0f;
    TEST_ASSERT_FALSE(robotcfg::validate(cfg));
    cfg = robotcfg::factoryConfig();
    cfg.tune.vmin = 0.9f;
    TEST_ASSERT_FALSE(robotcfg::validate(cfg));
    cfg = robotcfg::factoryConfig();
    cfg.profile = 3;
    TEST_ASSERT_FALSE(robotcfg::validate(cfg));
}

// E-1 (F-33): modo competição, chave de hardware desligada de fábrica
void test_robot_config_lock_defaults_off_and_is_hardware_key() {
    TEST_ASSERT_EQUAL_UINT8(0, robotcfg::factoryHardware().lock);
    const robotcfg::KeyInfo* k = robotcfg::findKey("lock");
    TEST_ASSERT_NOT_NULL(k);
    TEST_ASSERT_TRUE(k->hardware);
    TEST_ASSERT_EQUAL_STRING("hw", k->group);
    TEST_ASSERT_EQUAL(SetResult::OK, robotcfg::set(cfg, "lock", 1.0f, false));
    TEST_ASSERT_EQUAL_UINT8(1, cfg.hw.lock);
    TEST_ASSERT_EQUAL(SetResult::OUT_OF_RANGE, robotcfg::set(cfg, "lock", 2.0f, false));
    TEST_ASSERT_EQUAL(SetResult::LOCKED_WHILE_RUNNING, robotcfg::set(cfg, "lock", 0.0f, true));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_robot_config_factory_hardware_matches_spec);
    RUN_TEST(test_robot_config_factory_profiles_differ_only_in_vmax);
    RUN_TEST(test_robot_config_factory_values_are_inside_ranges);
    RUN_TEST(test_robot_config_keys_fit_command_buffer_and_have_known_group);
    RUN_TEST(test_robot_config_set_applies_valid_value);
    RUN_TEST(test_robot_config_unknown_key);
    RUN_TEST(test_robot_config_out_of_range_keeps_value);
    RUN_TEST(test_robot_config_rejects_non_finite);
    RUN_TEST(test_robot_config_integer_fields_round);
    RUN_TEST(test_robot_config_hardware_and_mode_locked_while_running);
    RUN_TEST(test_robot_config_vmin_cannot_exceed_vmax);
    RUN_TEST(test_robot_config_get_unknown_returns_false);
    RUN_TEST(test_robot_config_validate_detects_bad_values);
    RUN_TEST(test_robot_config_lock_defaults_off_and_is_hardware_key);
    return UNITY_END();
}
