#ifndef FIRMNGIN_NVS_H
#define FIRMNGIN_NVS_H

// Pure-ESP-IDF stand-in for the Arduino Preferences class, restricted to the
// subset the OTA rollback tracker uses. Same namespace/key/type mapping as
// Preferences (bool=u8, uchar=u8, uint=u32) so states stay portable.
// Arduino builds keep Preferences untouched: zero behavior change there.

// This shim compiles only under pure ESP-IDF. Arduino builds use Preferences.
#if defined(ESP_PLATFORM) && !defined(ARDUINO)
#include "fng_compat.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <vector>

class FngPrefs
{
public:
    bool begin(const char *name, bool readOnly = false)
    {
        if (_opened)
        {
            nvs_close(_handle);
            _opened = false;
        }
        _readOnly = readOnly;
        const esp_err_t err = nvs_open(name, readOnly ? NVS_READONLY : NVS_READWRITE, &_handle);
        _opened = (err == ESP_OK);
        return _opened;
    }

    void end()
    {
        if (_opened)
        {
            nvs_close(_handle);
            _opened = false;
        }
    }

    FString getString(const char *key, const FString &defaultValue = "")
    {
        if (!_opened)
            return defaultValue;
        size_t required = 0;
        if (nvs_get_str(_handle, key, nullptr, &required) != ESP_OK || required == 0)
            return defaultValue;
        std::vector<char> buf(required);
        if (nvs_get_str(_handle, key, buf.data(), &required) != ESP_OK)
            return defaultValue;
        return FString(buf.data());
    }

    bool getBool(const char *key, bool defaultValue = false)
    {
        uint8_t value = 0;
        return _opened && nvs_get_u8(_handle, key, &value) == ESP_OK ? value != 0 : defaultValue;
    }

    uint8_t getUChar(const char *key, uint8_t defaultValue = 0)
    {
        uint8_t value = defaultValue;
        return _opened && nvs_get_u8(_handle, key, &value) == ESP_OK ? value : defaultValue;
    }

    uint32_t getUInt(const char *key, uint32_t defaultValue = 0)
    {
        uint32_t value = defaultValue;
        return _opened && nvs_get_u32(_handle, key, &value) == ESP_OK ? value : defaultValue;
    }

    size_t putString(const char *key, const FString &value)
    {
        if (!_opened || _readOnly || nvs_set_str(_handle, key, value.c_str()) != ESP_OK)
            return 0;
        return commit() == ESP_OK ? value.length() : 0;
    }

    size_t putBool(const char *key, bool value)
    {
        if (!_opened || _readOnly || nvs_set_u8(_handle, key, value ? 1 : 0) != ESP_OK)
            return 0;
        return commit() == ESP_OK ? 1 : 0;
    }

    size_t putUChar(const char *key, uint8_t value)
    {
        if (!_opened || _readOnly || nvs_set_u8(_handle, key, value) != ESP_OK)
            return 0;
        return commit() == ESP_OK ? 1 : 0;
    }

    size_t putUInt(const char *key, uint32_t value)
    {
        if (!_opened || _readOnly || nvs_set_u32(_handle, key, value) != ESP_OK)
            return 0;
        return commit() == ESP_OK ? 4 : 0;
    }

    bool isKey(const char *key)
    {
        size_t size = 0;
        // A type mismatch still means the key exists; other errors must reach
        // remove() rather than silently treating a failed lookup as absence.
        return nvs_get_str(_handle, key, nullptr, &size) != ESP_ERR_NVS_NOT_FOUND;
    }

    bool remove(const char *key)
    {
        if (!_opened || _readOnly)
            return false;
        const esp_err_t err = nvs_erase_key(_handle, key);
        if (err == ESP_OK)
            return commit() == ESP_OK;
        // Absent key is the same end-state as "already erased".
        return err == ESP_ERR_NVS_NOT_FOUND;
    }

    void clear()
    {
        if (!_opened || _readOnly)
            return;
        nvs_erase_all(_handle);
        commit();
    }

private:
    esp_err_t commit()
    {
        const esp_err_t err = nvs_commit(_handle);
        return err;
    }

    nvs_handle_t _handle = 0;
    bool _opened = false;
    bool _readOnly = false;
};

#endif // ESP_PLATFORM

#endif // FIRMNGIN_NVS_H
