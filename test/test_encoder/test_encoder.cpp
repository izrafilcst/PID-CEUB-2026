// test/test_encoder/test_encoder.cpp
#include <unity.h>
#include "sensors/Encoder.h"

static Encoder* enc = nullptr;

void setUp() {
    enc = new Encoder(18, 19);  // pinos arbitrários — em native não importam
    enc->begin();
}

void tearDown() {
    delete enc;
    enc = nullptr;
}

// ─── 1. Estado inicial ──────────────────────────────────────────────────────
void test_encoder_starts_at_zero() {
    TEST_ASSERT_EQUAL_INT32(0, enc->getCount());
}

// ─── 2. Sequência completa para frente: 00 → 01 → 11 → 10 → 00 = +4 ────────
void test_encoder_forward_full_period_counts_four() {
    enc->_simulatePulse(false, true);   // 00 → 01
    enc->_simulatePulse(true,  true);   // 01 → 11
    enc->_simulatePulse(true,  false);  // 11 → 10
    enc->_simulatePulse(false, false);  // 10 → 00
    TEST_ASSERT_EQUAL_INT32(4, enc->getCount());
}

// ─── 3. Sequência completa para trás: 00 → 10 → 11 → 01 → 00 = −4 ──────────
void test_encoder_reverse_full_period_counts_minus_four() {
    enc->_simulatePulse(true,  false);  // 00 → 10
    enc->_simulatePulse(true,  true);   // 10 → 11
    enc->_simulatePulse(false, true);   // 11 → 01
    enc->_simulatePulse(false, false);  // 01 → 00
    TEST_ASSERT_EQUAL_INT32(-4, enc->getCount());
}

// ─── 4. Transições inválidas (glitch / ISR perdida) são ignoradas ──────────
void test_encoder_invalid_transition_ignored() {
    enc->_simulatePulse(true, true);  // 00 → 11 inválido (pula estado)
    TEST_ASSERT_EQUAL_INT32(0, enc->getCount());
}

// ─── 5. reset() zera contagem ──────────────────────────────────────────────
void test_encoder_reset_zeroes_count() {
    enc->_simulatePulse(false, true);   // 00 → 01 → +1
    enc->_simulatePulse(true,  true);   // 01 → 11 → +1
    TEST_ASSERT_EQUAL_INT32(2, enc->getCount());
    enc->reset();
    TEST_ASSERT_EQUAL_INT32(0, enc->getCount());
}

// ─── 6. getDelta() devolve mudança desde última chamada e zera delta ───────
void test_encoder_getDelta_returns_change_since_last_call() {
    enc->_simulatePulse(false, true);  // +1
    enc->_simulatePulse(true,  true);  // +1
    TEST_ASSERT_EQUAL_INT32(2, enc->getDelta());
    TEST_ASSERT_EQUAL_INT32(0, enc->getDelta());  // 2ª chamada: nada novo
    enc->_simulatePulse(true,  false); // +1
    TEST_ASSERT_EQUAL_INT32(1, enc->getDelta());
}

// ─── 7. getDelta() NÃO zera count() ────────────────────────────────────────
void test_encoder_getDelta_preserves_total_count() {
    enc->_simulatePulse(false, true);
    enc->_simulatePulse(true,  true);
    enc->getDelta();
    TEST_ASSERT_EQUAL_INT32(2, enc->getCount());
}

// ─── 8. getDirection() reflete última transição ────────────────────────────
void test_encoder_direction_matches_last_pulse() {
    enc->_simulatePulse(false, true);
    TEST_ASSERT_EQUAL_INT8(+1, enc->getDirection());
    enc->_simulatePulse(false, false);
    TEST_ASSERT_EQUAL_INT8(-1, enc->getDirection());
}

// ─── 9. Symmetric: avançar 100 + recuar 100 = 0 ────────────────────────────
void test_encoder_symmetric_forward_reverse() {
    // 25 períodos completos para frente (4 transições cada)
    for (int i = 0; i < 25; i++) {
        enc->_simulatePulse(false, true);
        enc->_simulatePulse(true,  true);
        enc->_simulatePulse(true,  false);
        enc->_simulatePulse(false, false);
    }
    TEST_ASSERT_EQUAL_INT32(100, enc->getCount());
    // 25 períodos completos para trás
    for (int i = 0; i < 25; i++) {
        enc->_simulatePulse(true,  false);
        enc->_simulatePulse(true,  true);
        enc->_simulatePulse(false, true);
        enc->_simulatePulse(false, false);
    }
    TEST_ASSERT_EQUAL_INT32(0, enc->getCount());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_encoder_starts_at_zero);
    RUN_TEST(test_encoder_forward_full_period_counts_four);
    RUN_TEST(test_encoder_reverse_full_period_counts_minus_four);
    RUN_TEST(test_encoder_invalid_transition_ignored);
    RUN_TEST(test_encoder_reset_zeroes_count);
    RUN_TEST(test_encoder_getDelta_returns_change_since_last_call);
    RUN_TEST(test_encoder_getDelta_preserves_total_count);
    RUN_TEST(test_encoder_direction_matches_last_pulse);
    RUN_TEST(test_encoder_symmetric_forward_reverse);
    return UNITY_END();
}
