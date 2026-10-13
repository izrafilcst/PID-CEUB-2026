#include <unity.h>
#include "sensors/ButtonInput.h"

void setUp() {}
void tearDown() {}

struct Counts {
    int shortCount = 0;
    int longCount = 0;
};

// Alimenta o botão com o mesmo nível de `from` até `to` (exclusivo), em passos de 1 ms
static void feed(ButtonInput& b, bool pressed, uint32_t from, uint32_t to, Counts& c) {
    for (uint32_t t = from; t < to; t++) {
        const ButtonEvent e = b.update(pressed, t);
        if (e == ButtonEvent::SHORT) c.shortCount++;
        if (e == ButtonEvent::LONG) c.longCount++;
    }
}

void test_button_short_press_fires_once_on_release() {
    ButtonInput b;
    Counts c;
    feed(b, false, 0, 100, c);
    feed(b, true, 100, 300, c);
    feed(b, false, 300, 400, c);
    TEST_ASSERT_EQUAL_INT(1, c.shortCount);
    TEST_ASSERT_EQUAL_INT(0, c.longCount);
}

void test_button_bounce_is_filtered() {
    ButtonInput b;
    Counts c;
    for (uint32_t t = 0; t < 20; t++) {
        const ButtonEvent e = b.update((t / 5) % 2 == 0, t);  // quica a cada 5 ms
        if (e != ButtonEvent::NONE) c.shortCount++;
    }
    feed(b, true, 20, 220, c);
    feed(b, false, 220, 320, c);
    TEST_ASSERT_EQUAL_INT(1, c.shortCount);
}

void test_button_glitch_shorter_than_debounce_is_ignored() {
    ButtonInput b;
    Counts c;
    feed(b, false, 0, 50, c);
    feed(b, true, 50, 60, c);  // 10 ms < 30 ms
    feed(b, false, 60, 200, c);
    TEST_ASSERT_EQUAL_INT(0, c.shortCount);
    TEST_ASSERT_EQUAL_INT(0, c.longCount);
}

void test_button_long_press_fires_while_held_and_not_short() {
    ButtonInput b;
    Counts c;
    feed(b, false, 0, 10, c);
    feed(b, true, 10, 1600, c);  // pressedAt = 40; LONG em 1540
    TEST_ASSERT_EQUAL_INT(1, c.longCount);
    feed(b, true, 1600, 2500, c);
    feed(b, false, 2500, 2600, c);
    TEST_ASSERT_EQUAL_INT(1, c.longCount);
    TEST_ASSERT_EQUAL_INT(0, c.shortCount);
}

void test_button_medium_press_fires_nothing() {
    ButtonInput b;
    Counts c;
    feed(b, true, 0, 1000, c);
    feed(b, false, 1000, 1100, c);
    TEST_ASSERT_EQUAL_INT(0, c.shortCount);
    TEST_ASSERT_EQUAL_INT(0, c.longCount);
}

// Review Focus 4
void test_button_held_from_boot_fires_single_long() {
    ButtonInput b;
    Counts c;
    feed(b, true, 0, 5000, c);
    feed(b, false, 5000, 5100, c);
    TEST_ASSERT_EQUAL_INT(1, c.longCount);
    TEST_ASSERT_EQUAL_INT(0, c.shortCount);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_button_short_press_fires_once_on_release);
    RUN_TEST(test_button_bounce_is_filtered);
    RUN_TEST(test_button_glitch_shorter_than_debounce_is_ignored);
    RUN_TEST(test_button_long_press_fires_while_held_and_not_short);
    RUN_TEST(test_button_medium_press_fires_nothing);
    RUN_TEST(test_button_held_from_boot_fires_single_long);
    return UNITY_END();
}
