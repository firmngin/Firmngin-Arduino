#include <unity.h>

#include "json.h"

void test_json_parser_string_and_int()
{
    const char *json = "{\"pin\":\"123456\",\"ttl\":60,\"pin_met\":true}";
    firmngin_json::Parser p(json, strlen(json));

    char pin[16];
    TEST_ASSERT_EQUAL_UINT32(6, p.getString("pin", pin, sizeof(pin)));
    TEST_ASSERT_EQUAL_STRING("123456", pin);
    TEST_ASSERT_EQUAL_INT(60, p.getInt("ttl", 0));
    TEST_ASSERT_TRUE(p.getBool("pin_met", false));
}

void test_json_builder_escape()
{
    char buf[128];
    firmngin_json::Builder b(buf, sizeof(buf));
    b.reset();
    TEST_ASSERT_TRUE(b.add("k", "a\"b"));
    const char *out = b.build();
    TEST_ASSERT_NOT_NULL(strstr(out, "\\\""));
}

void test_json_array_builder()
{
    char buf[256];
    firmngin_json::ArrayBuilder a(buf, sizeof(buf));
    a.reset();
    TEST_ASSERT_TRUE(a.add("gpio_1", "1"));
    TEST_ASSERT_TRUE(a.add("temp", "25.5"));
    TEST_ASSERT_EQUAL_INT(2, a.count());
    const char *out = a.build();
    TEST_ASSERT_NOT_NULL(strstr(out, "\"k\":\"gpio_1\""));
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_json_parser_string_and_int);
    RUN_TEST(test_json_builder_escape);
    RUN_TEST(test_json_array_builder);
    return UNITY_END();
}
