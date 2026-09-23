#include <unity.h>

#include "ota_rollback.h"

void test_no_pending_is_noop()
{
    OtaRollbackState st = {"1.0.0", false, 0};
    OtaRollbackResult r = firmngin_ota_rollback_decide("1.0.0", st, 0);
    TEST_ASSERT_EQUAL_INT(OTA_RB_NOOP, r.action);
    TEST_ASSERT_FALSE(r.clearPending);
    TEST_ASSERT_FALSE(r.setLastOk);
    TEST_ASSERT_EQUAL_INT(0, r.nextBootCount);
}

void test_candidate_boot_increments_count()
{
    OtaRollbackState st = {"1.0.0", true, 0};
    OtaRollbackResult r = firmngin_ota_rollback_decide("1.0.1", st, 1000);
    TEST_ASSERT_EQUAL_INT(OTA_RB_NOOP, r.action);
    TEST_ASSERT_EQUAL_INT(1, r.nextBootCount);
    TEST_ASSERT_FALSE(r.clearPending);
    TEST_ASSERT_FALSE(r.setLastOk);
}

void test_rollback_detected_when_running_is_last_ok()
{
    OtaRollbackState st = {"1.0.0", true, 2};
    OtaRollbackResult r = firmngin_ota_rollback_decide("1.0.0", st, 0);
    TEST_ASSERT_EQUAL_INT(OTA_RB_REPORT_ROLLED_BACK, r.action);
    TEST_ASSERT_TRUE(r.clearPending);
    TEST_ASSERT_FALSE(r.setLastOk);
    TEST_ASSERT_EQUAL_INT(0, r.nextBootCount);
}

void test_mark_valid_after_grace_period()
{
    OtaRollbackState st = {"1.0.0", true, 1};
    OtaRollbackResult r = firmngin_ota_rollback_decide("1.0.1", st, OTA_ROLLBACK_BOOT_OK_MS);
    TEST_ASSERT_EQUAL_INT(OTA_RB_MARK_VALID, r.action);
    TEST_ASSERT_TRUE(r.clearPending);
    TEST_ASSERT_TRUE(r.setLastOk);
    TEST_ASSERT_EQUAL_INT(0, r.nextBootCount);
}

void test_boot_failed_at_threshold()
{
    OtaRollbackState st = {"1.0.0", true, 2};
    OtaRollbackResult r = firmngin_ota_rollback_decide("1.0.1", st, 1000);
    TEST_ASSERT_EQUAL_INT(OTA_RB_REPORT_BOOT_FAILED, r.action);
    TEST_ASSERT_EQUAL_INT(3, r.nextBootCount);
    TEST_ASSERT_FALSE(r.clearPending);
}

void test_boot_failed_fires_once()
{
    OtaRollbackState st = {"1.0.0", true, 3};
    OtaRollbackResult r = firmngin_ota_rollback_decide("1.0.1", st, 1000);
    TEST_ASSERT_EQUAL_INT(OTA_RB_NOOP, r.action);
    TEST_ASSERT_EQUAL_INT(3, r.nextBootCount);
}

void test_last_ok_updates_on_mark_valid()
{
    OtaRollbackState st = {"1.0.0", true, 0};
    OtaRollbackResult r = firmngin_ota_rollback_decide("1.0.1", st, OTA_ROLLBACK_BOOT_OK_MS);
    TEST_ASSERT_EQUAL_INT(OTA_RB_MARK_VALID, r.action);
    TEST_ASSERT_TRUE(r.setLastOk);
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_no_pending_is_noop);
    RUN_TEST(test_candidate_boot_increments_count);
    RUN_TEST(test_rollback_detected_when_running_is_last_ok);
    RUN_TEST(test_mark_valid_after_grace_period);
    RUN_TEST(test_boot_failed_at_threshold);
    RUN_TEST(test_boot_failed_fires_once);
    RUN_TEST(test_last_ok_updates_on_mark_valid);
    return UNITY_END();
}
