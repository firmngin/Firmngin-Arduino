#ifndef FIRMNGIN_HTTP_RESPONSE_H
#define FIRMNGIN_HTTP_RESPONSE_H

#include "fng_compat.h"

enum class FirmnginHttpBodyResult { Complete, TooLarge, Incomplete, NoMemory };

// Read only complete, bounded bodies. The reader returns a byte count, 0 for EOF,
// or a negative transport error; completion must come from the HTTP parser.
template <typename Read, typename Complete>
FirmnginHttpBodyResult firmngin_read_http_body(Read read, Complete complete,
                                             String &body, size_t limit)
{
    body = "";
    char buffer[512];
    int count;
    while ((count = read(buffer, sizeof(buffer))) > 0)
    {
        if (static_cast<size_t>(count) > sizeof(buffer) ||
            static_cast<size_t>(count) > limit - body.length())
        {
            body = "";
            return FirmnginHttpBodyResult::TooLarge;
        }
        if (!fng_append(body, buffer, static_cast<size_t>(count)))
        {
            body = "";
            return FirmnginHttpBodyResult::NoMemory;
        }
    }
    if (count < 0 || !complete())
    {
        body = "";
        return FirmnginHttpBodyResult::Incomplete;
    }
    return FirmnginHttpBodyResult::Complete;
}

// No global SDK security setting is enabled on the application's behalf.
inline const char *firmngin_http_security_error(bool insecure, bool hasCA,
                                               bool insecureSupported)
{
    if (insecure && !insecureSupported)
        return "Insecure HTTPS requires explicit SDK CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY; use a CA in production";
    if (!insecure && !hasCA)
        return "Service CA certificate is not configured";
    return nullptr;
}

#endif
