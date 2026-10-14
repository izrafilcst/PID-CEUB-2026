#include <unity.h>
#include <cstdint>
#include "comm/PeriodicTimer.h"

void setUp() {}
void tearDown() {}

// Consulta a cada stepMs durante spanMs a partir de `from` (conta passos: atravessa o zero)
static int countEvents(PeriodicTimer& t, uint32_t from, uint32_t spanMs, uint32_t stepMs) {
    int n = 0;
    for (uint32_t k = 0; k * stepMs < spanMs; k++) {
        if (t.due(from + k * stepMs)) n++;
    }
    return n;
}

// A-WP1-06: a task comm roda a cada 10 ms; o tel tem que sair a ~30 Hz, não a 25 Hz
void test_periodic_timer_33ms_polled_every_10ms_gives_30_hz() {
    PeriodicTimer t(33);
    const int n = countEvents(t, 0, 3000, 10);
    TEST_ASSERT_INT_WITHIN(1, 90, n);
}

void test_periodic_timer_fires_on_first_call() {
    PeriodicTimer t(33);
    TEST_ASSERT_TRUE(t.due(1234));
    TEST_ASSERT_FALSE(t.due(1240));
}

void test_periodic_timer_no_burst_after_stall() {
    PeriodicTimer t(33);
    TEST_ASSERT_TRUE(t.due(0));
    TEST_ASSERT_TRUE(t.due(1000));   // atrasou ~30 períodos: dispara uma vez só
    TEST_ASSERT_FALSE(t.due(1010));
    TEST_ASSERT_FALSE(t.due(1020));
    TEST_ASSERT_TRUE(t.due(1040));
}

void test_periodic_timer_survives_millis_wraparound() {
    PeriodicTimer t(33);
    const uint32_t start = 0xFFFFFFFFu - 1000u;
    const int n = countEvents(t, start, 3000, 10);  // atravessa o zero
    TEST_ASSERT_INT_WITHIN(1, 90, n);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_periodic_timer_33ms_polled_every_10ms_gives_30_hz);
    RUN_TEST(test_periodic_timer_fires_on_first_call);
    RUN_TEST(test_periodic_timer_no_burst_after_stall);
    RUN_TEST(test_periodic_timer_survives_millis_wraparound);
    return UNITY_END();
}
