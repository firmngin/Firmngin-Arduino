#ifndef FIRMNGIN_I2S_H
#define FIRMNGIN_I2S_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#if defined(ESP32)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
#include <driver/i2s_std.h>
#else
#include <driver/i2s.h>
#endif
#endif

// Chip picks direction and defaults. Pins stay role-based (bclk, ws, data).
enum FirmnginI2sChip
{
    MAX98357A = 0, // TX amp
    PCM5102,       // TX DAC
    INMP441,       // RX mic
    I2S_TX,        // generic out
    I2S_RX         // generic in
};

enum FirmnginI2sChannel
{
    FIRMNGIN_I2S_LEFT = 0,
    FIRMNGIN_I2S_RIGHT = 1
};

// Fluent I2S wrapper tuned for Firmngin playback: 16 kHz, s16, stereo Philips,
// DMA = 4 descriptors x 320 samples (80 ms, one Firmngin frame per descriptor).
//
//   auto &i2s = fngin.setAudioInterface(entity, MAX98357A)
//       .pins(I2S_BCLK, I2S_LRC, I2S_DOUT);
//   fngin.begin(); // starts I2S with the rest of Firmngin
class FirmnginI2s
{
public:
    explicit FirmnginI2s(FirmnginI2sChip chip = MAX98357A);
    ~FirmnginI2s();

    FirmnginI2s(const FirmnginI2s &) = delete;
    FirmnginI2s &operator=(const FirmnginI2s &) = delete;
    FirmnginI2s(FirmnginI2s &&other) noexcept;
    FirmnginI2s &operator=(FirmnginI2s &&other) noexcept;

    FirmnginI2s &setChip(FirmnginI2sChip chip);
    // bclk, ws (LRC/LRCK), data (dout for TX, din for RX)
    FirmnginI2s &pins(int bclk, int ws, int data);
    FirmnginI2s &mclk(int pin);
    FirmnginI2s &sampleRate(uint32_t hz);
    FirmnginI2s &dma(uint8_t descCount, uint16_t samplesPerDesc);
    // Selects the slot driven by a mono I2S microphone such as INMP441. This must
    // match the module's L/R pin (LOW = left, HIGH = right).
    FirmnginI2s &channel(FirmnginI2sChannel channel);

    // Called from Firmngin::begin() when pins are set; safe to call again.
    FirmnginI2s &begin();
    void end();

    bool write(const uint8_t *data, size_t len);
    // Finishes the already queued DMA tail with a short ramp to silence. Natural clip
    // completion fades its final PCM frame before it reaches this driver.
    bool flush();
    size_t read(uint8_t *data, size_t len);

    bool pinsConfigured() const { return _bclk >= 0 && _ws >= 0 && _data >= 0; }
    bool ready() const { return _ready; }
    FirmnginI2sChannel selectedChannel() const { return _channel; }
    explicit operator bool() const { return _ready; }

private:
    bool _isTx() const;
    void _release();

    FirmnginI2sChip _chip;
    int _bclk = -1;
    int _ws = -1;
    int _data = -1;
    int _mclk = -1;
    uint32_t _sampleRate = 16000;
    uint8_t _dmaDesc = 4;
    uint16_t _dmaSamples = 320;
    FirmnginI2sChannel _channel = FIRMNGIN_I2S_LEFT;
    bool _ready = false;
    int16_t _lastLeft = 0;
    int16_t _lastRight = 0;

#if defined(ESP32)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    i2s_chan_handle_t _tx = nullptr;
    i2s_chan_handle_t _rx = nullptr;
#else
    i2s_port_t _port = I2S_NUM_0;
    bool _installed = false;
#endif
#endif
};

#endif
