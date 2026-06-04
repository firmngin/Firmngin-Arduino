#include <unity.h>

#include "queue_format.h"

void test_queue_header_roundtrip()
{
    uint8_t hdr[FIRMNGIN_QUEUE_FMT_HEADER_SIZE];
    TEST_ASSERT_TRUE(firmngin_queue_encode_header(hdr, sizeof(hdr), 2, 5, 3));

    uint16_t head = 0;
    uint16_t tail = 0;
    uint16_t count = 0;
    TEST_ASSERT_TRUE(firmngin_queue_decode_header(hdr, sizeof(hdr), 32, &head, &tail, &count));
    TEST_ASSERT_EQUAL_UINT16(2, head);
    TEST_ASSERT_EQUAL_UINT16(5, tail);
    TEST_ASSERT_EQUAL_UINT16(3, count);
}

void test_queue_record_roundtrip()
{
    uint8_t record[FIRMNGIN_QUEUE_FMT_RECORD_SIZE];
    const char *topic = "device/abc/entity";
    const uint8_t payload[] = {'{', '"', 'v', '"', ':', '1', '}'};

    TEST_ASSERT_TRUE(firmngin_queue_pack_record(record, sizeof(record), topic, payload, sizeof(payload), true));

    char topic_out[128];
    char payload_out[160];
    uint16_t payload_len = 0;
    bool retained = false;
    TEST_ASSERT_TRUE(firmngin_queue_unpack_record(record, sizeof(record), topic_out, sizeof(topic_out),
                                                   payload_out, sizeof(payload_out), &payload_len, &retained));
    TEST_ASSERT_EQUAL_STRING(topic, topic_out);
    TEST_ASSERT_EQUAL_UINT16(sizeof(payload), payload_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, (const uint8_t *)payload_out, sizeof(payload));
    TEST_ASSERT_TRUE(retained);
}

void test_queue_record_offset()
{
    TEST_ASSERT_EQUAL_UINT32(16 + (3 * 256), firmngin_queue_record_file_offset(3, 16, 256));
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_queue_header_roundtrip);
    RUN_TEST(test_queue_record_roundtrip);
    RUN_TEST(test_queue_record_offset);
    return UNITY_END();
}
