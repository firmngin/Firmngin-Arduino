#include <unity.h>

#include <string.h>

#include "audio_stream_protocol.h"

void test_control_event_maps_known_types()
{
    char key[AUDIO_DETAIL_MAX];

    // The exact frames the gateway sends, captured off a live stream. The ready
    // frame is marshalled from a map, so its keys are sorted and `type` is last:
    // the parser must find a key anywhere, not by position.
    const char *ready = "{\"channels\":1,\"codec\":\"pcm_s16le\",\"frame_bytes\":640,\"frame_ms\":20,\"sample_rate\":16000,\"stream_id\":\"5686455a735075574a5c7a959c25c3fd\",\"type\":\"ready\"}";
    TEST_ASSERT_EQUAL_INT(AUDIO_READY,
                          firmngin_audio_control_event(ready, strlen(ready), key, sizeof(key)));
    TEST_ASSERT_EQUAL_INT(AUDIO_FLUSH,
                          firmngin_audio_control_event("{\"type\":\"audio.flush\",\"stream_id\":\"5686455a735075574a5c7a959c25c3fd\"}", 68, key, sizeof(key)));
    TEST_ASSERT_EQUAL_INT(AUDIO_END,
                          firmngin_audio_control_event("{\"type\":\"audio.end\",\"stream_id\":\"abc\"}", 38, key, sizeof(key)));
}

void test_control_event_reads_the_entity_key()
{
    const char *frame = "{\"type\":\"audio.start\",\"stream_id\":\"5686455a735075574a5c7a959c25c3fd\",\"entity_key\":\"speaker\",\"execution_id\":\"exec-1\",\"codec\":\"pcm_s16le\",\"sample_rate\":16000,\"channels\":1,\"frame_ms\":20,\"frame_bytes\":640}";
    char key[AUDIO_DETAIL_MAX];

    TEST_ASSERT_EQUAL_INT(AUDIO_START,
                          firmngin_audio_control_event(frame, strlen(frame), key, sizeof(key)));
    TEST_ASSERT_EQUAL_STRING("speaker", key);

    // A frame without one leaves the detail empty rather than reusing an old value.
    const char *flush = "{\"type\":\"audio.flush\",\"stream_id\":\"abc\"}";
    strcpy(key, "stale");
    TEST_ASSERT_EQUAL_INT(AUDIO_FLUSH,
                          firmngin_audio_control_event(flush, strlen(flush), key, sizeof(key)));
    TEST_ASSERT_EQUAL_STRING("", key);
}

void test_control_event_maps_audio_input_status()
{
    char key[AUDIO_DETAIL_MAX];
    const char *accepted = "{\"type\":\"audio.input.accepted\",\"entity_key\":\"microphone\"}";
    const char *saved = "{\"type\":\"audio.input.saved\",\"entity_key\":\"microphone\"}";
    const char *failed = "{\"type\":\"audio.input.error\",\"entity_key\":\"microphone\"}";

    TEST_ASSERT_EQUAL_INT(AUDIO_INPUT_ACCEPTED,
                          firmngin_audio_control_event(accepted, strlen(accepted), key, sizeof(key)));
    TEST_ASSERT_EQUAL_STRING("microphone", key);
    TEST_ASSERT_EQUAL_INT(AUDIO_INPUT_SAVED,
                          firmngin_audio_control_event(saved, strlen(saved), key, sizeof(key)));
    TEST_ASSERT_EQUAL_INT(AUDIO_INPUT_ERROR,
                          firmngin_audio_control_event(failed, strlen(failed), key, sizeof(key)));
}

// A newer server may add a control type. It must be ignored, not treated as an
// error and not allowed to disturb playback.
void test_control_event_ignores_unknown_and_malformed_frames()
{
    char key[AUDIO_DETAIL_MAX];

    TEST_ASSERT_EQUAL_INT(AUDIO_NONE,
                          firmngin_audio_control_event("{\"type\":\"audio.mute\"}", 20, key, sizeof(key)));
    TEST_ASSERT_EQUAL_INT(AUDIO_NONE,
                          firmngin_audio_control_event("not json at all", 15, key, sizeof(key)));
    TEST_ASSERT_EQUAL_INT(AUDIO_NONE,
                          firmngin_audio_control_event("{\"stream_id\":\"abc\"}", 18, key, sizeof(key)));
    TEST_ASSERT_EQUAL_INT(AUDIO_NONE, firmngin_audio_control_event("", 0, key, sizeof(key)));
    TEST_ASSERT_EQUAL_INT(AUDIO_NONE, firmngin_audio_control_event(nullptr, 0, key, sizeof(key)));

    // "audio.start" must not be answered by a type that merely starts with it.
    TEST_ASSERT_EQUAL_INT(AUDIO_NONE,
                          firmngin_audio_control_event("{\"type\":\"audio.starting\"}", 23, key, sizeof(key)));
}

