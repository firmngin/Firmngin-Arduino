#include "firmngin.h"

#if defined(ESP32)
#include <esp_netif.h>
#include <string.h>
#include <stdlib.h>

struct FirmnginMqttPacket
{
    uint8_t which;
    char *topic;
    uint8_t *data;
    unsigned len;
};

struct MqttStartOpts
{
    const char *host;
    uint32_t port;
    const char *clientId;
    const char *user;
    const char *pass;
    const char *willTopic;
    const char *willMsg;
    bool tls;
    bool insecure;
    const char *ca;
    const char *cert;
    const char *key;
    int keepalive;
    int bufferSize;
};

void firmnginMqttEvent(void *arg, esp_event_base_t, int32_t, void *data)
{
    if (arg == nullptr || data == nullptr)
        return;
    static_cast<Firmngin *>(arg)->_onMqttEvent(static_cast<esp_mqtt_event_handle_t>(data));
}

static esp_mqtt_client_handle_t startMqtt(const MqttStartOpts &opt, void *arg)
{
    esp_mqtt_client_config_t cfg = {};
    const bool verify = opt.tls && !opt.insecure && opt.ca != nullptr && opt.ca[0] != '\0';
    const bool clientAuth = opt.tls && opt.cert != nullptr && opt.key != nullptr && opt.cert[0] != '\0' && opt.key[0] != '\0';

#if ESP_ARDUINO_VERSION_MAJOR >= 3
    cfg.broker.address.hostname = opt.host;
    cfg.broker.address.port = opt.port;
    cfg.broker.address.transport = opt.tls ? MQTT_TRANSPORT_OVER_SSL : MQTT_TRANSPORT_OVER_TCP;
    if (verify)
        cfg.broker.verification.certificate = opt.ca;
    cfg.broker.verification.skip_cert_common_name_check = opt.tls && opt.insecure;
    cfg.credentials.client_id = opt.clientId;
    cfg.credentials.username = opt.user;
    cfg.credentials.authentication.password = opt.pass;
    if (clientAuth)
    {
        cfg.credentials.authentication.certificate = opt.cert;
        cfg.credentials.authentication.key = opt.key;
    }
    cfg.session.last_will.topic = opt.willTopic;
    cfg.session.last_will.msg = opt.willMsg;
    cfg.session.last_will.qos = 1;
    cfg.session.last_will.retain = 1;
    cfg.session.disable_clean_session = true;
    cfg.session.keepalive = opt.keepalive;
    cfg.buffer.size = opt.bufferSize;
    cfg.buffer.out_size = opt.bufferSize;
    cfg.network.timeout_ms = 10000;
    cfg.network.reconnect_timeout_ms = 5000;
#else
    cfg.host = opt.host;
    cfg.port = opt.port;
    cfg.transport = opt.tls ? MQTT_TRANSPORT_OVER_SSL : MQTT_TRANSPORT_OVER_TCP;
    cfg.client_id = opt.clientId;
    cfg.username = opt.user;
    cfg.password = opt.pass;
    cfg.lwt_topic = opt.willTopic;
    cfg.lwt_msg = opt.willMsg;
    cfg.lwt_qos = 1;
    cfg.lwt_retain = 1;
    cfg.disable_clean_session = 1;
    cfg.keepalive = opt.keepalive;
    cfg.buffer_size = opt.bufferSize;
    cfg.out_buffer_size = opt.bufferSize;
    cfg.reconnect_timeout_ms = 5000;
    cfg.skip_cert_common_name_check = opt.tls && opt.insecure;
    if (verify)
        cfg.cert_pem = opt.ca;
    if (clientAuth)
    {
        cfg.client_cert_pem = opt.cert;
        cfg.client_key_pem = opt.key;
    }
#endif

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&cfg);
    if (client == nullptr)
        return nullptr;
    if (esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, firmnginMqttEvent, arg) != ESP_OK ||
        esp_mqtt_client_start(client) != ESP_OK)
    {
        esp_mqtt_client_destroy(client);
        return nullptr;
    }
    return client;
}

