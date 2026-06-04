#include "crypto.h"

#include <Arduino.h>

#if defined(ESP32)
#include <esp_system.h>
#include <mbedtls/gcm.h>
#elif defined(ESP8266)
#include <ChaChaPoly.h>
extern "C"
{
#include "user_interface.h"
}
#endif

void firmngin_fill_e2ee_nonce(uint8_t *iv, size_t len)
{
#if defined(ESP32)
    esp_fill_random(iv, len);
#elif defined(ESP8266)
    if (!os_get_random(iv, len))
    {
        for (size_t i = 0; i < len; i++)
            iv[i] = (uint8_t)random(0, 256);
    }
#else
    for (size_t i = 0; i < len; i++)
        iv[i] = (uint8_t)random(0, 256);
#endif
}

bool firmngin_encrypt_e2ee(const uint8_t *plaintext, size_t plain_len, const uint8_t *key, size_t key_len,
                           uint8_t *packet, size_t *packet_len)
{
    *packet_len = 0;

    uint8_t *iv = packet;
    uint8_t *ciphertext = packet + 12;
    uint8_t *tag = packet + 12 + plain_len;
    firmngin_fill_e2ee_nonce(iv, 12);

#if defined(ESP32)
    if (key_len != 16 && key_len != 32)
        return false;

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);

    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, key_len == 16 ? 128 : 256);
    if (ret != 0)
    {
        mbedtls_gcm_free(&gcm);
        return false;
    }

    ret = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, plain_len, iv, 12, NULL, 0, plaintext, ciphertext, 16, tag);
    mbedtls_gcm_free(&gcm);

    if (ret != 0)
        return false;

    *packet_len = 12 + plain_len + 16;
    return true;

#elif defined(ESP8266)
    if (key_len != 32)
        return false;

    ChaChaPoly chacha;
    if (!chacha.setKey(key, 32))
        return false;
    if (!chacha.setIV(iv, 12))
        return false;

    chacha.encrypt(ciphertext, plaintext, plain_len);
    chacha.computeTag(tag, 16);

    *packet_len = 12 + plain_len + 16;
    return true;
#else
    (void)plaintext;
    (void)plain_len;
    (void)key;
    (void)key_len;
    (void)packet;
    return false;
#endif
}

bool firmngin_decrypt_e2ee(const uint8_t *packet, size_t packet_len, const uint8_t *key, size_t key_len,
                           uint8_t *plaintext, size_t *plain_len)
{
    *plain_len = 0;

    if (packet_len < 28)
        return false;

    const uint8_t *iv = packet;
    size_t cipher_len = packet_len - 12 - 16;
    const uint8_t *ciphertext = packet + 12;
    const uint8_t *tag = packet + 12 + cipher_len;

#if defined(ESP32)
    if (key_len == 16)
    {
        mbedtls_gcm_context gcm;
        mbedtls_gcm_init(&gcm);

        int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 128);
        if (ret != 0)
        {
            mbedtls_gcm_free(&gcm);
            return false;
        }

        ret = mbedtls_gcm_auth_decrypt(&gcm, cipher_len, iv, 12, NULL, 0, tag, 16, ciphertext, plaintext);
        mbedtls_gcm_free(&gcm);

        if (ret != 0)
            return false;

        *plain_len = cipher_len;
        return true;
    }
    if (key_len == 32)
    {
        mbedtls_gcm_context gcm;
        mbedtls_gcm_init(&gcm);

        int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
        if (ret != 0)
        {
            mbedtls_gcm_free(&gcm);
            return false;
        }

        ret = mbedtls_gcm_auth_decrypt(&gcm, cipher_len, iv, 12, NULL, 0, tag, 16, ciphertext, plaintext);
        mbedtls_gcm_free(&gcm);

        if (ret != 0)
            return false;

        *plain_len = cipher_len;
        return true;
    }

    return false;

#elif defined(ESP8266)
    if (key_len != 32)
        return false;

    ChaChaPoly chacha;
    if (!chacha.setKey(key, 32))
        return false;
    if (!chacha.setIV(iv, 12))
        return false;

    chacha.decrypt(plaintext, ciphertext, cipher_len);

    uint8_t computed_tag[16];
    chacha.computeTag(computed_tag, 16);

    if (!chacha.checkTag(tag, 16))
    {
        memset(plaintext, 0, cipher_len);
        return false;
    }

    *plain_len = cipher_len;
    return true;
#else
    (void)iv;
    (void)ciphertext;
    (void)tag;
    return false;
#endif
}