void test_ws_auth_message_matches_the_platform()
{
    char message[AUDIO_AUTH_MESSAGE_MAX];

    TEST_ASSERT_TRUE(firmngin_audio_ws_auth_message("fngin_device_a", "1700000000",
                                                    "/v1/stream/4e05568fae831282a530381f93d013cc",
                                                    message, sizeof(message)));
    TEST_ASSERT_EQUAL_STRING("fngin_device_a.1700000000.GET./v1/stream/4e05568fae831282a530381f93d013cc", message);

    // Every part is required, and a message that would not fit is refused outright
    // rather than signed truncated.
    TEST_ASSERT_FALSE(firmngin_audio_ws_auth_message("", "1700000000", "/v1/stream/x", message, sizeof(message)));
    TEST_ASSERT_FALSE(firmngin_audio_ws_auth_message("fngin_a", "", "/v1/stream/x", message, sizeof(message)));
    TEST_ASSERT_FALSE(firmngin_audio_ws_auth_message("fngin_a", "1700000000", "", message, sizeof(message)));
    char tiny[16];
    TEST_ASSERT_FALSE(firmngin_audio_ws_auth_message("fngin_device_a", "1700000000", "/v1/stream/x", tiny, sizeof(tiny)));
    TEST_ASSERT_EQUAL_CHAR('\0', tiny[0]);
}

void test_frame_size_matches_the_server()
{
    // The gateway writes 640-byte PCM messages (16 kHz mono 20 ms). The callback gets
    // the same 320 samples duplicated to L/R, ready for ESP32 stereo I2S.
    TEST_ASSERT_EQUAL_UINT32(320, AUDIO_SAMPLES_PER_FRAME);
    TEST_ASSERT_EQUAL_UINT32(640, AUDIO_FRAME_BYTES);
    TEST_ASSERT_EQUAL_UINT32(1280, AUDIO_PLAYBACK_FRAME_BYTES);
}

