#ifndef FIRMNGIN_HMAC_H
#define FIRMNGIN_HMAC_H

#include <Arduino.h>

String firmngin_hmac_sha256_hex(const char *key, const char *message);

#endif
