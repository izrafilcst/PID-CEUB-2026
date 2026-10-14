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

// A-WP1-01: o info só chega se for mandado depois de o cliente ativar as notificações
void test_ble_tuner_notify_subscription_raises_flag_once() {
    TEST_ASSERT_FALSE(t->consumeNewSubscription());
    t->injectSubscribeForTest(1);  // notificações
    TEST_ASSERT_TRUE(t->consumeNewSubscription());
    TEST_ASSERT_FALSE(t->consumeNewSubscription());
    t->injectSubscribeForTest(3);  // notificações + indicações
    TEST_ASSERT_TRUE(t->consumeNewSubscription());
}

void test_ble_tuner_unsubscribe_or_indications_only_do_not_raise_flag() {
    t->injectSubscribeForTest(0);
    TEST_ASSERT_FALSE(t->consumeNewSubscription());
    t->injectSubscribeForTest(2);
    TEST_ASSERT_FALSE(t->consumeNewSubscription());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_ble_tuner_starts_empty_and_disconnected);
    RUN_TEST(test_ble_tuner_queues_received_commands_in_order);
    RUN_TEST(test_ble_tuner_notify_while_disconnected_is_noop);
    RUN_TEST(test_ble_tuner_notify_subscription_raises_flag_once);
    RUN_TEST(test_ble_tuner_unsubscribe_or_indications_only_do_not_raise_flag);
    return UNITY_END();
}