void test_mono_pcm_expands_to_interleaved_stereo()
{
    const uint8_t mono[] = {0x34, 0x12, 0x78, 0x56};
    uint8_t stereo[8] = {};
    const uint8_t expected[] = {0x34, 0x12, 0x34, 0x12, 0x78, 0x56, 0x78, 0x56};

    TEST_ASSERT_EQUAL_UINT32(sizeof(stereo),
                             firmngin_audio_mono_s16le_to_stereo(mono, sizeof(mono), stereo, sizeof(stereo)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, stereo, sizeof(expected));

    TEST_ASSERT_EQUAL_UINT32(0, firmngin_audio_mono_s16le_to_stereo(nullptr, sizeof(mono), stereo, sizeof(stereo)));
    TEST_ASSERT_EQUAL_UINT32(0, firmngin_audio_mono_s16le_to_stereo(mono, 3, stereo, sizeof(stereo)));
    TEST_ASSERT_EQUAL_UINT32(0, firmngin_audio_mono_s16le_to_stereo(mono, sizeof(mono), stereo, 7));
}

void test_inmp441_stereo_slots_convert_to_mono_s16le()
{
    // Two stereo frames. INMP441's 24-bit value is MSB-aligned in a 32-bit slot.
    const uint8_t stereo32[] = {
        0x00, 0x00, 0x34, 0x12, 0x00, 0x00, 0xDC, 0xFE,
        0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0xFF, 0x7F,
    };
    uint8_t mono[4] = {};
    const uint8_t expectedLeft[] = {0x34, 0x12, 0x00, 0x80};
    const uint8_t expectedRight[] = {0xDC, 0xFE, 0xFF, 0x7F};

    TEST_ASSERT_EQUAL_UINT32(sizeof(mono), firmngin_audio_inmp441_to_mono_s16le(
                                               stereo32, sizeof(stereo32), false, mono, sizeof(mono)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedLeft, mono, sizeof(mono));
    TEST_ASSERT_EQUAL_UINT32(sizeof(mono), firmngin_audio_inmp441_to_mono_s16le(
                                               stereo32, sizeof(stereo32), true, mono, sizeof(mono)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedRight, mono, sizeof(mono));
    TEST_ASSERT_EQUAL_UINT32(0, firmngin_audio_inmp441_to_mono_s16le(
                                    stereo32, sizeof(stereo32) - 1, false, mono, sizeof(mono)));
}

void test_fade_scales_samples_toward_silence()
{
    uint8_t pcm[] = {0x00, 0x10, 0x00, 0xF0}; // +4096, -4096

    firmngin_audio_fade_s16le(pcm, sizeof(pcm), 1, 2);
    const uint8_t half[] = {0x00, 0x08, 0x00, 0xF8}; // +2048, -2048
    TEST_ASSERT_EQUAL_UINT8_ARRAY(half, pcm, sizeof(half));

    // Zero gain must land exactly on silence.
    uint8_t loud[] = {0xCD, 0xAB, 0x34, 0x12};
    firmngin_audio_fade_s16le(loud, sizeof(loud), 0, 4);
    const uint8_t silence[] = {0, 0, 0, 0};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(silence, loud, sizeof(silence));

    // Full gain is refused (no-op): fading only ever goes down.
    uint8_t keep[] = {0x34, 0x12};
    firmngin_audio_fade_s16le(keep, sizeof(keep), 1, 1);
    TEST_ASSERT_EQUAL_UINT8(0x34, keep[0]);
    TEST_ASSERT_EQUAL_UINT8(0x12, keep[1]);

    // Odd trailing byte is left alone rather than read out of bounds.
    uint8_t odd[] = {0x34, 0x12, 0xFF};
    firmngin_audio_fade_s16le(odd, sizeof(odd), 0, 2);
    TEST_ASSERT_EQUAL_UINT8(0x00, odd[0]);
    TEST_ASSERT_EQUAL_UINT8(0x00, odd[1]);
    TEST_ASSERT_EQUAL_UINT8(0xFF, odd[2]);
}

void test_stream_id_matches_platform_normalization()
{
    char id[33];

    TEST_ASSERT_TRUE(firmngin_audio_stream_id("fngin_device_a", "fngin_key_a", id, sizeof(id)));
    TEST_ASSERT_EQUAL_STRING("4e05568fae831282a530381f93d013cc", id);

    // Platform lowercases the id and trims both sides; key case is preserved.
    TEST_ASSERT_TRUE(firmngin_audio_stream_id("FNGIN_DEVICE_A", "fngin_key_a", id, sizeof(id)));
    TEST_ASSERT_EQUAL_STRING("4e05568fae831282a530381f93d013cc", id);
    TEST_ASSERT_TRUE(firmngin_audio_stream_id("  fngin_3f2504e0_4f89_11d3_9a0c  ",
                                              "  fngin_key_b  ", id, sizeof(id)));
    TEST_ASSERT_EQUAL_STRING("1ddbebc8e02bd748236af35ae8dd09b0", id);

    TEST_ASSERT_FALSE(firmngin_audio_stream_id("", "fngin_key_a", id, sizeof(id)));
    TEST_ASSERT_FALSE(firmngin_audio_stream_id("fngin_device_a", "", id, sizeof(id)));
    char tiny[8];
    TEST_ASSERT_FALSE(firmngin_audio_stream_id("fngin_device_a", "fngin_key_a", tiny, sizeof(tiny)));
}

void test_fade_out_reaches_silence_continuously()
{
    // 4 samples: cosine fade must end on exact 0 and not jump in one step.
    uint8_t pcm[] = {0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10};
    uint32_t pos = 0;
    firmngin_audio_fade_out_s16le(pcm, sizeof(pcm), &pos, 4);
    TEST_ASSERT_EQUAL_UINT32(4, pos);
    TEST_ASSERT_EQUAL_UINT8(0x00, pcm[6]);
    TEST_ASSERT_EQUAL_UINT8(0x00, pcm[7]);
    // First sample stays near full level (cosine starts at 1).
    TEST_ASSERT_EQUAL_UINT8(0x00, pcm[0]);
    TEST_ASSERT_EQUAL_UINT8(0x10, pcm[1]);
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_control_event_maps_known_types);
    RUN_TEST(test_control_event_reads_the_entity_key);
    RUN_TEST(test_control_event_maps_audio_input_status);
    RUN_TEST(test_control_event_ignores_unknown_and_malformed_frames);
    RUN_TEST(test_ws_auth_message_matches_the_platform);
    RUN_TEST(test_frame_size_matches_the_server);
    RUN_TEST(test_mono_pcm_expands_to_interleaved_stereo);
    RUN_TEST(test_inmp441_stereo_slots_convert_to_mono_s16le);
    RUN_TEST(test_fade_scales_samples_toward_silence);
    RUN_TEST(test_fade_out_reaches_silence_continuously);
    RUN_TEST(test_stream_id_matches_platform_normalization);
    return UNITY_END();
}
