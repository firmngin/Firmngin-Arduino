#ifndef FIRMNGIN_IDENTITY_META_H
#define FIRMNGIN_IDENTITY_META_H

// Pure provisioning-meta parsing/validation shared by the ESP build and the
// native (host) unit tests. The meta.txt format is "key=value" lines.

#include "fng_compat.h"
#include "firmngin_identity.h"

inline String trimLine(const String &line)
{
    return fng_trim(line);
}

inline bool isCertificatePEM(const String &content)
{
    return fng_indexOf(content, "-----BEGIN CERTIFICATE-----") >= 0 &&
           fng_indexOf(content, "-----END CERTIFICATE-----") >= 0;
}

inline bool isPrivateKeyPEM(const String &content)
{
    return (fng_indexOf(content, "-----BEGIN PRIVATE KEY-----") >= 0 &&
            fng_indexOf(content, "-----END PRIVATE KEY-----") >= 0) ||
           (fng_indexOf(content, "-----BEGIN RSA PRIVATE KEY-----") >= 0 &&
            fng_indexOf(content, "-----END RSA PRIVATE KEY-----") >= 0) ||
           (fng_indexOf(content, "-----BEGIN EC PRIVATE KEY-----") >= 0 &&
            fng_indexOf(content, "-----END EC PRIVATE KEY-----") >= 0);
}

inline bool isHexValue(const String &value, size_t requiredLength)
{
    if (value.length() != requiredLength)
        return false;
    for (unsigned int i = 0; i < value.length(); i++)
    {
        const char c = fng_charAt(value, i);
        if (!((c >= '0' && c <= '9') ||
              (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F')))
            return false;
    }
    return true;
}

inline bool splitMetaLine(const String &line, String &key, String &value)
{
    int idx = fng_indexOf(line, '=');
    if (idx <= 0)
        return false;
    key = fng_trim(fng_substring(line, 0, idx));
    value = fng_trim(fng_substring(line, idx + 1));
    return key.length() > 0;
}

inline bool isValidHttpsBaseUrl(const String &value)
{
    if (!fng_startsWith(value, "https://") || value.length() <= 8 || fng_endsWith(value, "/"))
        return false;

    int authorityEnd = fng_indexOf(value, '/', 8);
    String authority = authorityEnd >= 0 ? fng_substring(value, 8, authorityEnd) : fng_substring(value, 8);
    if (authority.length() == 0 || fng_indexOf(authority, '@') >= 0)
        return false;

    for (unsigned int i = 0; i < value.length(); i++)
    {
        const char c = fng_charAt(value, i);
        if (c <= ' ' || c == '?' || c == '#')
            return false;
    }
    return true;
}

inline bool isValidStreamEndpoint(const String &value)
{
    // Validation only: recognize both secure and plain schemes supplied by provisioning.
    const bool secure = fng_isSecureWsScheme(value) || fng_startsWith(value, "https://");
    const bool plain = fng_isPlainWsScheme(value) || fng_startsWith(value, "http://");
    if (!secure && !plain)
        return false;

    const int schemeEnd = fng_indexOf(value, "://");
    const unsigned int authorityStart = static_cast<unsigned int>(schemeEnd + 3);
    if (value.length() <= authorityStart)
        return false;

    for (unsigned int i = 0; i < value.length(); i++)
    {
        const char c = fng_charAt(value, i);
        if (c <= ' ' || c == '?' || c == '#')
            return false;
    }

    const int pathStart = fng_indexOf(value, "/", static_cast<int>(authorityStart));
    const String authority = pathStart >= 0
                                 ? fng_substring(value, static_cast<int>(authorityStart), pathStart)
                                 : fng_substring(value, static_cast<int>(authorityStart));
    return authority.length() > 0 && fng_indexOf(authority, '@') < 0;
}

inline bool findMetaValue(const String &meta, const char *wantedKey, String &out)
{
    int start = 0;
    while (start < static_cast<int>(meta.length()))
    {
        int end = fng_indexOf(meta, '\n', start);
        if (end < 0)
            end = static_cast<int>(meta.length());

        String line = trimLine(fng_substring(meta, start, end));
        String key;
        String value;
        if (splitMetaLine(line, key, value) && key == wantedKey)
        {
            out = value;
            return true;
        }
        start = end + 1;
    }
    out = "";
    return false;
}

inline bool parseMeta(const String &meta, FirmnginIdentityRecord &out)
{
    bool invalidServiceEndpoint = false;
    int start = 0;
    while (start < static_cast<int>(meta.length()))
    {
        int end = fng_indexOf(meta, '\n', start);
        if (end < 0)
            end = static_cast<int>(meta.length());

        String line = trimLine(fng_substring(meta, start, end));
        String key;
        String value;
        if (splitMetaLine(line, key, value))
        {
            if (key == "device_id")
                out.deviceId = value;
            else if (key == "device_key")
                out.deviceKey = value;
            else if (key == "decryptor")
                out.decryptor = value;
            else if (key == "tls")
                out.tlsMode = value;
            else if (key == "fingerprint")
                out.fingerprintHex = value;
            else if (key == "broker")
                out.brokerAddr = value;
            else if (key == "broker_port")
                out.brokerPort = fng_toInt(value);
            else if (key == "api_base_url")
            {
                if (isValidHttpsBaseUrl(value))
                    out.apiBaseUrl = value;
                else
                    invalidServiceEndpoint = true;
            }
            else if (key == "ota_base_url")
            {
                if (isValidHttpsBaseUrl(value))
                    out.otaBaseUrl = value;
                else
                    invalidServiceEndpoint = true;
            }
            else if (key == "audio_stream_url")
            {
                if (isValidStreamEndpoint(value))
                    out.audioStreamUrl = value;
                else
                    invalidServiceEndpoint = true;
            }
            else if (key == "audio_stream_token")
            {
                if (value.length() > 0)
                    out.audioStreamToken = value;
            }
        }
        start = end + 1;
    }
    return !invalidServiceEndpoint;
}

inline bool validateIdentityRecord(const FirmnginIdentityRecord &record, bool requireDecryptor = true)
{
    if (record.deviceId.length() == 0 || record.deviceKey.length() == 0)
        return false;
    if ((requireDecryptor || record.decryptor.length() > 0) &&
        !isHexValue(record.decryptor, 32) && !isHexValue(record.decryptor, 64))
        return false;
    if (record.tlsMode != "ca" && record.tlsMode != "fingerprint")
        return false;
    if (!isValidHttpsBaseUrl(record.apiBaseUrl) || !isValidHttpsBaseUrl(record.otaBaseUrl))
        return false;
    if (record.audioStreamUrl.length() > 0 && !isValidStreamEndpoint(record.audioStreamUrl))
        return false;
    if (!isCertificatePEM(record.clientCert) || !isPrivateKeyPEM(record.privateKey))
        return false;
    if (!isCertificatePEM(record.serviceCaCert))
        return false;
    if (record.tlsMode == "ca" && !isCertificatePEM(record.caCert))
        return false;
    if (record.tlsMode == "fingerprint" && !isHexValue(record.fingerprintHex, 40))
        return false;
    return true;
}

#endif // FIRMNGIN_IDENTITY_META_H
