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

static void calibrateFlat(Calibration& cal) {
    int lo[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int hi[8] = {1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000};
    cal.update(lo);
    cal.update(hi);
}

// F-02: sem calibração, normalize não pode fazer INT_MIN − INT_MAX
void test_calibration_normalize_after_reset_is_zero() {
    Calibration cal(8);
    int raw[8] = {500, 500, 500, 500, 500, 500, 500, 500};
    int out[8];
    cal.normalize(raw, out);
    for (int i = 0; i < 8; i++) TEST_ASSERT_EQUAL_INT(0, out[i]);
}

void test_calibration_white_line_inverts_normalized_value() {
    Calibration cal(8);
    calibrateFlat(cal);
    cal.setWhiteLine(true);
    int raw[8] = {0, 250, 500, 750, 1000, 1000, 1000, 1000};
    int out[8];
    cal.normalize(raw, out);
    TEST_ASSERT_EQUAL_INT(1000, out[0]);
    TEST_ASSERT_EQUAL_INT(750, out[1]);
    TEST_ASSERT_EQUAL_INT(0, out[4]);
}

// F-03: QTR-8A lê menos tensão sobre o branco; a posição tem que seguir a leitura baixa
void test_calibration_white_line_position_follows_low_reading() {
    Calibration cal(8);
    calibrateFlat(cal);
    cal.setWhiteLine(true);
    int raw[8] = {1000, 1000, 1000, 1000, 1000, 1000, 100, 0};
    int norm[8];
    cal.normalize(raw, norm);
    // (2500 × 900 + 3500 × 1000) / 1900
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 3026.3f, cal.weightedPosition(norm));
}

void test_calibration_is_valid_reports_narrow_channels() {
    Calibration cal(8);
    int lo[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int hi[8] = {1000, 1000, 1000, 150, 1000, 199, 1000, 1000};
    cal.update(lo);
    cal.update(hi);
    uint8_t bad[8] = {};
    int n = 0;
    TEST_ASSERT_FALSE(cal.isValid(bad, &n));
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_EQUAL_UINT8(3, bad[0]);
    TEST_ASSERT_EQUAL_UINT8(5, bad[1]);
}

void test_calibration_is_valid_after_full_sweep() {
    Calibration cal(8);
    calibrateFlat(cal);
    int n = -1;
    TEST_ASSERT_TRUE(cal.isValid(nullptr, &n));
    TEST_ASSERT_EQUAL_INT(0, n);
}

void test_calibration_is_invalid_after_reset() {
    Calibration cal(8);
    TEST_ASSERT_FALSE(cal.isValid());
}

void test_calibration_export_import_roundtrip() {
    Calibration a(8);
    int lo[8] = {10, 20, 30, 40, 50, 60, 70, 80};
    int hi[8] = {900, 910, 920, 930, 940, 950, 960, 970};
    a.update(lo);
    a.update(hi);
    const CalibrationData d = a.exportData();
    Calibration b(8);
    b.importData(d);
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_EQUAL_INT(a.getMin(i), b.getMin(i));
        TEST_ASSERT_EQUAL_INT(a.getMax(i), b.getMax(i));
    }
    TEST_ASSERT_TRUE(b.isValid());
}

void test_calibration_import_uncalibrated_channel_stays_invalid() {
    CalibrationData d{};
    for (int i = 0; i < 8; i++) { d.min[i] = 0; d.max[i] = 1000; }
    d.max[2] = 0;  // canal 2 sem faixa
    Calibration cal(8);
    cal.importData(d);
    TEST_ASSERT_FALSE(cal.isValid());
    TEST_ASSERT_TRUE(cal.getMin(2) > cal.getMax(2));
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
    RUN_TEST(test_calibration_normalize_after_reset_is_zero);
    RUN_TEST(test_calibration_white_line_inverts_normalized_value);
    RUN_TEST(test_calibration_white_line_position_follows_low_reading);
    RUN_TEST(test_calibration_is_valid_reports_narrow_channels);
    RUN_TEST(test_calibration_is_valid_after_full_sweep);
    RUN_TEST(test_calibration_is_invalid_after_reset);
    RUN_TEST(test_calibration_export_import_roundtrip);
    RUN_TEST(test_calibration_import_uncalibrated_channel_stays_invalid);
    return UNITY_END();
}
