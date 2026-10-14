#include "audio_stream_protocol.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "json.h"
#include "md5.h"

namespace
{
    void trim_bounds(const char *in, const char **begin, const char **end)
    {
        if (in == nullptr)
        {
            *begin = "";
            *end = *begin;
            return;
        }
        const char *b = in;
        while (*b != '\0' && isspace(static_cast<unsigned char>(*b)))
            ++b;
        const char *e = b + strlen(b);
        while (e > b && isspace(static_cast<unsigned char>(e[-1])))
            --e;
        *begin = b;
        *end = e;
    }
} // namespace

size_t firmngin_audio_mono_s16le_to_stereo(const uint8_t *mono, size_t mono_len,
                                           uint8_t *stereo, size_t stereo_len)
{
    if (mono == nullptr || stereo == nullptr || mono_len == 0 || (mono_len % 2) != 0 ||
        stereo_len < mono_len * AUDIO_PLAYBACK_CHANNELS)
        return 0;

    for (size_t in = 0, out = 0; in < mono_len; in += 2)
    {
        // Copy the little-endian sample once per channel: L(lo,hi), R(lo,hi).
        stereo[out++] = mono[in];
        stereo[out++] = mono[in + 1];
        stereo[out++] = mono[in];
        stereo[out++] = mono[in + 1];
    }
    return mono_len * AUDIO_PLAYBACK_CHANNELS;
}

size_t firmngin_audio_inmp441_to_mono_s16le(const uint8_t *stereo32, size_t stereo32_len,
                                            bool right_channel,
                                            uint8_t *mono16, size_t mono16_len)
{
    if (stereo32 == nullptr || mono16 == nullptr || stereo32_len % 8 != 0)
        return 0;
    const size_t samples = stereo32_len / 8;
    if (mono16_len < samples * 2)
        return 0;

    const size_t slotOffset = right_channel ? 4 : 0;
    for (size_t i = 0; i < samples; ++i)
    {
        const uint8_t *slot = stereo32 + i * 8 + slotOffset;
        const int32_t sample = static_cast<int32_t>(
            static_cast<uint32_t>(slot[0]) |
            (static_cast<uint32_t>(slot[1]) << 8) |
            (static_cast<uint32_t>(slot[2]) << 16) |
            (static_cast<uint32_t>(slot[3]) << 24));
        const int16_t pcm = static_cast<int16_t>(sample >> 16);
        mono16[i * 2] = static_cast<uint8_t>(pcm & 0xff);
        mono16[i * 2 + 1] = static_cast<uint8_t>(static_cast<uint16_t>(pcm) >> 8);
    }
    return samples * 2;
}

void firmngin_audio_fade_s16le(uint8_t *pcm, size_t len, uint32_t gain_num, uint32_t gain_den)
{
    if (pcm == nullptr || gain_den == 0 || gain_num >= gain_den)
        return;
    for (size_t i = 0; i + 1 < len; i += 2)
    {
        const int16_t sample = (int16_t)(pcm[i] | (uint16_t)(pcm[i + 1] << 8));
        const int16_t scaled = (int16_t)(((int32_t)sample * gain_num) / (int32_t)gain_den);
        pcm[i] = (uint8_t)(scaled & 0xFF);
        pcm[i + 1] = (uint8_t)((uint16_t)scaled >> 8);
    }
}

void firmngin_audio_fade_out_s16le(uint8_t *pcm, size_t len, uint32_t *pos, uint32_t total)
{
    if (pcm == nullptr || pos == nullptr || total == 0 || len < 2)
        return;

    const uint32_t last = total > 1 ? total - 1 : 1;
    for (size_t i = 0; i + 1 < len; i += 2)
    {
        const uint32_t p = *pos;
        int16_t sample = (int16_t)(pcm[i] | (uint16_t)(pcm[i + 1] << 8));
        if (p >= last)
        {
            sample = 0;
        }
        else
        {
            // Cosine ease 1→0: no step at frame boundaries (those sounded like crackle).
            const float t = static_cast<float>(p) / static_cast<float>(last);
            const float gain = 0.5f * (1.0f + cosf(3.14159265f * t));
            sample = static_cast<int16_t>(static_cast<float>(sample) * gain);
        }
        pcm[i] = (uint8_t)(sample & 0xFF);
        pcm[i + 1] = (uint8_t)((uint16_t)sample >> 8);
        (*pos)++;
    }
}

