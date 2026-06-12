#include <unity.h>

#include "ota_progress.h"

void test_ota_progress_status_mapping()
{
    TEST_ASSERT_EQUAL_INT(5, firmngin_normalize_ota_progress("checking", nullptr));
    TEST_ASSERT_EQUAL_INT(25, firmngin_normalize_ota_progress("downloading", nullptr));
    TEST_ASSERT_EQUAL_INT(100, firmngin_normalize_ota_progress("installed", nullptr));
    TEST_ASSERT_EQUAL_INT(0, firmngin_normalize_ota_progress("failed", nullptr));
}

void test_ota_progress_message_override()
{
    TEST_ASSERT_EQUAL_INT(42, firmngin_normalize_ota_progress("downloading", "progress 42%"));
    TEST_ASSERT_EQUAL_INT(100, firmngin_normalize_ota_progress("downloading", "progress 150%"));
}

void test_ota_progress_case_insensitive()
{
    TEST_ASSERT_EQUAL_INT(95, firmngin_normalize_ota_progress("VERIFYING", nullptr));
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_ota_progress_status_mapping);
    RUN_TEST(test_ota_progress_message_override);
    RUN_TEST(test_ota_progress_case_insensitive);
    return UNITY_END();
}