bool Firmngin::_networkReady() const
{
    if (WiFi.status() == WL_CONNECTED)
        return true;
    esp_netif_t *eth = esp_netif_get_handle_from_ifkey("ETH_DEF");
    if (eth == nullptr)
        return false;
    esp_netif_ip_info_t info = {};
    if (esp_netif_get_ip_info(eth, &info) != ESP_OK)
        return false;
    return info.ip.addr != 0;
}

bool Firmngin::_mqttIsConnected() const
{
    return _mqttUp;
}

bool Firmngin::_mqttIsStarted() const
{
    return _mqtt != nullptr;
}

bool Firmngin::_mqttPublish(const char *topic, const uint8_t *data, size_t len, bool retained)
{
    if (!_mqttUp || _mqtt == nullptr || topic == nullptr || data == nullptr)
        return false;
    int id = esp_mqtt_client_enqueue(_mqtt, topic, reinterpret_cast<const char *>(data), static_cast<int>(len), 1, retained ? 1 : 0, true);
    return id >= 0;
}

void Firmngin::_ensureMqttInbox()
{
    if (_mqttLock == nullptr)
        _mqttLock = xSemaphoreCreateMutex();
    if (_mqttInbox == nullptr)
        _mqttInbox = xQueueCreate(12, sizeof(FirmnginMqttPacket *));
}

void Firmngin::_mqttRxReset(uint8_t which)
{
    if (which > 1)
        return;
    free(_rxTopic[which]);
    free(_rxData[which]);
    _rxTopic[which] = nullptr;
    _rxData[which] = nullptr;
    _rxFilled[which] = 0;
    _rxTotal[which] = 0;
}

void Firmngin::_mqttPost(uint8_t which, const char *topic, const uint8_t *data, unsigned len)
{
    if (_mqttInbox == nullptr || topic == nullptr)
        return;
    FirmnginMqttPacket *msg = static_cast<FirmnginMqttPacket *>(calloc(1, sizeof(FirmnginMqttPacket)));
    if (msg == nullptr)
    {
        _msgDroppedCount = _msgDroppedCount + 1;
        if (_debug)
            Serial.printf("[FNG-E%d] Messages queue alloc failed, dropped message to %s\n", FMSG_ERR_BUFFER_OVERFLOW, topic);
        return;
    }
    size_t topicLen = strlen(topic);
    msg->topic = static_cast<char *>(malloc(topicLen + 1));
    msg->data = len > 0 ? static_cast<uint8_t *>(malloc(len)) : nullptr;
    if (msg->topic == nullptr || (len > 0 && msg->data == nullptr))
    {
        free(msg->topic);
        free(msg->data);
        free(msg);
        _msgDroppedCount = _msgDroppedCount + 1;
        if (_debug)
            Serial.printf("[FNG-E%d] Messages queue alloc failed, dropped message to %s\n", FMSG_ERR_BUFFER_OVERFLOW, topic);
        return;
    }
    memcpy(msg->topic, topic, topicLen + 1);
    if (len > 0)
        memcpy(msg->data, data, len);
    msg->which = which;
    msg->len = len;
    if (xQueueSend(_mqttInbox, &msg, 0) != pdTRUE)
    {
        free(msg->topic);
        free(msg->data);
        free(msg);
        _msgDroppedCount = _msgDroppedCount + 1;
        if (_debug)
            Serial.printf("[FNG-E%d] Messages queue full, dropped message to %s (total dropped: %u)\n", 
                FMSG_ERR_QUEUE_FULL, topic, static_cast<unsigned int>(_msgDroppedCount));
    }
}

void Firmngin::_subscribePlatform()
{
    static const char *suffixes[] = {
        "pm", "dp", "pp", "pr", "qr", "mi", "ds", "init", "dpin", "vr", "ur",
        "le", "nl", "mop", "mpp", "moa", "moe", "mos", "pi", nullptr};
    char topic[96];
    for (int i = 0; suffixes[i] != nullptr; i++)
    {
        snprintf(topic, sizeof(topic), "/c/%s/%s", _deviceId, suffixes[i]);
        esp_mqtt_client_subscribe(_mqtt, topic, 1);
    }
    snprintf(topic, sizeof(topic), "/c/%s/ot/trg", _deviceId);
    esp_mqtt_client_subscribe(_mqtt, topic, 1);
    snprintf(topic, sizeof(topic), "/d/%s/rs/+", _deviceId);
    esp_mqtt_client_subscribe(_mqtt, topic, 1);
    const char *online = "1";
    esp_mqtt_client_enqueue(_mqtt, _mqttWillTopic.c_str(), online, 1, 1, 1, true);
    _mqttHelloPending = true;
}

