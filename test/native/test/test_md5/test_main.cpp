#include <unity.h>

#include <string.h>

#include "md5.h"

static const char *md5_of(const char *text)
{
    static char out[33];
    firmngin_md5_hex((const uint8_t *)text, strlen(text), out, sizeof(out));
    return out;
}

// RFC 1321 test suite. The stream id the device derives must equal the one the
// server derives, so the digest has to be exact, not merely checksum-shaped.
void test_md5_rfc1321_vectors()
{
    TEST_ASSERT_EQUAL_STRING("d41d8cd98f00b204e9800998ecf8427e", md5_of(""));
    TEST_ASSERT_EQUAL_STRING("0cc175b9c0f1b6a831c399e269772661", md5_of("a"));
    TEST_ASSERT_EQUAL_STRING("900150983cd24fb0d6963f7d28e17f72", md5_of("abc"));
    TEST_ASSERT_EQUAL_STRING("f96b697d7cb7938d525a2f31aaf161d0", md5_of("message digest"));
    TEST_ASSERT_EQUAL_STRING("c3fcd3d76192e4007dfb496cca67e13b", md5_of("abcdefghijklmnopqrstuvwxyz"));
    TEST_ASSERT_EQUAL_STRING("d174ab98d277d9f5a5611c2c9f419d9f",
                             md5_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"));
    TEST_ASSERT_EQUAL_STRING("57edf4a22be3c955ac49da2e2107b67a",
                             md5_of("12345678901234567890123456789012345678901234567890123456789012345678901234567890"));
}

// The padding rule changes at 56 bytes inside the final block: 64 bytes exactly is
// the case a hand-rolled digest usually gets wrong.
void test_md5_padding_boundaries()
{
    TEST_ASSERT_EQUAL_STRING("eb6c4179c0a7c82cc2828c1e6338e165",
                             md5_of("1234567890123456789012345678901234567890123456789012345678901234"));
}

void test_md5_hashes_a_device_id()
{
    // md5(lower(id) + "\x1f" + key): pinned digests shared with the platform tests.
    TEST_ASSERT_EQUAL_STRING("4e05568fae831282a530381f93d013cc",
                             md5_of("fngin_device_a" "\x1f" "fngin_key_a"));
    TEST_ASSERT_EQUAL_STRING("1ddbebc8e02bd748236af35ae8dd09b0",
                             md5_of("fngin_3f2504e0_4f89_11d3_9a0c" "\x1f" "fngin_key_b"));
}

void test_md5_refuses_a_short_buffer()
{
    char small[8];
    memset(small, 'x', sizeof(small));
    firmngin_md5_hex((const uint8_t *)"abc", 3, small, sizeof(small));
    TEST_ASSERT_EQUAL_CHAR('\0', small[0]);

    // A null buffer must not be written to.
    firmngin_md5_hex((const uint8_t *)"abc", 3, nullptr, 33);
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_md5_rfc1321_vectors);
    RUN_TEST(test_md5_padding_boundaries);
    RUN_TEST(test_md5_hashes_a_device_id);
    RUN_TEST(test_md5_refuses_a_short_buffer);
    return UNITY_END();
}
