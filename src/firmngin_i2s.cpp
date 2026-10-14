#include "firmngin_i2s.h"

FirmnginI2s::FirmnginI2s(FirmnginI2sChip chip) : _chip(chip) {}

FirmnginI2s::~FirmnginI2s()
{
    end();
}

FirmnginI2s::FirmnginI2s(FirmnginI2s &&other) noexcept
    : _chip(other._chip),
      _bclk(other._bclk),
      _ws(other._ws),
      _data(other._data),
      _mclk(other._mclk),
      _sampleRate(other._sampleRate),
      _dmaDesc(other._dmaDesc),
      _dmaSamples(other._dmaSamples),
      _channel(other._channel),
      _ready(other._ready),
      _lastLeft(other._lastLeft),
      _lastRight(other._lastRight)
{
#if defined(ESP32)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    _tx = other._tx;
    _rx = other._rx;
    other._tx = nullptr;
    other._rx = nullptr;
#else
    _port = other._port;
    _installed = other._installed;
    other._installed = false;
#endif
#endif
    other._ready = false;
    other._lastLeft = 0;
    other._lastRight = 0;
}

FirmnginI2s &FirmnginI2s::operator=(FirmnginI2s &&other) noexcept
{
    if (this == &other)
        return *this;
    end();
    _chip = other._chip;
    _bclk = other._bclk;
    _ws = other._ws;
    _data = other._data;
    _mclk = other._mclk;
    _sampleRate = other._sampleRate;
    _dmaDesc = other._dmaDesc;
    _dmaSamples = other._dmaSamples;
    _channel = other._channel;
    _ready = other._ready;
    _lastLeft = other._lastLeft;
    _lastRight = other._lastRight;
#if defined(ESP32)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    _tx = other._tx;
    _rx = other._rx;
    other._tx = nullptr;
    other._rx = nullptr;
#else
    _port = other._port;
    _installed = other._installed;
    other._installed = false;
#endif
#endif
    other._ready = false;
    other._lastLeft = 0;
    other._lastRight = 0;
    return *this;
}

FirmnginI2s &FirmnginI2s::setChip(FirmnginI2sChip chip)
{
    if (_ready && _chip != chip)
        end();
    _chip = chip;
    return *this;
}

FirmnginI2s &FirmnginI2s::pins(int bclk, int ws, int data)
{
    _bclk = bclk;
    _ws = ws;
    _data = data;
    return *this;
}

FirmnginI2s &FirmnginI2s::mclk(int pin)
{
    _mclk = pin;
    return *this;
}

FirmnginI2s &FirmnginI2s::sampleRate(uint32_t hz)
{
    if (hz > 0)
        _sampleRate = hz;
    return *this;
}

FirmnginI2s &FirmnginI2s::dma(uint8_t descCount, uint16_t samplesPerDesc)
{
    if (descCount > 0)
        _dmaDesc = descCount;
    if (samplesPerDesc > 0)
        _dmaSamples = samplesPerDesc;
    return *this;
}

FirmnginI2s &FirmnginI2s::channel(FirmnginI2sChannel channel)
{
    if (_ready && _channel != channel)
        end();
    _channel = channel;
    return *this;
}

bool FirmnginI2s::_isTx() const
{
    return _chip != INMP441 && _chip != I2S_RX;
}

void FirmnginI2s::_release()
{
#if defined(ESP32)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    if (_tx != nullptr)
    {
        i2s_channel_disable(_tx);
        i2s_del_channel(_tx);
        _tx = nullptr;
    }
    if (_rx != nullptr)
    {
        i2s_channel_disable(_rx);
        i2s_del_channel(_rx);
        _rx = nullptr;
    }
#else
    if (_installed)
    {
        i2s_driver_uninstall(_port);
        _installed = false;
    }
#endif
#endif
    _ready = false;
    _lastLeft = 0;
    _lastRight = 0;
}

void FirmnginI2s::end()
{
    _release();
}

