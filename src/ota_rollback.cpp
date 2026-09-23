#include "ota_rollback.h"

#include <string.h>

OtaRollbackResult firmngin_ota_rollback_decide(const char *runningVersion,
                                               const OtaRollbackState &state,
                                               unsigned long uptimeMs)
{
    OtaRollbackResult r;
    r.action = OTA_RB_NOOP;
    r.clearPending = false;
    r.setLastOk = false;
    r.nextBootCount = state.bootCount;

    if (!state.pending)
        return r;

    const char *rv = (runningVersion != nullptr) ? runningVersion : "";
    const char *lv = (state.lastOkVersion != nullptr) ? state.lastOkVersion : "";

    if (strcmp(rv, lv) == 0)
    {
        // The previously-good app is back while an OTA was pending: a rollback
        // happened (bootloader reverted after the new app failed to verify).
        r.action = OTA_RB_REPORT_ROLLED_BACK;
        r.clearPending = true;
        r.nextBootCount = 0;
        return r;
    }

    // Candidate (new) app boot: count attempts, then mark valid once the app has
    // been stable for the grace period. Report boot_failed once at the threshold
    // (mainly meaningful on ESP8266; on ESP32 the bootloader reverts instead).
    r.nextBootCount = (state.bootCount < OTA_ROLLBACK_MAX_BOOTS)
                          ? state.bootCount + 1
                          : state.bootCount;

    if (uptimeMs >= OTA_ROLLBACK_BOOT_OK_MS)
    {
        r.action = OTA_RB_MARK_VALID;
        r.clearPending = true;
        r.setLastOk = true;
        r.nextBootCount = 0;
        return r;
    }

    if (state.bootCount < OTA_ROLLBACK_MAX_BOOTS &&
        r.nextBootCount >= OTA_ROLLBACK_MAX_BOOTS)
    {
        r.action = OTA_RB_REPORT_BOOT_FAILED;
    }
    return r;
}
