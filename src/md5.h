#ifndef FIRMNGIN_MD5_H
#define FIRMNGIN_MD5_H

#include <stddef.h>
#include <stdint.h>

void firmngin_md5_hex(const uint8_t *data, size_t len, char *out, size_t out_len);

#endif // FIRMNGIN_MD5_H
