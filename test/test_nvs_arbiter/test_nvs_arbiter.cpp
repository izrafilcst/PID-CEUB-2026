#include <unity.h>
#include "storage/NvsArbiter.h"

void setUp() {}
void tearDown() {}

// A-WP1-05: escrita na NVS (comm) e carro armado (control) se excluem de forma atômica
void test_nvs_arbiter_starts_free() {
    NvsArbiter a;
    TEST_ASSERT_FALSE(a.writeInProgress());
    TEST_ASSERT_FALSE(a.armed());
}

void test_nvs_arbiter_write_blocks_arming() {
    NvsArbiter a;
    TEST_ASSERT_TRUE(a.tryBeginWrite());
    TEST_ASSERT_TRUE(a.writeInProgress());
    TEST_ASSERT_FALSE(a.tryArm());
    a.endWrite();
    TEST_ASSERT_TRUE(a.tryArm());
}

void test_nvs_arbiter_arming_blocks_write() {
    NvsArbiter a;
    TEST_ASSERT_TRUE(a.tryArm());
    TEST_ASSERT_TRUE(a.armed());
    TEST_ASSERT_FALSE(a.tryBeginWrite());
    a.disarm();
    TEST_ASSERT_TRUE(a.tryBeginWrite());
}

void test_nvs_arbiter_second_writer_is_refused() {
    NvsArbiter a;
    TEST_ASSERT_TRUE(a.tryBeginWrite());
    TEST_ASSERT_FALSE(a.tryBeginWrite());
    a.endWrite();
    TEST_ASSERT_FALSE(a.writeInProgress());
}

void test_nvs_arbiter_release_by_wrong_owner_is_ignored() {
    NvsArbiter a;
    TEST_ASSERT_TRUE(a.tryArm());
    a.endWrite();  // a comm não pode liberar o que é da control
    TEST_ASSERT_TRUE(a.armed());
    a.disarm();
    TEST_ASSERT_TRUE(a.tryBeginWrite());
    a.disarm();  // nem a control o que é da comm
    TEST_ASSERT_TRUE(a.writeInProgress());
}

void test_nvs_write_guard_releases_on_scope_exit() {
    NvsArbiter a;
    {
        NvsWriteGuard g(&a);
        TEST_ASSERT_TRUE(g.acquired());
        TEST_ASSERT_FALSE(a.tryArm());
    }
    TEST_ASSERT_TRUE(a.tryArm());
    {
        NvsWriteGuard g(&a);
        TEST_ASSERT_FALSE(g.acquired());  // carro armado: não escreve
    }
    TEST_ASSERT_TRUE(a.armed());
}

void test_nvs_write_guard_without_arbiter_always_acquires() {
    NvsWriteGuard g(nullptr);
    TEST_ASSERT_TRUE(g.acquired());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_nvs_arbiter_starts_free);
    RUN_TEST(test_nvs_arbiter_write_blocks_arming);
    RUN_TEST(test_nvs_arbiter_arming_blocks_write);
    RUN_TEST(test_nvs_arbiter_second_writer_is_refused);
    RUN_TEST(test_nvs_arbiter_release_by_wrong_owner_is_ignored);
    RUN_TEST(test_nvs_write_guard_releases_on_scope_exit);
    RUN_TEST(test_nvs_write_guard_without_arbiter_always_acquires);
    return UNITY_END();
}
