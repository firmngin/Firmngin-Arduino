#ifndef FIRMNGIN_CRYPTO_H
#define FIRMNGIN_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

void firmngin_fill_e2ee_nonce(uint8_t *iv, size_t len);
bool firmngin_encrypt_e2ee(const uint8_t *plaintext, size_t plain_len, const uint8_t *key, size_t key_len,
                           uint8_t *packet, size_t *packet_len);
bool firmngin_decrypt_e2ee(const uint8_t *packet, size_t packet_len, const uint8_t *key, size_t key_len,
                           uint8_t *plaintext, size_t *plain_len);

#endif
