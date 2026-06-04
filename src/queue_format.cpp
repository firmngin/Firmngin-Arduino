#include "queue_format.h"

#include <string.h>

bool firmngin_queue_encode_header(uint8_t *hdr, size_t hdr_len, uint16_t head, uint16_t tail, uint16_t count)
{
    if (hdr == nullptr || hdr_len < FIRMNGIN_QUEUE_FMT_HEADER_SIZE)
        return false;

    memset(hdr, 0, FIRMNGIN_QUEUE_FMT_HEADER_SIZE);
    hdr[0] = FIRMNGIN_QUEUE_FMT_MAGIC & 0xFF;
    hdr[1] = (FIRMNGIN_QUEUE_FMT_MAGIC >> 8) & 0xFF;
    hdr[2] = (FIRMNGIN_QUEUE_FMT_MAGIC >> 16) & 0xFF;
    hdr[3] = (FIRMNGIN_QUEUE_FMT_MAGIC >> 24) & 0xFF;
    hdr[4] = FIRMNGIN_QUEUE_FMT_VERSION & 0xFF;
    hdr[5] = (FIRMNGIN_QUEUE_FMT_VERSION >> 8) & 0xFF;
    hdr[8] = head & 0xFF;
    hdr[9] = (head >> 8) & 0xFF;
    hdr[10] = tail & 0xFF;
    hdr[11] = (tail >> 8) & 0xFF;
    hdr[12] = count & 0xFF;
    hdr[13] = (count >> 8) & 0xFF;
    return true;
}

bool firmngin_queue_decode_header(const uint8_t *hdr, size_t hdr_len, uint16_t capacity,
                                  uint16_t *head, uint16_t *tail, uint16_t *count)
{
    if (hdr == nullptr || hdr_len < FIRMNGIN_QUEUE_FMT_HEADER_SIZE || head == nullptr || tail == nullptr || count == nullptr)
        return false;

    uint32_t magic = (uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8) | ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24);
    if (magic != FIRMNGIN_QUEUE_FMT_MAGIC)
        return false;

    uint16_t version = (uint16_t)hdr[4] | ((uint16_t)hdr[5] << 8);
    if (version != FIRMNGIN_QUEUE_FMT_VERSION)
        return false;

    *head = (uint16_t)hdr[8] | ((uint16_t)hdr[9] << 8);
    *tail = (uint16_t)hdr[10] | ((uint16_t)hdr[11] << 8);
    *count = (uint16_t)hdr[12] | ((uint16_t)hdr[13] << 8);

    if (*head >= capacity || *tail >= capacity || *count > capacity)
        return false;

    return true;
}

size_t firmngin_queue_record_file_offset(uint16_t index, size_t header_size, size_t record_size)
{
    return header_size + (size_t)index * record_size;
}

bool firmngin_queue_pack_record(uint8_t *record, size_t record_size, const char *topic,
                                const uint8_t *payload, size_t payload_len, bool retained)
{
    if (record == nullptr || record_size < FIRMNGIN_QUEUE_FMT_RECORD_SIZE || topic == nullptr || payload == nullptr)
        return false;

    size_t topic_len = strnlen(topic, FIRMNGIN_QUEUE_FMT_TOPIC_MAX + 1);
    if (topic_len == 0 || topic_len > FIRMNGIN_QUEUE_FMT_TOPIC_MAX || payload_len > FIRMNGIN_QUEUE_FMT_PAYLOAD_MAX)
        return false;

    memset(record, 0, FIRMNGIN_QUEUE_FMT_RECORD_SIZE);
    record[0] = (uint8_t)(topic_len & 0xFF);
    record[1] = (uint8_t)((topic_len >> 8) & 0xFF);
    record[2] = (uint8_t)(payload_len & 0xFF);
    record[3] = (uint8_t)((payload_len >> 8) & 0xFF);
    record[4] = retained ? 1 : 0;
    memcpy(record + 8, topic, topic_len);
    memcpy(record + 8 + FIRMNGIN_QUEUE_FMT_TOPIC_MAX, payload, payload_len);
    return true;
}

bool firmngin_queue_unpack_record(const uint8_t *record, size_t record_size,
                                  char *topic_out, size_t topic_out_len,
                                  char *payload_out, size_t payload_out_len,
                                  uint16_t *payload_len, bool *retained)
{
    if (record == nullptr || record_size < FIRMNGIN_QUEUE_FMT_RECORD_SIZE ||
        topic_out == nullptr || payload_out == nullptr || payload_len == nullptr || retained == nullptr)
        return false;

    uint16_t topic_len = (uint16_t)record[0] | ((uint16_t)record[1] << 8);
    uint16_t plen = (uint16_t)record[2] | ((uint16_t)record[3] << 8);
    *retained = record[4] != 0;

    if (topic_len == 0 || topic_len > FIRMNGIN_QUEUE_FMT_TOPIC_MAX || plen > FIRMNGIN_QUEUE_FMT_PAYLOAD_MAX)
        return false;
    if (topic_out_len < (size_t)topic_len + 1 || payload_out_len < (size_t)plen + 1)
        return false;

    memcpy(topic_out, record + 8, topic_len);
    topic_out[topic_len] = '\0';
    memcpy(payload_out, record + 8 + FIRMNGIN_QUEUE_FMT_TOPIC_MAX, plen);
    payload_out[plen] = '\0';
    *payload_len = plen;
    return true;
}