FirmnginI2s &FirmnginI2s::begin()
{
    _release();

#if !defined(ESP32)
    return *this;
#else
    if (_bclk < 0 || _ws < 0 || _data < 0)
        return *this;

#if ESP_ARDUINO_VERSION_MAJOR >= 3
    const bool tx = _isTx();
    i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan.dma_desc_num = _dmaDesc;
    chan.dma_frame_num = _dmaSamples;
    // On an underrun the TX DMA must emit silence, not repeat its previous buffer as
    // a buzz. The legacy driver enables the equivalent tx_desc_auto_clear below.
    chan.auto_clear = true;

    if (i2s_new_channel(&chan, tx ? &_tx : nullptr, tx ? nullptr : &_rx) != ESP_OK)
        return *this;

    i2s_std_config_t config = {};
    config.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(_sampleRate);
    // INMP441 emits a 24-bit two's-complement sample inside each 32-clock I2S
    // slot. Keep RX stereo so the microphone still receives the required 64
    // clocks per WS frame; the capture task selects its configured L/R slot.
    // The macro expands to a brace initializer, which cannot be an operand of
    // ?: -- assign it from each branch instead.
    if (_chip == INMP441)
        config.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
    else
        config.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    config.gpio_cfg.mclk = _mclk >= 0 ? (gpio_num_t)_mclk : I2S_GPIO_UNUSED;
    config.gpio_cfg.bclk = (gpio_num_t)_bclk;
    config.gpio_cfg.ws = (gpio_num_t)_ws;
    config.gpio_cfg.dout = tx ? (gpio_num_t)_data : I2S_GPIO_UNUSED;
    config.gpio_cfg.din = tx ? I2S_GPIO_UNUSED : (gpio_num_t)_data;

    i2s_chan_handle_t handle = tx ? _tx : _rx;
    if (i2s_channel_init_std_mode(handle, &config) != ESP_OK ||
        i2s_channel_enable(handle) != ESP_OK)
    {
        _release();
        return *this;
    }
    _ready = true;
    return *this;
#else
    const bool tx = _isTx();
    i2s_config_t config = {};
    config.mode = (i2s_mode_t)(I2S_MODE_MASTER | (tx ? I2S_MODE_TX : I2S_MODE_RX));
    config.sample_rate = _sampleRate;
    config.bits_per_sample = _chip == INMP441 ? I2S_BITS_PER_SAMPLE_32BIT : I2S_BITS_PER_SAMPLE_16BIT;
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = 0;
    config.dma_buf_count = _dmaDesc;
    config.dma_buf_len = _dmaSamples;
    config.use_apll = false;
    config.tx_desc_auto_clear = true;
    config.fixed_mclk = 0;

    i2s_pin_config_t pins = {};
    // The struct must not be left zero-initialized: IDF treats mck_io_num = 0 as a
    // real pin and routes MCLK to GPIO0 (the boot strapping pin) in master mode.
    pins.mck_io_num = _mclk >= 0 ? _mclk : I2S_PIN_NO_CHANGE;
    pins.bck_io_num = _bclk;
    pins.ws_io_num = _ws;
    pins.data_out_num = tx ? _data : I2S_PIN_NO_CHANGE;
    pins.data_in_num = tx ? I2S_PIN_NO_CHANGE : _data;

    if (i2s_driver_install(_port, &config, 0, nullptr) != ESP_OK)
        return *this;
    _installed = true;
    if (i2s_set_pin(_port, &pins) != ESP_OK)
    {
        _release();
        return *this;
    }
    _ready = true;
    return *this;
#endif
#endif
}

bool FirmnginI2s::write(const uint8_t *data, size_t len)
{
    if (!_ready || data == nullptr || len == 0 || !_isTx())
        return false;
#if defined(ESP32)
    bool ok = false;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    size_t written = 0;
    ok = i2s_channel_write(_tx, data, len, &written, portMAX_DELAY) == ESP_OK && written == len;
#else
    size_t written = 0;
    ok = i2s_write(_port, data, len, &written, portMAX_DELAY) == ESP_OK && written == len;
#endif
    if (ok && len >= 4)
    {
        const size_t tail = len - 4;
        _lastLeft = static_cast<int16_t>(
            static_cast<uint16_t>(data[tail]) |
            (static_cast<uint16_t>(data[tail + 1]) << 8));
        _lastRight = static_cast<int16_t>(
            static_cast<uint16_t>(data[tail + 2]) |
            (static_cast<uint16_t>(data[tail + 3]) << 8));
    }
    return ok;
#else
    return false;
#endif
}

bool FirmnginI2s::flush()
{
    if (!_ready || !_isTx())
        return false;
#if defined(ESP32)
    // Hard disable/enable cuts a sample at an arbitrary point and is heard as a click.
    // Let the small hardware DMA tail finish, then land on zero over one 20 ms frame.
    static uint8_t fade[320 * 2 * 2];
    const int32_t denominator = 319;
    for (int32_t i = 0; i < 320; ++i)
    {
        const int32_t gain = denominator - i;
        const int16_t left = static_cast<int16_t>((static_cast<int32_t>(_lastLeft) * gain) / denominator);
        const int16_t right = static_cast<int16_t>((static_cast<int32_t>(_lastRight) * gain) / denominator);
        const size_t out = static_cast<size_t>(i) * 4;
        fade[out] = static_cast<uint8_t>(left & 0xFF);
        fade[out + 1] = static_cast<uint8_t>(static_cast<uint16_t>(left) >> 8);
        fade[out + 2] = static_cast<uint8_t>(right & 0xFF);
        fade[out + 3] = static_cast<uint8_t>(static_cast<uint16_t>(right) >> 8);
    }
    return write(fade, sizeof(fade));
#else
    return false;
#endif
}

size_t FirmnginI2s::read(uint8_t *data, size_t len)
{
    if (!_ready || data == nullptr || len == 0 || _isTx())
        return 0;
#if defined(ESP32)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    size_t readBytes = 0;
    if (i2s_channel_read(_rx, data, len, &readBytes, portMAX_DELAY) != ESP_OK)
        return 0;
    return readBytes;
#else
    size_t readBytes = 0;
    if (i2s_read(_port, data, len, &readBytes, portMAX_DELAY) != ESP_OK)
        return 0;
    return readBytes;
#endif
#else
    return 0;
#endif
}
