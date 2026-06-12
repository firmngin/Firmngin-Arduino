#ifndef FIRMNGIN_OTA_PROGRESS_H
#define FIRMNGIN_OTA_PROGRESS_H

// Maps OTA status/message to 0-100 progress. Portable (no Arduino types).
int firmngin_normalize_ota_progress(const char *status, const char *message);

#endif
