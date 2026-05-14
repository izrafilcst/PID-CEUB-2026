#include <unity.h>
#include "strategy/LapTimer.h"

void setUp() {}
void tearDown() {}

// RED: define expected behavior before implementation

void test_lap_timer_starts_stopped() {
    LapTimer lt;
    TEST_ASSERT_FALSE(lt.isRunning());
    TEST_ASSERT_EQUAL(0, lt.getLapCount());
    TEST_ASSERT_EQUAL_UINT32(0u, lt.getBestLapMs());
    TEST_ASSERT_EQUAL_UINT32(0u, lt.getLastLapMs());
}

void test_lap_timer_records_lap_on_crossing() {
    LapTimer lt;
    lt.start(1000);                  // começa em t=1000ms
    lt.notifyLineCrossing(3500);     // cruzamento em t=3500ms → 1° volta = 2500ms
    TEST_ASSERT_TRUE(lt.isRunning());
    TEST_ASSERT_EQUAL(1, lt.getLapCount());
    TEST_ASSERT_EQUAL_UINT32(2500u, lt.getLastLapMs());
}

void test_lap_timer_tracks_best_lap() {
    LapTimer lt;
    lt.start(0);
    lt.notifyLineCrossing(5000);   // volta 1 = 5000ms
    lt.notifyLineCrossing(8000);   // volta 2 = 3000ms  ← melhor
    lt.notifyLineCrossing(12000);  // volta 3 = 4000ms
    TEST_ASSERT_EQUAL(3, lt.getLapCount());
    TEST_ASSERT_EQUAL_UINT32(3000u, lt.getBestLapMs());
    TEST_ASSERT_EQUAL_UINT32(4000u, lt.getLastLapMs());
}

void test_lap_timer_resets_cleanly() {
    LapTimer lt;
    lt.start(0);
    lt.notifyLineCrossing(2000);
    lt.reset();
    TEST_ASSERT_FALSE(lt.isRunning());
    TEST_ASSERT_EQUAL(0, lt.getLapCount());
    TEST_ASSERT_EQUAL_UINT32(0u, lt.getBestLapMs());
    TEST_ASSERT_EQUAL_UINT32(0u, lt.getLastLapMs());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_lap_timer_starts_stopped);
    RUN_TEST(test_lap_timer_records_lap_on_crossing);
    RUN_TEST(test_lap_timer_tracks_best_lap);
    RUN_TEST(test_lap_timer_resets_cleanly);
    return UNITY_END();
}