void Firmngin::_userMqttResubscribe()
{
    if (_userMqtt == nullptr)
        return;
    std::vector<UserMqttSub> subs;
    if (_mqttLock != nullptr)
    {
        if (xSemaphoreTake(_mqttLock, pdMS_TO_TICKS(5000)) != pdTRUE)
        {
            if (_debug)
                Serial.printf("[FNG-E%d] Device busy: incoming messages not restored after reconnect (timeout)\n", FMSG_ERR_MUTEX_TIMEOUT);
            return;
        }
    }
    subs = _userMqttSubs;
    if (_mqttLock != nullptr)
        xSemaphoreGive(_mqttLock);
    for (const auto &sub : subs)
        esp_mqtt_client_subscribe(_userMqtt, sub.topic.c_str(), sub.qos);
}

void Firmngin::_onMqttEvent(esp_mqtt_event_handle_t event)
{
    if (event == nullptr)
        return;

    const bool user = event->client == _userMqtt;
    if (event->event_id == MQTT_EVENT_CONNECTED)
    {
        if (user)
        {
            _userMqttUp = true;
            _msgUserReconnects = _msgUserReconnects + 1;
            _userMqttResubscribe();
            if (_debug)
                Serial.println("[FNG] User server connected");
        }
        else
        {
            _mqttUp = true;
            _msgPlatformReconnects = _msgPlatformReconnects + 1;
            _msgReconnectBackoff = _msgReconnectBackoffMin;
            _subscribePlatform();
        }
        return;
    }
    if (event->event_id == MQTT_EVENT_DISCONNECTED)
    {
        if (user)
        {
            _userMqttUp = false;
            if (_debug)
                Serial.println("[FNG] User server disconnected");
        }
        else
        {
            _mqttUp = false;
            _msgLastReconnectMs = millis();
            if (_msgReconnectBackoff < _msgReconnectBackoffMax)
                _msgReconnectBackoff = min(_msgReconnectBackoff * 2, _msgReconnectBackoffMax);
            if (_debug)
                Serial.printf("[FNG] Platform server disconnected (backoff: %lus)\n", _msgReconnectBackoff / 1000);
        }
        return;
    }
    if (event->event_id == MQTT_EVENT_ERROR)
    {
        if (event->error_handle == nullptr)
        {
            if (_debug)
                Serial.printf("[FNG-E%d] Connection error (unknown)\n", FMSG_ERR_UNKNOWN);
            return;
        }
        
        const char *clientType = user ? "User" : "Platform";
        
        switch (event->error_handle->error_type)
        {
        case MQTT_ERROR_TYPE_TCP_TRANSPORT:
            if (_debug)
                Serial.printf("[FNG-E%d] %s: TCP transport error\n", FMSG_ERR_TCP_CONNECT, clientType);
            break;
        case MQTT_ERROR_TYPE_CONNECTION_REFUSED:
            if (_debug)
                Serial.printf("[FNG-E%d] %s: Connection refused (code: %d)\n", 
                    FMSG_ERR_CONN_REFUSED, clientType, 
                    static_cast<int>(event->error_handle->connect_return_code));
            break;
        default:
            if (event->error_handle->esp_tls_last_esp_err != 0)
            {
                if (_debug)
                    Serial.printf("[FNG-E%d] %s: TLS error (0x%x)\n", 
                        FMSG_ERR_TLS_HANDSHAKE, clientType,
                        event->error_handle->esp_tls_last_esp_err);
            }
            else if (event->error_handle->esp_tls_stack_err != 0)
            {
                if (_debug)
                    Serial.printf("[FNG-E%d] %s: TLS cert verify failed (0x%x)\n", 
                        FMSG_ERR_TLS_CERT_VERIFY, clientType,
                        event->error_handle->esp_tls_stack_err);
            }
            else
            {
                if (_debug)
                    Serial.printf("[FNG-E%d] %s: Connection error\n", FMSG_ERR_UNKNOWN, clientType);
            }
            break;
        }
        return;
    }
    if (event->event_id != MQTT_EVENT_DATA)
        return;

    const uint8_t which = user ? 1 : 0;
    const int total = event->total_data_len;
    const int offset = event->current_data_offset;
    const int chunk = event->data_len < 0 ? 0 : event->data_len;

    if (offset == 0)
    {
        _mqttRxReset(which);
        if (event->topic == nullptr || event->topic_len <= 0 || total < 0)
            return;
        if (total > 4096)
        {
            _msgDroppedCount = _msgDroppedCount + 1;
            if (_debug)
                Serial.printf("[FNG-E%d] Message too large: %d bytes (max 4096), dropped\n", 
                    FMSG_ERR_MESSAGE_TOO_LARGE, total);
            return;
        }
        _rxTopic[which] = static_cast<char *>(malloc(static_cast<size_t>(event->topic_len) + 1));
        if (_rxTopic[which] == nullptr)
            return;
        memcpy(_rxTopic[which], event->topic, static_cast<size_t>(event->topic_len));
        _rxTopic[which][event->topic_len] = '\0';
        if (chunk >= total)
        {
            _mqttPost(which, _rxTopic[which], reinterpret_cast<const uint8_t *>(event->data), static_cast<unsigned>(chunk));
            _mqttRxReset(which);
            return;
        }
        _rxData[which] = static_cast<uint8_t *>(malloc(static_cast<size_t>(total)));
        _rxTotal[which] = total;
        if (_rxData[which] == nullptr)
        {
            _mqttRxReset(which);
            return;
        }
    }

    if (_rxData[which] == nullptr || _rxTopic[which] == nullptr || offset != _rxFilled[which] || offset + chunk > _rxTotal[which])
    {
        _mqttRxReset(which);
        return;
    }
    if (chunk > 0 && event->data != nullptr)
        memcpy(_rxData[which] + offset, event->data, static_cast<size_t>(chunk));
    _rxFilled[which] = offset + chunk;
    if (_rxFilled[which] >= _rxTotal[which])
    {
        _mqttPost(which, _rxTopic[which], _rxData[which], static_cast<unsigned>(_rxFilled[which]));
        _mqttRxReset(which);
    }
}

