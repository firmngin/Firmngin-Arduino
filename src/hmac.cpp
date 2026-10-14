#include "hmac.h"

#include <string.h>

#if defined(ESP32)
#include <mbedtls/md.h>
#endif

String firmngin_hmac_sha256_hex(const char *key, const char *message)
{
#if defined(ESP32)
    uint8_t hmac_result[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&ctx, (const unsigned char *)key, strlen(key));
    mbedtls_md_hmac_update(&ctx, (const unsigned char *)message, strlen(message));
    mbedtls_md_hmac_finish(&ctx, hmac_result);
    mbedtls_md_free(&ctx);

    char hex[65];
    for (int i = 0; i < 32; i++)
        sprintf(hex + (i * 2), "%02x", hmac_result[i]);
    hex[64] = '\0';
    return String(hex);

#else
    (void)key;
    (void)message;
    return String("");
#endif
}
