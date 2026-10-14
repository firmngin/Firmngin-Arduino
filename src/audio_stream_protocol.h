#ifndef FIRMNGIN_AUDIO_STREAM_PROTOCOL_H
#define FIRMNGIN_AUDIO_STREAM_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

// One frame on the wire: 16 kHz mono signed-16 little-endian PCM, 20 ms.
// The server writes exactly this many bytes per binary message.
#define AUDIO_SAMPLES_PER_FRAME 320
#define AUDIO_FRAME_BYTES (AUDIO_SAMPLES_PER_FRAME * 2)

// ESP32 standard-mode 16-bit mono reorders pairs of samples on TX. The library avoids
// that hardware-specific layout by expanding every wire sample to interleaved L/R
// before the chunk callback. The callback can write this frame straight to stereo I2S.
#define AUDIO_PLAYBACK_CHANNELS 2
#define AUDIO_PLAYBACK_FRAME_BYTES (AUDIO_FRAME_BYTES * AUDIO_PLAYBACK_CHANNELS)

// Longest entity_key / detail string carried by a control frame.
#define AUDIO_DETAIL_MAX 48

// Longest "<device_id>.<timestamp>.GET.<path>" signing string.
#define AUDIO_AUTH_MESSAGE_MAX 192

// Earliest epoch a stream connection may be signed with. A device without a synced
// clock would otherwise sign with 1970 and be refused as a stale replay.
#define AUDIO_MIN_VALID_EPOCH 1600000000UL

// Text control frames the server sends on the audio stream.
enum FirmnginAudioEvent
{
    AUDIO_NONE = 0,
    // The opening frame: the stream is live and this is the audio format.
    AUDIO_READY,
    // A newer clip replaces whatever is playing: discard software FIFO and built-in
    // I2S TX DMA. A custom chunk callback owns its hardware queue and clears it here.
    AUDIO_FLUSH,
    // A clip begins playing.
    AUDIO_START,
    // The server finished sending the clip. Buffered PCM may still be playing; do not
    // stop I2S until the device's own FIFO/DMA has drained.
    AUDIO_END,
    // The socket dropped. Playback stops immediately (ring + I2S cleared); the library
    // reconnects with a freshly signed request so a stale timestamp never locks the
    // device out.
    AUDIO_DISCONNECTED,
    // The gateway accepted an audio_input capture and is consuming PCM frames.
    AUDIO_INPUT_ACCEPTED,
    // The capture was finalized and saved.
    AUDIO_INPUT_SAVED,
    // The capture was rejected or could not be persisted.
    AUDIO_INPUT_ERROR,
    // The stream could not be opened or failed. `detail` says why.
    AUDIO_ERROR,
};

// One event as it travels from the socket task to loop(), where the user callback
// runs. Copyable and fixed-size so it can sit in a FreeRTOS queue.
struct FirmnginAudioEventMessage
{
    FirmnginAudioEvent event;
    // entity_key of the control frame that raised this event (empty when it carries
    // none): the router that picks the ON_AUDIO handler for the clip.
    char entity_key[AUDIO_DETAIL_MAX];
    char detail[AUDIO_DETAIL_MAX];
};

// Maps one text control frame to an event, copying the frame's entity_key into
// entity_key when it carries one (empty string otherwise).
//
// An unknown or malformed frame answers AUDIO_NONE: a later server may add
// a control type this library does not know, and that must not disturb playback.
FirmnginAudioEvent firmngin_audio_control_event(const char *payload, size_t len,
                                               char *entity_key, size_t entity_key_len);

// Expands little-endian signed-16 mono PCM into interleaved L/R without decoding or
// changing the samples. Returns the stereo byte count, or 0 for invalid buffers.
size_t firmngin_audio_mono_s16le_to_stereo(const uint8_t *mono, size_t mono_len,
                                           uint8_t *stereo, size_t stereo_len);

// Selects one slot from interleaved stereo signed-32 I2S input and converts the
// INMP441's MSB-aligned 24-bit sample to the mono signed-16 wire format.
size_t firmngin_audio_inmp441_to_mono_s16le(const uint8_t *stereo32, size_t stereo32_len,
                                            bool right_channel,
                                            uint8_t *mono16, size_t mono16_len);

// Scales every little-endian signed-16 sample by gain_num/gain_den (<= 1). Used to fade
// a stream's queued tail toward silence, so a stop never clicks mid-waveform.
void firmngin_audio_fade_s16le(uint8_t *pcm, size_t len, uint32_t gain_num, uint32_t gain_den);

// Continuous cosine fade-out across a PCM span. `pos` is the sample index within a
// fade of `total` samples (updated in place). Last sample lands on exact silence.
void firmngin_audio_fade_out_s16le(uint8_t *pcm, size_t len, uint32_t *pos, uint32_t total);

// Answers false when an input is missing or the result would not fit, so a caller
// never signs a truncated message.
bool firmngin_audio_ws_auth_message(const char *device_id, const char *timestamp, const char *path,
                                   char *out, size_t out_len);

bool firmngin_audio_stream_id(const char *device_id, const char *device_key,
                              char *out, size_t out_len);

#endif // FIRMNGIN_AUDIO_STREAM_PROTOCOL_H
