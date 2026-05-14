#include <unity.h>

void setUp() {}
void tearDown() {}

void test_framework_works() {
    TEST_ASSERT_TRUE(1);
    TEST_ASSERT_EQUAL(42, 42);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_framework_works);
    return UNITY_END();
}
