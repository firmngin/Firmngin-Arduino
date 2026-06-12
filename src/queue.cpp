#include "firmngin.h"
#include "queue_format.h"

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
    if (_debug)
    {
        Serial.println(F("[firmngin] queue: cleared"));
    }
}

bool Firmngin::_initQueue()
{
#if defined(ESP32) || defined(ESP8266)
    if (_queueFileReady)
        return true;

#if defined(ESP32)
    if (!LittleFS.begin(true))
#elif defined(ESP8266)
    if (!LittleFS.begin())
#endif
    {
        _Debug("queue: LITTLEFS mount failed", true);
        return false;
    }

    if (!LittleFS.exists(FIRMNGIN_QUEUE_FILE_NAME))
    {
        _resetQueueFile();
        _queueFileReady = true;
        if (_debug)
        {
            Serial.print(F("[firmngin] queue: created file, capacity="));
            Serial.println(_queueCapacity);
        }
        return true;
    }

    uint16_t head, tail, count;
    if (!_readQueueHeader(head, tail, count))
    {
        _Debug("queue: file corrupt, recreating", true);
        LittleFS.remove(FIRMNGIN_QUEUE_FILE_NAME);
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
        Serial.print(F("[firmngin] queue: loaded, count="));
        Serial.print(_queueCount);
        Serial.print(F("/"));
        Serial.println(_queueCapacity);
    }
    return true;
#else
    return false;
#endif
}

void Firmngin::_shutdownQueue()
{
    _queueFileReady = false;
#if defined(ESP32) || defined(ESP8266)
    LittleFS.end();
#endif
}

void Firmngin::_resetQueueFile()
{
    _queueHead = 0;
    _queueTail = 0;
    _queueCount = 0;

#if defined(ESP32) || defined(ESP8266)
    File f = LittleFS.open(FIRMNGIN_QUEUE_FILE_NAME, "w");
    if (!f)
        return;

    size_t fileSize = FIRMNGIN_QUEUE_HEADER_SIZE + (size_t)_queueCapacity * FIRMNGIN_QUEUE_RECORD_SIZE;
    uint8_t zero[64] = {0};
    size_t remaining = fileSize;
    while (remaining > 0)
    {
        size_t chunk = remaining > sizeof(zero) ? sizeof(zero) : remaining;
        if (f.write(zero, chunk) != chunk)
            break;
        remaining -= chunk;
    }
    f.close();

    _writeQueueHeader(0, 0, 0);
#endif
}

bool Firmngin::_readQueueHeader(uint16_t &head, uint16_t &tail, uint16_t &count)
{
#if defined(ESP32) || defined(ESP8266)
    File f = LittleFS.open(FIRMNGIN_QUEUE_FILE_NAME, "r");
    if (!f || f.size() < FIRMNGIN_QUEUE_HEADER_SIZE)
    {
        if (f)
            f.close();
        return false;
    }

    uint8_t hdr[FIRMNGIN_QUEUE_HEADER_SIZE];
    if (f.readBytes((char *)hdr, FIRMNGIN_QUEUE_HEADER_SIZE) != FIRMNGIN_QUEUE_HEADER_SIZE)
    {
        f.close();
        return false;
    }
    f.close();

    return firmngin_queue_decode_header(hdr, FIRMNGIN_QUEUE_HEADER_SIZE, _queueCapacity, &head, &tail, &count);
#else
    return false;
#endif
}

bool Firmngin::_writeQueueHeader(uint16_t head, uint16_t tail, uint16_t count)
{
#if defined(ESP32) || defined(ESP8266)
    File f = LittleFS.open(FIRMNGIN_QUEUE_FILE_NAME, "r+");
    if (!f)
        return false;
    if (!f.seek(0))
    {
        f.close();
        return false;
    }

    uint8_t hdr[FIRMNGIN_QUEUE_HEADER_SIZE];
    if (!firmngin_queue_encode_header(hdr, sizeof(hdr), head, tail, count))
    {
        f.close();
        return false;
    }

    size_t wrote = f.write(hdr, FIRMNGIN_QUEUE_HEADER_SIZE);
    f.close();
    return wrote == FIRMNGIN_QUEUE_HEADER_SIZE;
#else
    return false;
#endif
}

bool Firmngin::_enqueueToQueue(const char *topic, const uint8_t *payload, size_t payloadLen, bool retained)
{
#if defined(ESP32) || defined(ESP8266)
    if (!_queueFileReady || topic == nullptr || payload == nullptr)
        return false;

    if (_queueCount >= _queueCapacity)
    {
        _queueHead = (_queueHead + 1) % _queueCapacity;
        _queueCount--;
        if (_debug)
        {
            Serial.println(F("[firmngin] queue: full, dropped oldest"));
        }
    }

    uint8_t record[FIRMNGIN_QUEUE_RECORD_SIZE];
    if (!firmngin_queue_pack_record(record, sizeof(record), topic, payload, payloadLen, retained))
    {
        if (_debug)
            Serial.println(F("[firmngin] queue: pack failed"));
        return false;
    }

    size_t recordOffset = firmngin_queue_record_file_offset(_queueTail, FIRMNGIN_QUEUE_HEADER_SIZE, FIRMNGIN_QUEUE_RECORD_SIZE);
    File f = LittleFS.open(FIRMNGIN_QUEUE_FILE_NAME, "r+");
    if (!f)
        return false;
    if (!f.seek(recordOffset))
    {
        f.close();
        return false;
    }
    size_t wrote = f.write(record, FIRMNGIN_QUEUE_RECORD_SIZE);
    f.close();
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
#if defined(ESP32) || defined(ESP8266)
    if (!_queueFileReady || _queueCount == 0)
        return false;

    size_t recordOffset = firmngin_queue_record_file_offset(_queueHead, FIRMNGIN_QUEUE_HEADER_SIZE, FIRMNGIN_QUEUE_RECORD_SIZE);
    File f = LittleFS.open(FIRMNGIN_QUEUE_FILE_NAME, "r");
    if (!f)
        return false;
    if (!f.seek(recordOffset))
    {
        f.close();
        return false;
    }

    uint8_t record[FIRMNGIN_QUEUE_RECORD_SIZE];
    if (f.readBytes((char *)record, FIRMNGIN_QUEUE_RECORD_SIZE) != FIRMNGIN_QUEUE_RECORD_SIZE)
    {
        f.close();
        return false;
    }
    f.close();

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
    if (!_mqttClient.connected())
        return;
    if (_queueCount == 0)
    {
        _lastQueueDrainMs = millis();
        return;
    }

    unsigned long now = millis();
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

        bool ok = _mqttClient.publish(topic.c_str(), (const uint8_t *)payload.c_str(), payload.length(), retained);
        if (!ok)
        {
            break;
        }

        _dropQueueHead();
        drained++;
    }

    if (_debug && (drained > 0 || failed > 0))
    {
        Serial.print(F("[firmngin] queue: drained="));
        Serial.print(drained);
        Serial.print(F(", dropped_corrupt="));
        Serial.println(failed);
    }
}
