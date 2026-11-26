#ifndef FIRMNGIN_OTA_MANIFEST_H
#define FIRMNGIN_OTA_MANIFEST_H

// Pure OTA-manifest validation helpers shared by the ESP build and the native
// (host) unit tests. No Arduino or IDF headers beyond the compat layer.

#include "fng_compat.h"
#include <string.h>
#include <strings.h>
#include "json.h"

struct FirmnginOtaManifest
{
    String id;
    String version;
    String sha256;
};

enum class FirmnginOtaManifestResult { Available, UpToDate, Invalid };

inline bool otaHeaderMatches(const char *key, const char *expected)
{
    return key != nullptr && expected != nullptr && strcasecmp(key, expected) == 0;
}

inline bool isValidOTAHash(const String &hash)
{
    if (hash.length() != 64)
        return false;
    for (size_t i = 0; i < hash.length(); ++i)
    {
        const char c = fng_charAt(hash, i);
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F')))
            return false;
    }
    return true;
}

inline bool isValidFirmwareID(const char *id)
{
    if (id == nullptr || strlen(id) != 36)
        return false;
    for (size_t i = 0; i < 36; ++i)
    {
        if (i == 8 || i == 13 || i == 18 || i == 23)
        {
            if (id[i] != '-')
                return false;
            continue;
        }
        const char c = id[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F')))
            return false;
    }
    return true;
}

// RFC 3986 unreserved percent-encoding for the manifest query string, bounded
// by the caller-provided buffer.
inline bool appendOTAQueryParameter(char *query, size_t capacity, size_t &length,
                                    const char *name, const String &value)
{
    static const char hex[] = "0123456789ABCDEF";
    const size_t nameLength = strlen(name);
    const size_t separatorLength = length == 0 ? 0 : 1;
    if (length + separatorLength + nameLength + 1 >= capacity)
        return false;
    if (separatorLength > 0)
        query[length++] = '&';
    memcpy(query + length, name, nameLength);
    length += nameLength;
    query[length++] = '=';

    for (size_t i = 0; i < value.length(); ++i)
    {
        const uint8_t c = static_cast<uint8_t>(fng_charAt(value, i));
        const bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                                (c >= '0' && c <= '9') || c == '-' || c == '.' ||
                                c == '_' || c == '~';
        const size_t encodedLength = unreserved ? 1 : 3;
        if (length + encodedLength >= capacity)
            return false;
        if (unreserved)
        {
            query[length++] = static_cast<char>(c);
        }
        else
        {
            query[length++] = '%';
            query[length++] = hex[c >> 4];
            query[length++] = hex[c & 0x0F];
        }
    }
    query[length] = '\0';
    return true;
}

inline FirmnginOtaManifestResult firmngin_parse_ota_manifest(const String &body,
                                                           FirmnginOtaManifest &out)
{
    out = FirmnginOtaManifest();
    firmngin_json::Parser parser(body.c_str(), body.length());
    char id[64] = {}, version[256] = {}, sha[128] = {};
    const size_t idLength = parser.getString("firmware_id", id, sizeof(id));
    if (idLength == 0)
    {
        size_t length = 0;
        const char *data = parser.getRaw("data", length);
        return data != nullptr && length == 4 && memcmp(data, "null", 4) == 0
                   ? FirmnginOtaManifestResult::UpToDate
                   : FirmnginOtaManifestResult::Invalid;
    }
    const size_t versionLength = parser.getString("version", version, sizeof(version));
    const size_t shaLength = parser.getString("sha256", sha, sizeof(sha));
    if (idLength != 36 || !isValidFirmwareID(id) || versionLength == 0 ||
        versionLength >= sizeof(version) - 1 || shaLength != 64 || !isValidOTAHash(sha))
        return FirmnginOtaManifestResult::Invalid;
    out.id = id;
    out.version = version;
    out.sha256 = sha;
    return FirmnginOtaManifestResult::Available;
}

inline bool firmngin_ota_manifest_path(const String &board, const String &model,
                                       const String &version, String &path)
{
    char query[512] = {};
    size_t length = 0;
    if ((board.length() && !appendOTAQueryParameter(query, sizeof(query), length, "target_board", board)) ||
        (model.length() && !appendOTAQueryParameter(query, sizeof(query), length, "target_model", model)) ||
        !appendOTAQueryParameter(query, sizeof(query), length, "current_version", version))
        return false;
    path = String("/manifest?") + query;
    return true;
}

// True once no OTA byte arrived for longer than the stall budget. Unsigned
// subtraction is wrap-safe against millis()/esp_timer tick overflow.
inline bool otaStallExceeded(unsigned long nowMs, unsigned long lastByteMs, unsigned long budgetMs)
{
    return nowMs - lastByteMs > budgetMs;
}

#endif // FIRMNGIN_OTA_MANIFEST_H
