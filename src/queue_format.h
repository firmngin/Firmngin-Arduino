#ifndef FIRMNGIN_QUEUE_FORMAT_H
#define FIRMNGIN_QUEUE_FORMAT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define FIRMNGIN_QUEUE_FMT_MAGIC 0x464E5151U
#define FIRMNGIN_QUEUE_FMT_VERSION 1
#define FIRMNGIN_QUEUE_FMT_HEADER_SIZE 16
#define FIRMNGIN_QUEUE_FMT_RECORD_SIZE 256
#define FIRMNGIN_QUEUE_FMT_TOPIC_MAX 96
#define FIRMNGIN_QUEUE_FMT_PAYLOAD_MAX 140

bool firmngin_queue_encode_header(uint8_t *hdr, size_t hdr_len, uint16_t head, uint16_t tail, uint16_t count);
bool firmngin_queue_decode_header(const uint8_t *hdr, size_t hdr_len, uint16_t capacity,
                                  uint16_t *head, uint16_t *tail, uint16_t *count);
size_t firmngin_queue_record_file_offset(uint16_t index, size_t header_size, size_t record_size);
bool firmngin_queue_pack_record(uint8_t *record, size_t record_size, const char *topic,
                                const uint8_t *payload, size_t payload_len, bool retained);
bool firmngin_queue_unpack_record(const uint8_t *record, size_t record_size,
                                  char *topic_out, size_t topic_out_len,
                                  char *payload_out, size_t payload_out_len,
                                  uint16_t *payload_len, bool *retained);

#endif
