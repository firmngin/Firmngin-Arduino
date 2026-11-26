#ifndef FIRMNGIN_OTA_PROGRESS_H
#define FIRMNGIN_OTA_PROGRESS_H

enum OTAAsyncState
{
    OTA_ASYNC_IDLE,
    OTA_ASYNC_DOWNLOADING,
    OTA_ASYNC_VERIFYING,
    OTA_ASYNC_INSTALLING,
    OTA_ASYNC_FAILED
};

inline bool firmngin_ota_can_start(OTAAsyncState state)
{
    return state == OTA_ASYNC_IDLE || state == OTA_ASYNC_FAILED;
}

// Maps OTA status/message to 0-100 progress. Portable (no Arduino types).
int firmngin_normalize_ota_progress(const char *status, const char *message);

#endif
