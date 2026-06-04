#include "hmac.h"

#include <string.h>

#if defined(ESP32)
#include <mbedtls/md.h>
#elif defined(ESP8266)
#include <bearssl/bearssl_hash.h>
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

#elif defined(ESP8266)
    uint8_t key_block[64];
    memset(key_block, 0, sizeof(key_block));
    size_t key_len = strlen(key);
    if (key_len > 64)
    {
        br_sha256_context sha;
        br_sha256_init(&sha);
        br_sha256_update(&sha, key, key_len);
        br_sha256_out(&sha, key_block);
        key_len = 32;
    }
    else
    {
        memcpy(key_block, key, key_len);
    }

    uint8_t o_key_pad[64];
    uint8_t i_key_pad[64];
    for (int i = 0; i < 64; i++)
    {
        o_key_pad[i] = key_block[i] ^ 0x5c;
        i_key_pad[i] = key_block[i] ^ 0x36;
    }

    br_sha256_context sha;
    uint8_t inner_hash[32];
    br_sha256_init(&sha);
    br_sha256_update(&sha, i_key_pad, 64);
    br_sha256_update(&sha, message, strlen(message));
    br_sha256_out(&sha, inner_hash);

    uint8_t hmac_result[32];
    br_sha256_init(&sha);
    br_sha256_update(&sha, o_key_pad, 64);
    br_sha256_update(&sha, inner_hash, 32);
    br_sha256_out(&sha, hmac_result);

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