bool firmngin_audio_ws_auth_message(const char *device_id, const char *timestamp, const char *path,
                                   char *out, size_t out_len)
{
    if (out == nullptr || out_len == 0)
        return false;
    out[0] = '\0';
    if (device_id == nullptr || device_id[0] == '\0' ||
        timestamp == nullptr || timestamp[0] == '\0' ||
        path == nullptr || path[0] == '\0')
        return false;

    const int written = snprintf(out, out_len, "%s.%s.GET.%s", device_id, timestamp, path);
    if (written < 0 || static_cast<size_t>(written) >= out_len)
    {
        out[0] = '\0';
        return false;
    }
    return true;
}

bool firmngin_audio_stream_id(const char *device_id, const char *device_key,
                              char *out, size_t out_len)
{
    if (out == nullptr || out_len < 33)
    {
        if (out != nullptr && out_len > 0)
            out[0] = '\0';
        return false;
    }
    out[0] = '\0';

    const char *idBegin = nullptr;
    const char *idEnd = nullptr;
    const char *keyBegin = nullptr;
    const char *keyEnd = nullptr;
    trim_bounds(device_id, &idBegin, &idEnd);
    trim_bounds(device_key, &keyBegin, &keyEnd);

    const size_t idLen = static_cast<size_t>(idEnd - idBegin);
    const size_t keyLen = static_cast<size_t>(keyEnd - keyBegin);
    if (idLen == 0 || keyLen == 0)
        return false;

    // id (lowercased) + unit separator + key (case preserved), same as the platform.
    char material[256];
    if (idLen + 1 + keyLen >= sizeof(material))
        return false;

    for (size_t i = 0; i < idLen; ++i)
        material[i] = static_cast<char>(tolower(static_cast<unsigned char>(idBegin[i])));
    material[idLen] = '\x1f';
    memcpy(material + idLen + 1, keyBegin, keyLen);
    material[idLen + 1 + keyLen] = '\0';

    firmngin_md5_hex(reinterpret_cast<const uint8_t *>(material), idLen + 1 + keyLen, out, out_len);
    return out[0] != '\0';
}

FirmnginAudioEvent firmngin_audio_control_event(const char *payload, size_t len,
                                               char *entity_key, size_t entity_key_len)
{
    if (entity_key != nullptr && entity_key_len > 0)
        entity_key[0] = '\0';
    if (payload == nullptr || len == 0)
        return AUDIO_NONE;

    firmngin_json::Parser parser(payload, len);
    char type[32];
    if (parser.getString("type", type, sizeof(type)) == 0)
        return AUDIO_NONE;

    if (entity_key != nullptr && entity_key_len > 0)
        parser.getString("entity_key", entity_key, entity_key_len);

    if (strcmp(type, "ready") == 0)
        return AUDIO_READY;
    if (strcmp(type, "audio.flush") == 0)
        return AUDIO_FLUSH;
    if (strcmp(type, "audio.start") == 0)
        return AUDIO_START;
    if (strcmp(type, "audio.end") == 0)
        return AUDIO_END;
    if (strcmp(type, "audio.input.accepted") == 0)
        return AUDIO_INPUT_ACCEPTED;
    if (strcmp(type, "audio.input.saved") == 0)
        return AUDIO_INPUT_SAVED;
    if (strcmp(type, "audio.input.error") == 0)
        return AUDIO_INPUT_ERROR;
    return AUDIO_NONE;
}
