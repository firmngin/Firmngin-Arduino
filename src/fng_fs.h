#ifndef FIRMNGIN_FS_H
#define FIRMNGIN_FS_H

// LittleFS mount helper shared by both frameworks.
//
// The Arduino LittleFS class mounts partition label "spiffs" at base path
// "/littlefs" (its .begin() defaults). Library files live under that mount, so
// the C-API registration here uses the same base + label: existing provisioned
// devices keep reading their files. All access then happens through the VFS
// (stdio fopen/stat/remove/rename/mkdir), which routes through esp_vfs in both
// Arduino-ESP32 and pure ESP-IDF.
//
// Arduino builds may still use the LittleFS sketch API: the helper is
// idempotent because it only registers when the label is not already mounted.

#include "fng_compat.h"

#if defined(ESP_PLATFORM)
#include "esp_littlefs.h"
#include <sys/stat.h>
#include <sys/types.h>

#define FNG_FS_MOUNT_BASE "/littlefs"
#define FNG_FS_PARTITION_LABEL "spiffs"

inline bool fng_fs_mounted()
{
    size_t total = 0, used = 0;
    return esp_littlefs_info(FNG_FS_PARTITION_LABEL, &total, &used) == ESP_OK;
}

inline bool fng_fs_mount()
{
    if (fng_fs_mounted())
        return true;
    esp_vfs_littlefs_conf_t conf{};
    conf.base_path = FNG_FS_MOUNT_BASE;
    conf.partition_label = FNG_FS_PARTITION_LABEL;
    conf.format_if_mount_failed = true;
    return esp_vfs_littlefs_register(&conf) == ESP_OK;
}

inline void fng_fs_unmount()
{
    if (fng_fs_mounted())
        esp_vfs_littlefs_unregister(FNG_FS_PARTITION_LABEL);
}

// Full VFS path existence probe (mount-relative paths must already be resolved
// via fng_fs_path()).
inline bool fng_fs_file_exists(const char *path)
{
    if (path == nullptr)
        return false;
    struct stat st = {};
    return stat(path, &st) == 0;
}
#endif // ESP_PLATFORM

// Arduino LittleFS.open("/x") resolves to the VFS path <mount>/x; sketch-side
// absolute-seeming paths are relative to the mount. Translate the same way.
inline FString fng_fs_path(const char *path)
{
#if defined(ESP_PLATFORM)
    return FString(FNG_FS_MOUNT_BASE) + path;
#else
    (void)path;
    return FString();
#endif
}

#endif // FIRMNGIN_FS_H
