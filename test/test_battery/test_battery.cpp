#include <unity.h>
#include "config/RobotConfig.h"
#include "sensors/BatteryMonitor.h"

static constexpr float RATIO = 147.0f / 47.0f;
static HardwareConfig hw;

void setUp() { hw = robotcfg::factoryHardware(); }
void tearDown() {}

void test_battery_first_sample_seeds_filter() {
    BatteryMonitor b(RATIO);
    TEST_ASSERT_FALSE(b.hasReading());
    b.update(2687, 0.1f, 0, hw);  // 2,687 V no ADC → 8,404 V na bateria
    TEST_ASSERT_TRUE(b.hasReading());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 8.404f, b.volts());
}

void test_battery_correction_factor_applies() {
    BatteryMonitor b(RATIO);
    hw.vbk = 1.1f;
    b.update(2000, 0.1f, 0, hw);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.0f * RATIO * 1.1f, b.volts());
}

void test_battery_filter_follows_step_with_time_constant() {
    BatteryMonitor b(RATIO, 0.5f);
    b.update(2400, 0.1f, 0, hw);
    const float start = b.volts();
    const float target = 2.2f * RATIO;
    uint32_t t = 0;
    for (int i = 0; i < 5; i++) b.update(2200, 0.1f, t += 100, hw);  // 0,5 s = 1 τ
    const float progress = (start - b.volts()) / (start - target);
    TEST_ASSERT_FLOAT_WITHIN(0.08f, 0.63f, progress);
}

void test_battery_low_requires_hold_time() {
    BatteryMonitor b(RATIO, 0.01f, 1000);
    b.update(1950, 0.1f, 0, hw);  // ≈ 6,1 V < 6,4 V
    TEST_ASSERT_FALSE(b.isLow());
    b.update(1950, 0.1f, 999, hw);
    TEST_ASSERT_FALSE(b.isLow());
    b.update(1950, 0.1f, 1000, hw);
    TEST_ASSERT_TRUE(b.isLow());
}

void test_battery_recovery_clears_low() {
    BatteryMonitor b(RATIO, 0.01f, 1000);
    b.update(1950, 0.1f, 0, hw);
    b.update(1950, 0.1f, 1500, hw);
    TEST_ASSERT_TRUE(b.isLow());
    b.update(2500, 0.1f, 1600, hw);
    TEST_ASSERT_FALSE(b.isLow());
}

// Review Focus 2: divisor solto → 0 mV
void test_battery_zero_reading_goes_low() {
    BatteryMonitor b(RATIO, 0.01f, 1000);
    b.update(0, 0.1f, 0, hw);
    b.update(0, 0.1f, 1000, hw);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, b.volts());
    TEST_ASSERT_TRUE(b.isLow());
    TEST_ASSERT_EQUAL_UINT8(0, b.percent(1));
}

void test_battery_percent_tables() {
    TEST_ASSERT_EQUAL_UINT8(100, BatteryMonitor::percentFor(8.4f, 1));
    TEST_ASSERT_EQUAL_UINT8(0, BatteryMonitor::percentFor(6.0f, 1));
    TEST_ASSERT_EQUAL_UINT8(40, BatteryMonitor::percentFor(7.2f, 1));
    TEST_ASSERT_EQUAL_UINT8(100, BatteryMonitor::percentFor(9.0f, 1));
    TEST_ASSERT_EQUAL_UINT8(0, BatteryMonitor::percentFor(6.6f, 0));
    TEST_ASSERT_EQUAL_UINT8(40, BatteryMonitor::percentFor(7.4f, 0));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_battery_first_sample_seeds_filter);
    RUN_TEST(test_battery_correction_factor_applies);
    RUN_TEST(test_battery_filter_follows_step_with_time_constant);
    RUN_TEST(test_battery_low_requires_hold_time);
    RUN_TEST(test_battery_recovery_clears_low);
    RUN_TEST(test_battery_zero_reading_goes_low);
    RUN_TEST(test_battery_percent_tables);
    return UNITY_END();
}