void Firmngin::_drainMqttInbox()
{
    if (_mqttInbox != nullptr)
    {
        FirmnginMqttPacket *msg = nullptr;
        while (xQueueReceive(_mqttInbox, &msg, 0) == pdTRUE)
        {
            if (msg != nullptr)
            {
                if (msg->which == 0 && msg->topic != nullptr)
                    mqttCallback(msg->topic, reinterpret_cast<byte *>(msg->data), msg->len);
                else if (msg->which == 1 && _userMqttCallback && msg->topic != nullptr)
                    _userMqttCallback(msg->topic, msg->data, msg->len);
                free(msg->topic);
                free(msg->data);
                free(msg);
            }
        }
    }
    _mqttAfterConnect();
}

void Firmngin::_mqttAfterConnect()
{
    if (!_mqttUp || !_mqttHelloPending)
        return;
    _mqttHelloPending = false;
    if (_debug)
        Serial.println("Successfully connected");
    syncFirmwareInfo();
    requestInit();
    if (_queueEnabled && _queueFileReady && _queueCount > 0)
    {
        _lastQueueDrainMs = 0;
        _drainQueue();
    }
}

void Firmngin::_mqttShutdown()
{
    if (_mqtt != nullptr)
    {
        esp_mqtt_client_stop(_mqtt);
        esp_mqtt_client_destroy(_mqtt);
        _mqtt = nullptr;
    }
    if (_userMqtt != nullptr)
    {
        esp_mqtt_client_stop(_userMqtt);
        esp_mqtt_client_destroy(_userMqtt);
        _userMqtt = nullptr;
    }
    _mqttUp = false;
    _userMqttUp = false;
    _mqttRxReset(0);
    _mqttRxReset(1);
    if (_mqttInbox != nullptr)
    {
        FirmnginMqttPacket *msg = nullptr;
        while (xQueueReceive(_mqttInbox, &msg, 0) == pdTRUE)
        {
            if (msg != nullptr)
            {
                free(msg->topic);
                free(msg->data);
                free(msg);
            }
        }
        vQueueDelete(_mqttInbox);
        _mqttInbox = nullptr;
    }
    if (_mqttLock != nullptr)
    {
        vSemaphoreDelete(_mqttLock);
        _mqttLock = nullptr;
    }
}

