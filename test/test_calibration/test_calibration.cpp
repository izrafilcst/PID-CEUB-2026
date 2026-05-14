#include <unity.h>
#include "sensors/Calibration.h"

void setUp() {}
void tearDown() {}

void test_initial_min_max_inverted() {
    Calibration cal(8);
    // min deve começar alto, max deve começar baixo (para update funcionar)
    TEST_ASSERT_TRUE(cal.getMin(0) > cal.getMax(0));
}

void test_update_captures_extremes() {
    Calibration cal(8);
    int raw[8] = {100, 200, 300, 400, 500, 600, 700, 800};
    cal.update(raw);
    int raw2[8] = {50, 250, 280, 450, 480, 650, 650, 850};
    cal.update(raw2);
    TEST_ASSERT_EQUAL(50, cal.getMin(0));
    TEST_ASSERT_EQUAL(100, cal.getMax(0));
    TEST_ASSERT_EQUAL(800, cal.getMin(7));
    TEST_ASSERT_EQUAL(850, cal.getMax(7));
}

void test_normalize_0_to_1000() {
    Calibration cal(8);
    int raw[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    cal.update(raw);
    int raw2[8] = {1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000};
    cal.update(raw2);
    int result[8];
    int input[8] = {500, 500, 500, 500, 500, 500, 500, 500};
    cal.normalize(input, result);
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_EQUAL(500, result[i]);
    }
}

void test_normalize_clamps_outliers() {
    Calibration cal(8);
    int raw_min[8] = {200, 200, 200, 200, 200, 200, 200, 200};
    int raw_max[8] = {800, 800, 800, 800, 800, 800, 800, 800};
    cal.update(raw_min);
    cal.update(raw_max);
    int result[8];
    int input_low[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    cal.normalize(input_low, result);
    TEST_ASSERT_EQUAL(0, result[0]);
    int input_high[8] = {1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023};
    cal.normalize(input_high, result);
    TEST_ASSERT_EQUAL(1000, result[0]);
}

void test_normalize_handles_zero_range() {
    Calibration cal(8);
    int raw[8] = {500, 500, 500, 500, 500, 500, 500, 500};
    cal.update(raw);
    cal.update(raw);  // min == max
    int result[8];
    cal.normalize(raw, result);
    TEST_ASSERT_EQUAL(0, result[0]);  // sem div/0
}

void test_reset_clears() {
    Calibration cal(8);
    int raw[8] = {100, 100, 100, 100, 100, 100, 100, 100};
    cal.update(raw);
    cal.reset();
    TEST_ASSERT_TRUE(cal.getMin(0) > cal.getMax(0));
}

void test_weighted_position_center() {
    Calibration cal(8);
    // Calibrar com range 0-1000
    int rawMin[8] = {0,0,0,0,0,0,0,0};
    int rawMax[8] = {1000,1000,1000,1000,1000,1000,1000,1000};
    cal.update(rawMin);
    cal.update(rawMax);
    // sensor central (índice 3 e 4) ativos — posição deve ser ≈ 0
    int readings[8] = {0, 0, 0, 700, 700, 0, 0, 0};
    int normalized[8];
    cal.normalize(readings, normalized);
    float pos = cal.weightedPosition(normalized);
    TEST_ASSERT_FLOAT_WITHIN(200.0f, 0.0f, pos);
}

void test_weighted_position_left() {
    Calibration cal(8);
    int rawMin[8] = {0,0,0,0,0,0,0,0};
    int rawMax[8] = {1000,1000,1000,1000,1000,1000,1000,1000};
    cal.update(rawMin);
    cal.update(rawMax);
    int readings[8] = {900, 600, 100, 0, 0, 0, 0, 0};
    int normalized[8];
    cal.normalize(readings, normalized);
    float pos = cal.weightedPosition(normalized);
    TEST_ASSERT_TRUE(pos < -2000.0f);
}

void test_weighted_position_right() {
    Calibration cal(8);
    int rawMin[8] = {0,0,0,0,0,0,0,0};
    int rawMax[8] = {1000,1000,1000,1000,1000,1000,1000,1000};
    cal.update(rawMin);
    cal.update(rawMax);
    int readings[8] = {0, 0, 0, 0, 0, 100, 600, 900};
    int normalized[8];
    cal.normalize(readings, normalized);
    float pos = cal.weightedPosition(normalized);
    TEST_ASSERT_TRUE(pos > 2000.0f);
}

void test_line_lost_returns_last_known() {
    Calibration cal(8);
    int rawMin[8] = {0,0,0,0,0,0,0,0};
    int rawMax[8] = {1000,1000,1000,1000,1000,1000,1000,1000};
    cal.update(rawMin);
    cal.update(rawMax);
    int readings[8] = {0, 0, 0, 700, 700, 0, 0, 0};
    int normalized[8];
    cal.normalize(readings, normalized);
    float pos1 = cal.weightedPosition(normalized);  // posição válida
    int zeros[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    cal.normalize(zeros, normalized);
    float pos2 = cal.weightedPosition(normalized);  // linha perdida
    TEST_ASSERT_FLOAT_WITHIN(0.001f, pos1, pos2);  // mantém última posição
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_initial_min_max_inverted);
    RUN_TEST(test_update_captures_extremes);
    RUN_TEST(test_normalize_0_to_1000);
    RUN_TEST(test_normalize_clamps_outliers);
    RUN_TEST(test_normalize_handles_zero_range);
    RUN_TEST(test_reset_clears);
    RUN_TEST(test_weighted_position_center);
    RUN_TEST(test_weighted_position_left);
    RUN_TEST(test_weighted_position_right);
    RUN_TEST(test_line_lost_returns_last_known);
    return UNITY_END();
}
