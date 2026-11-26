#include "firmngin.h"
#include "queue_format.h"
#include "fng_fs.h"

void Firmngin::setQueueEnabled(bool enabled)
{
    _queueEnabled = enabled;
    if (enabled && !_queueFileReady)
    {
        if (!_initQueue())
        {
            _Debug("queue: init failed, persistent queue disabled", true);
        }
    }
}

void Firmngin::setMaxQueueEntries(uint16_t maxEntries)
{
    if (_queueFileReady)
    {
        _Debug("queue: cannot resize after init; call before setQueueEnabled()", true);
        return;
    }
    if (maxEntries < FIRMNGIN_QUEUE_MIN_CAPACITY)
        maxEntries = FIRMNGIN_QUEUE_MIN_CAPACITY;
    if (maxEntries > FIRMNGIN_QUEUE_MAX_CAPACITY)
        maxEntries = FIRMNGIN_QUEUE_MAX_CAPACITY;
    _queueCapacity = maxEntries;
}

uint16_t Firmngin::getQueueSize() const
{
    return _queueCount;
}

void Firmngin::clearQueue()
{
    if (!_queueFileReady)
        return;
    _queueHead = 0;
    _queueTail = 0;
    _queueCount = 0;
    _writeQueueHeader(0, 0, 0);
    _Debug("queue: cleared", true);
}

bool Firmngin::_initQueue()
{
#if defined(ESP_PLATFORM)
    if (_queueFileReady)
        return true;

    if (!fng_fs_mount())
    {
        _Debug("queue: LITTLEFS mount failed", true);
        return false;
    }

    if (!fng_fs_file_exists(fng_fs_path(FIRMNGIN_QUEUE_FILE_NAME).c_str()))
    {
        _resetQueueFile();
        _queueFileReady = true;
        if (_debug)
        {
            _Debug(String("queue: created file, capacity=") + String(_queueCapacity), true);
        }
        return true;
    }

    uint16_t head, tail, count;
    if (!_readQueueHeader(head, tail, count))
    {
        _Debug("queue: file corrupt, recreating", true);
        remove(fng_fs_path(FIRMNGIN_QUEUE_FILE_NAME).c_str());
        _resetQueueFile();
        _queueFileReady = true;
        return true;
    }

    _queueHead = head;
    _queueTail = tail;
    _queueCount = count;
    _queueFileReady = true;

    if (_debug)
    {
        _Debug(String("queue: loaded, count=") + String(_queueCount) + "/" + String(_queueCapacity), true);
    }
    return true;
#else
    return false;
#endif
}

void Firmngin::_shutdownQueue()
{
    _queueFileReady = false;
#if defined(ESP_PLATFORM)
    fng_fs_unmount();
#endif
}

void Firmngin::_resetQueueFile()
{
    _queueHead = 0;
    _queueTail = 0;
    _queueCount = 0;

#if defined(ESP_PLATFORM)
    FILE *f = fopen(fng_fs_path(FIRMNGIN_QUEUE_FILE_NAME).c_str(), "w");
    if (f == nullptr)
        return;

    size_t fileSize = FIRMNGIN_QUEUE_HEADER_SIZE + (size_t)_queueCapacity * FIRMNGIN_QUEUE_RECORD_SIZE;
    uint8_t zero[64] = {0};
    size_t remaining = fileSize;
    while (remaining > 0)
    {
        size_t chunk = remaining > sizeof(zero) ? sizeof(zero) : remaining;
        if (fwrite(zero, 1, chunk, f) != chunk)
            break;
        remaining -= chunk;
    }
    fclose(f);

    _writeQueueHeader(0, 0, 0);
#endif
}

bool Firmngin::_readQueueHeader(uint16_t &head, uint16_t &tail, uint16_t &count)
{
#if defined(ESP_PLATFORM)
    FILE *f = fopen(fng_fs_path(FIRMNGIN_QUEUE_FILE_NAME).c_str(), "r");
    if (f == nullptr)
        return false;
    bool ok = true;
    uint8_t hdr[FIRMNGIN_QUEUE_HEADER_SIZE];
    if (fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr))
        ok = false;
    fclose(f);
    if (!ok)
        return false;

    return firmngin_queue_decode_header(hdr, FIRMNGIN_QUEUE_HEADER_SIZE, _queueCapacity, &head, &tail, &count);
#else
    return false;
#endif
}

bool Firmngin::_writeQueueHeader(uint16_t head, uint16_t tail, uint16_t count)
{
#if defined(ESP_PLATFORM)
    FILE *f = fopen(fng_fs_path(FIRMNGIN_QUEUE_FILE_NAME).c_str(), "r+");
    if (f == nullptr)
        return false;
    uint8_t hdr[FIRMNGIN_QUEUE_HEADER_SIZE];
    if (!firmngin_queue_encode_header(hdr, sizeof(hdr), head, tail, count))
    {
        fclose(f);
        return false;
    }

    const size_t wrote = fwrite(hdr, 1, FIRMNGIN_QUEUE_HEADER_SIZE, f);
    fclose(f);
    return wrote == FIRMNGIN_QUEUE_HEADER_SIZE;
#else
    return false;
#endif
}