bool Firmngin::connectServer()
{
    if (!_securityReady)
    {
        Serial.println("ERROR: secure initialization is incomplete; server connection blocked");
        return false;
    }
    if (_mqtt != nullptr)
        return _mqttUp;

    _ensureMqttInbox();
    _mqttWillTopic = String("/d/") + _deviceId + "/lwt";

    MqttStartOpts opt = {};
    opt.host = _mqttServer.c_str();
    opt.port = static_cast<uint32_t>(_mqttPort);
    opt.clientId = _deviceId;
    opt.user = _deviceId;
    opt.pass = _deviceKey;
    opt.willTopic = _mqttWillTopic.c_str();
    opt.willMsg = "0";
    opt.tls = !_mqttNoTls;
    opt.insecure = _insecure;
    opt.ca = _caCert;
    opt.cert = _clientCert;
    opt.key = _privateKey;
    opt.keepalive = 15;
    opt.bufferSize = 2048;

    Serial.println("Connecting to server...");
    _mqtt = startMqtt(opt, this);
    if (_mqtt == nullptr)
    {
        Serial.println("Failed to start server client");
        return false;
    }

    unsigned long start = millis();
    while (millis() - start < 10000)
    {
        _drainMqttInbox();
        if (_mqttUp && !_mqttHelloPending)
            return true;
        delay(20);
    }
    _drainMqttInbox();
    if (!_mqttUp)
        _Debug("server client started, still waiting to connect");
    return _mqttUp && !_mqttHelloPending;
}

void Firmngin::mqttClientSetBufferSize(uint16_t size)
{
    if (_userMqtt != nullptr)
        return;
    if (size < 128)
        size = 128;
    _userMqttBufferSize = size;
}

void Firmngin::mqttClientSetInsecure(bool insecure)
{
    if (_userMqtt != nullptr)
        return;
    _userMqttInsecure = insecure;
    if (insecure)
        Serial.println("WARNING: user server client TLS verification explicitly disabled");
}

bool Firmngin::mqttClientBegin(const char *host, uint16_t port,
                               const char *clientId, const char *user, const char *pass,
                               const char *willTopic, const char *willMessage,
                               MqttMessageHandler onMessage, bool tls, const char *caCert)
{
    if (_userMqtt != nullptr)
        return true;
    if (host == nullptr || host[0] == '\0' || clientId == nullptr || clientId[0] == '\0')
        return false;

    _userMqttHost = host;
    _userMqttPort = port;
    _userMqttClientId = clientId;
    _userMqttUser = user != nullptr ? user : "";
    _userMqttPass = pass != nullptr ? pass : "";
    _userMqttWillTopic = willTopic != nullptr ? willTopic : "";
    _userMqttWillMsg = willMessage != nullptr ? willMessage : "";
    _userMqttTls = tls;
    _userMqttCallback = onMessage;
    _userMqttCa = (caCert != nullptr && caCert[0] != '\0') ? caCert : "";
    if (_userMqttBufferSize < 128)
        _userMqttBufferSize = 128;
    if (tls && _userMqttCa.length() == 0 && !_userMqttInsecure)
    {
        Serial.println("ERROR: user server TLS needs a CA, or mqttClientSetInsecure(true)");
        return false;
    }
    _ensureMqttInbox();

    MqttStartOpts opt = {};
    opt.host = _userMqttHost.c_str();
    opt.port = _userMqttPort;
    opt.clientId = _userMqttClientId.c_str();
    opt.user = _userMqttUser.length() > 0 ? _userMqttUser.c_str() : nullptr;
    opt.pass = _userMqttPass.length() > 0 ? _userMqttPass.c_str() : nullptr;
    opt.willTopic = _userMqttWillTopic.length() > 0 ? _userMqttWillTopic.c_str() : nullptr;
    opt.willMsg = _userMqttWillMsg.c_str();
    opt.tls = _userMqttTls;
    opt.insecure = _userMqttCa.length() == 0 && _userMqttInsecure;
    opt.ca = _userMqttCa.length() > 0 ? _userMqttCa.c_str() : nullptr;
    opt.keepalive = 30;
    opt.bufferSize = _userMqttBufferSize;
    _userMqtt = startMqtt(opt, this);
    return _userMqtt != nullptr;
}

