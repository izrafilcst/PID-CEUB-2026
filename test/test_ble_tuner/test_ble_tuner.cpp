#include <unity.h>
#include <cstring>
#include "comm/BLETuner.h"

static BLETuner* t = nullptr;
void setUp() { t = new BLETuner(); }
void tearDown() { delete t; t = nullptr; }

void test_ble_tuner_starts_empty_and_disconnected() {
    char buf[64];
    TEST_ASSERT_FALSE(t->popCommand(buf, sizeof buf));
    TEST_ASSERT_FALSE(t->isConnected());
    TEST_ASSERT_FALSE(t->consumeNewConnection());
}

void test_ble_tuner_queues_received_commands_in_order() {
    TEST_ASSERT_TRUE(t->injectForTest("{\"t\":\"get\"}"));
    TEST_ASSERT_TRUE(t->injectForTest("{\"t\":\"save\"}"));
    char buf[64];
    TEST_ASSERT_TRUE(t->popCommand(buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"get\"}", buf);
    TEST_ASSERT_TRUE(t->popCommand(buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"save\"}", buf);
}

void test_ble_tuner_notify_while_disconnected_is_noop() {
    t->notify("{\"t\":\"tel\"}");
    TEST_PASS();
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_ble_tuner_starts_empty_and_disconnected);
    RUN_TEST(test_ble_tuner_queues_received_commands_in_order);
    RUN_TEST(test_ble_tuner_notify_while_disconnected_is_noop);
    return UNITY_END();
}