bool Firmngin::_enqueueToQueue(const char *topic, const uint8_t *payload, size_t payloadLen, bool retained)
{
#if defined(ESP_PLATFORM)
    if (!_queueFileReady || topic == nullptr || payload == nullptr)
        return false;

    if (_queueCount >= _queueCapacity)
    {
        _queueHead = (_queueHead + 1) % _queueCapacity;
        _queueCount--;
        _Debug("queue: full, dropped oldest", true);
    }

    uint8_t record[FIRMNGIN_QUEUE_RECORD_SIZE];
    if (!firmngin_queue_pack_record(record, sizeof(record), topic, payload, payloadLen, retained))
    {
        _Debug("queue: pack failed", true);
        return false;
    }

    size_t recordOffset = firmngin_queue_record_file_offset(_queueTail, FIRMNGIN_QUEUE_HEADER_SIZE, FIRMNGIN_QUEUE_RECORD_SIZE);
    FILE *f = fopen(fng_fs_path(FIRMNGIN_QUEUE_FILE_NAME).c_str(), "r+");
    if (f == nullptr)
        return false;
    if (fseek(f, static_cast<long>(recordOffset), SEEK_SET) != 0)
    {
        fclose(f);
        return false;
    }
    const size_t wrote = fwrite(record, 1, FIRMNGIN_QUEUE_RECORD_SIZE, f);
    fclose(f);
    if (wrote != FIRMNGIN_QUEUE_RECORD_SIZE)
        return false;

    _queueTail = (_queueTail + 1) % _queueCapacity;
    _queueCount++;
    _writeQueueHeader(_queueHead, _queueTail, _queueCount);
    return true;
#else
    return false;
#endif
}

bool Firmngin::_peekQueueHead(String &topic, String &payload, bool &retained)
{
#if defined(ESP_PLATFORM)
    if (!_queueFileReady || _queueCount == 0)
        return false;

    size_t recordOffset = firmngin_queue_record_file_offset(_queueHead, FIRMNGIN_QUEUE_HEADER_SIZE, FIRMNGIN_QUEUE_RECORD_SIZE);
    FILE *f = fopen(fng_fs_path(FIRMNGIN_QUEUE_FILE_NAME).c_str(), "r");
    if (f == nullptr)
        return false;
    if (fseek(f, static_cast<long>(recordOffset), SEEK_SET) != 0)
    {
        fclose(f);
        return false;
    }

    uint8_t record[FIRMNGIN_QUEUE_RECORD_SIZE];
    if (fread(record, 1, sizeof(record), f) != sizeof(record))
    {
        fclose(f);
        return false;
    }
    fclose(f);

    char topicBuf[FIRMNGIN_QUEUE_TOPIC_MAX + 1];
    char payloadBuf[FIRMNGIN_QUEUE_PAYLOAD_MAX + 1];
    uint16_t payloadLen = 0;
    if (!firmngin_queue_unpack_record(record, sizeof(record), topicBuf, sizeof(topicBuf), payloadBuf, sizeof(payloadBuf), &payloadLen, &retained))
        return false;

    topic = String(topicBuf);
    payload = String(payloadBuf);
    return true;
#else
    return false;
#endif
}

void Firmngin::_dropQueueHead()
{
    if (!_queueFileReady || _queueCount == 0)
        return;
    _queueHead = (_queueHead + 1) % _queueCapacity;
    _queueCount--;
    _writeQueueHeader(_queueHead, _queueTail, _queueCount);
}

void Firmngin::_drainQueue()
{
    if (!_queueEnabled || !_queueFileReady)
        return;
    if (!_mqttIsConnected())
        return;
    if (_queueCount == 0)
    {
        _lastQueueDrainMs = fng_now_ms();
        return;
    }

    unsigned long now = fng_now_ms();
    if (_lastQueueDrainMs != 0 && (now - _lastQueueDrainMs) < FIRMNGIN_QUEUE_DRAIN_INTERVAL_MS)
        return;
    _lastQueueDrainMs = now;

    uint16_t toDrain = _queueCount;
    uint16_t drained = 0;
    uint16_t failed = 0;

    while (drained + failed < toDrain)
    {
        String topic, payload;
        bool retained;
        if (!_peekQueueHead(topic, payload, retained))
        {
            _dropQueueHead();
            failed++;
            continue;
        }

        bool ok = _mqttPublish(topic.c_str(), reinterpret_cast<const uint8_t *>(payload.c_str()), payload.length(), retained);
        if (!ok)
        {
            break;
        }

        _dropQueueHead();
        drained++;
    }

    if (_debug && (drained > 0 || failed > 0))
    {
        _Debug(String("queue: drained=") + String(drained) + ", dropped_corrupt=" + String(failed), true);
    }
}