bool Firmngin::mqttClientSubscribe(const char *topic, int qos)
{
    if (topic == nullptr || topic[0] == '\0')
        return false;
    if (qos < 0)
        qos = 0;
    if (qos > 2)
        qos = 2;
    _ensureMqttInbox();
    if (_mqttLock != nullptr)
    {
        if (xSemaphoreTake(_mqttLock, pdMS_TO_TICKS(5000)) != pdTRUE)
        {
            if (_debug)
                Serial.printf("[FNG-E%d] Device busy: incoming messages not set up (timeout)\n", FMSG_ERR_MUTEX_TIMEOUT);
            return false;
        }
    }
    _userMqttSubs.push_back({String(topic), qos});
    if (_mqttLock != nullptr)
        xSemaphoreGive(_mqttLock);
    if (_userMqtt != nullptr && _userMqttUp)
        return esp_mqtt_client_subscribe(_userMqtt, topic, qos) >= 0;
    return true;
}

bool Firmngin::mqttClientPublish(const char *topic, const char *payload, bool retained)
{
    if (_userMqtt == nullptr || !_userMqttUp || topic == nullptr)
        return false;
    const char *body = payload != nullptr ? payload : "";
    int id = esp_mqtt_client_enqueue(_userMqtt, topic, body, static_cast<int>(strlen(body)), 1, retained ? 1 : 0, true);
    return id >= 0;
}

bool Firmngin::mqttClientConnected() const
{
    return _userMqttUp;
}

void Firmngin::mqttClientLoop()
{
    _drainMqttInbox();
}

void Firmngin::mqttClientDisconnect()
{
    if (_userMqtt == nullptr)
        return;
    esp_mqtt_client_stop(_userMqtt);
    esp_mqtt_client_destroy(_userMqtt);
    _userMqtt = nullptr;
    _userMqttUp = false;
}

uint32_t Firmngin::getDroppedMessageCount() const
{
    return _msgDroppedCount;
}

uint32_t Firmngin::getReconnectCount() const
{
    return _msgPlatformReconnects + _msgUserReconnects;
}

void Firmngin::resetMessageStats()
{
    _msgDroppedCount = 0;
    _msgPlatformReconnects = 0;
    _msgUserReconnects = 0;
}

#else

bool Firmngin::_networkReady() const
{
    return false;
}

bool Firmngin::_mqttIsConnected() const { return false; }
bool Firmngin::_mqttIsStarted() const { return false; }
bool Firmngin::_mqttPublish(const char *, const uint8_t *, size_t, bool) { return false; }
void Firmngin::_ensureMqttInbox() {}
void Firmngin::_drainMqttInbox() {}
void Firmngin::_mqttAfterConnect() {}
void Firmngin::_mqttShutdown() {}
bool Firmngin::connectServer() { return false; }
void Firmngin::mqttClientSetBufferSize(uint16_t) {}
void Firmngin::mqttClientSetInsecure(bool) {}
bool Firmngin::mqttClientBegin(const char *, uint16_t, const char *, const char *, const char *, const char *, const char *, MqttMessageHandler, bool, const char *) { return false; }
bool Firmngin::mqttClientSubscribe(const char *, int) { return false; }
bool Firmngin::mqttClientPublish(const char *, const char *, bool) { return false; }
bool Firmngin::mqttClientConnected() const { return false; }
void Firmngin::mqttClientLoop() {}
void Firmngin::mqttClientDisconnect() {}
uint32_t Firmngin::getDroppedMessageCount() const { return 0; }
uint32_t Firmngin::getReconnectCount() const { return 0; }
void Firmngin::resetMessageStats() {}

#endif
