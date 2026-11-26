#ifndef FIRMNGIN_OTA_ROLLBACK_H
#define FIRMNGIN_OTA_ROLLBACK_H

// OTA boot-rollback state machine. Portable: no Arduino types, no NVS.
// The caller (Firmngin) owns persistence (Preferences) and the esp_ota_* calls.

#define OTA_ROLLBACK_MAX_BOOTS 3
#define OTA_ROLLBACK_BOOT_OK_MS 60000UL

enum OtaRollbackAction
{
    OTA_RB_NOOP = 0,
    OTA_RB_MARK_VALID,
    OTA_RB_REPORT_ROLLED_BACK,
    OTA_RB_REPORT_BOOT_FAILED
};

struct OtaRollbackState
{
    const char *lastOkVersion; // may be nullptr
    bool pending;
    int bootCount;
};

struct OtaRollbackResult
{
    OtaRollbackAction action;
    bool clearPending; // caller should clear the pending flag
    bool setLastOk;    // caller should store the running version as last known good
    int nextBootCount; // value to persist
};

// Decides the rollback action for the current boot. runningVersion == nullptr is
// treated as "". uptimeMs is the time since boot (0 on the very first check).
// candidatePartition: 1 = running the saved target partition, 0 = another
// partition, -1 = legacy state without partition identity (version fallback).
OtaRollbackResult firmngin_ota_rollback_decide(const char *runningVersion,
                                               const OtaRollbackState &state,
                                               unsigned long uptimeMs,
                                               int candidatePartition = -1);

// Arduino Preferences::remove returns false for an absent key.
template <typename Store>
bool firmngin_clear_ota_boot_report(Store &store)
{
    return (!store.isKey("boot_report") || store.remove("boot_report")) &&
           (!store.isKey("boot_message") || store.remove("boot_message"));
}

#endif // FIRMNGIN_OTA_ROLLBACK_H
