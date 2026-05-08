#include "firmngin.h"

const char *NTP_SERVER = "pool.ntp.org";
int GMT_OFFSET_SEC = 7 * 3600;
int DAYLIGHT_OFFSET_SEC = 0;

const char *MQTT_SERVER_ADDR = FIRMNGIN_SERVER_ADDR;
const int MQTT_SERVER_PORT = FIRMNGIN_SERVER_PORT;

#if defined(ESP8266)
Firmngin::Firmngin(const char *deviceId, const char *deviceKey)
    : _deviceId(deviceId),
      _deviceKey(deviceKey),
      _debug(false),
      _lastMQTTAttempt(0),
      _mqttClient(_wifiClient),
      _mqttServer(MQTT_SERVER_ADDR),
      _mqttPort(MQTT_SERVER_PORT)
{
    if (!PLATFORM_SUPPORTED)
    {
        Serial.println("ERROR: Platform not supported");
    }
}

Firmngin::Firmngin(const char *deviceId, const char *deviceKey, const char *clientCert, const char *privateKey, const uint8_t *fingerprint)
    : _deviceId(deviceId),
      _deviceKey(deviceKey),
      _debug(false),
      _lastMQTTAttempt(0),
      _mqttClient(_wifiClient),
      _mqttServer(MQTT_SERVER_ADDR),
      _mqttPort(MQTT_SERVER_PORT),
      _clientCert(clientCert),
      _privateKey(privateKey),
      _fingerprint(fingerprint)
{
    if (!PLATFORM_SUPPORTED)
    {
        Serial.println("ERROR: Platform not supported");
    }
}
#elif defined(ESP32)
Firmngin::Firmngin(const char *deviceId, const char *deviceKey)
    : _deviceId(deviceId),
      _deviceKey(deviceKey),
      _debug(false),
      _lastMQTTAttempt(0),
      _mqttClient(_wifiClient),
      _mqttServer(MQTT_SERVER_ADDR),
      _mqttPort(MQTT_SERVER_PORT)
{
    if (!PLATFORM_SUPPORTED)
    {
        Serial.println("ERROR: Platform not supported");
    }
}

Firmngin::Firmngin(const char *deviceId, const char *deviceKey, const char *caCert, const char *clientCert, const char *privateKey)
    : _deviceId(deviceId),
      _deviceKey(deviceKey),
      _debug(false),
      _lastMQTTAttempt(0),
      _mqttClient(_wifiClient),
      _mqttServer(MQTT_SERVER_ADDR),
      _mqttPort(MQTT_SERVER_PORT),
      _caCert(caCert),
      _clientCert(clientCert),
      _privateKey(privateKey),
      _fingerprint(nullptr)
{
    if (!PLATFORM_SUPPORTED)
    {
        Serial.println("ERROR: Platform not supported");
    }
}

Firmngin::Firmngin(const char *deviceId, const char *deviceKey, const uint8_t *fingerprint, const char *clientCert, const char *privateKey)
    : _deviceId(deviceId),
      _deviceKey(deviceKey),
      _debug(false),
      _lastMQTTAttempt(0),
      _mqttClient(_wifiClient),
      _mqttServer(MQTT_SERVER_ADDR),
      _mqttPort(MQTT_SERVER_PORT),
      _caCert(nullptr),
      _clientCert(clientCert),
      _privateKey(privateKey),
      _fingerprint(fingerprint)
{
    if (!PLATFORM_SUPPORTED)
    {
        Serial.println("ERROR: Platform not supported");
    }
}
#else
Firmngin::Firmngin(const char *deviceId, const char *deviceKey)
    : _deviceId(deviceId),
      _deviceKey(deviceKey),
      _debug(false),
      _lastMQTTAttempt(0),
      _mqttClient(_wifiClient),
      _mqttServer(MQTT_SERVER_ADDR),
      _mqttPort(MQTT_SERVER_PORT)
{
    if (!PLATFORM_SUPPORTED)
    {
        Serial.println("ERROR: Platform not supported");
    }
}
#endif

Firmngin::~Firmngin()
{
#if defined(ESP8266) && KEYS_H_AVAILABLE
    delete _clientCertList;
    delete _clientPrivKey;
#endif
}

String Firmngin::getPathPayment(String deviceId)
{
    return String("/c/") + deviceId + "/" + PATH_PAYMENT;
}

String Firmngin::getPathDeviceStatus(String deviceId)
{
    return String("/c/") + deviceId + "/" + PATH_DEVICE_STATUS;
}

String Firmngin::getPathPendingPayment(String deviceId)
{
    return String("/c/") + deviceId + "/" + PATH_PENDING_PAYMENT;
}

String Firmngin::getPathMetadataOnPending(String deviceId)
{
    return String("/c/") + deviceId + "/" + PATH_METADATA_ON_PENDING;
}

String Firmngin::getPathMetadataOnExpired(String deviceId)
{
    return String("/c/") + deviceId + "/" + PATH_METADATA_ON_EXPIRED;
}

String Firmngin::getPathMetadataOnSuccess(String deviceId)
{
    return String("/c/") + deviceId + "/" + PATH_METADATA_ON_SUCCESS;
}

String Firmngin::getTopicInit(String deviceId)
{
    return String("/c/") + deviceId + "/init";
}

String Firmngin::getTopicDisplayPIN(String deviceId)
{
    return String("/c/") + deviceId + "/dpin";
}

String Firmngin::getTopicVerificationResult(String deviceId)
{
    return String("/c/") + deviceId + "/vr";
}

String Firmngin::getTopicUsageResponse(String deviceId)
{
    return String("/c/") + deviceId + "/ur";
}

String Firmngin::getTopicLimitExceeded(String deviceId)
{
    return String("/c/") + deviceId + "/le";
}

String Firmngin::getTopicNearLimit(String deviceId)
{
    return String("/c/") + deviceId + "/nl";
}

String Firmngin::getTopicUpdateEntity(String deviceId)
{
    return String("/d/") + deviceId + "/ps";
}

String Firmngin::getTopicUpdateEntities(String deviceId)
{
    return String("/d/") + deviceId + "/psb";
}

String Firmngin::getTopicRequestInit(String deviceId)
{
    return String("/d/") + deviceId + "/a/init";
}

String Firmngin::getTopicEntityCommand(String deviceId)
{
    return String("/d/") + deviceId + "/rs/+";
}

static void printBanner()
{
    Serial.println();
    Serial.println(F("   __             _            _            "));
    Serial.println(F("  / _|           (_)          | |           "));
    Serial.println(F(" | |_ _ __   __ _ _ _ __    __| | _____   __"));
    Serial.println(F(" |  _| '_ \\ / _' | | '_ \\  / _' |/ _ \\ \\ / /"));
    Serial.println(F(" | | | | | | (_| | | | | || (_| |  __/\\ V / "));
    Serial.println(F(" |_| |_| |_|\\__, |_|_| |_(_)__,_|\\___| \\_/  "));
    Serial.println(F("             __/ |                          "));
    Serial.println(F("            |___/                           "));
    Serial.println();
    Serial.println(F("firmngin.dev AIoT Platform    "));
    Serial.println();
}

void Firmngin::begin()
{
    if (!PLATFORM_SUPPORTED)
        return;

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("ERROR: WiFi not connected");
        delay(2000);
        ESP.restart();
        return;
    }

    syncTime();

    if (_debug)
    {
        time_t now = time(nullptr);
        Serial.print("Epoch: ");
        Serial.println((unsigned long)now);
    }

    printBanner();

#if defined(ESP8266)
    const char *clientCert = _clientCert;
    const char *privateKey = _privateKey;
    const uint8_t *fingerprint = _fingerprint;

#if KEYS_H_AVAILABLE
    if (clientCert == nullptr)
        clientCert = CLIENT_CERT;
    if (privateKey == nullptr)
        privateKey = PRIVATE_KEY;
#if defined(USE_FINGERPRINT)
    if (fingerprint == nullptr)
        fingerprint = SERVER_FINGERPRINT_BYTES;
#elif defined(USE_CA_CERT)
    // ESP8266 CA cert mode: use trust anchors (memory heavy but possible)
    // Note: user must define USE_CA_CERT in keys.h or sketch
#else
    // Default: fingerprint
    if (fingerprint == nullptr)
        fingerprint = SERVER_FINGERPRINT_BYTES;
#endif
#endif

    if (clientCert == nullptr || privateKey == nullptr)
    {
        Serial.println("ERROR: mTLS credentials not configured");
        return;
    }

    _clientCertList = new BearSSL::X509List(clientCert);
    _clientPrivKey = new BearSSL::PrivateKey(privateKey);
    _wifiClient.setClientRSACert(_clientCertList, _clientPrivKey);
    _wifiClient.setBufferSizes(512, 512);

#if defined(USE_CA_CERT) && KEYS_H_AVAILABLE
    _wifiClient.setTrustAnchors(new BearSSL::X509List(CA_CERT));
    Serial.println("Server validation: CA Certificate (USE_CA_CERT)");
#elif fingerprint != nullptr
    _wifiClient.setFingerprint(fingerprint);
    Serial.println("Server validation: Fingerprint");
#else
    Serial.println("ERROR: No server validation method configured");
    return;
#endif

#elif defined(ESP32)
    const char *caCert = _caCert;
    const char *clientCert = _clientCert;
    const char *privateKey = _privateKey;

#if KEYS_H_AVAILABLE
    // ESP32: Always use CA cert mode (fingerprint not supported in newer ESP32 cores)
    if (caCert == nullptr)
        caCert = CA_CERT;
    if (clientCert == nullptr)
        clientCert = CLIENT_CERT;
    if (privateKey == nullptr)
        privateKey = PRIVATE_KEY;
#endif

    if (clientCert == nullptr || privateKey == nullptr)
    {
        Serial.println("ERROR: mTLS credentials not configured");
        return;
    }

    if (_insecure)
    {
        _wifiClient.setInsecure();
        Serial.println("Server validation: DISABLED (setInsecure)");
    }
    else if (caCert != nullptr)
    {
        _wifiClient.setCACert(caCert);
        Serial.println("Server validation: CA Certificate");
    }
    else
    {
        _wifiClient.setInsecure();
        Serial.println("Server validation: DISABLED (no CA cert)");
    }
    _wifiClient.setCertificate(clientCert);
    _wifiClient.setPrivateKey(privateKey);
#endif

    _mqttClient.setServer(_mqttServer.c_str(), _mqttPort);
    _mqttClient.setCallback([this](char *topic, byte *payload, unsigned int length)
                            { this->mqttCallback(topic, payload, length); });
    _mqttClient.setBufferSize(2048);
    _mqttClient.setKeepAlive(30);
    _mqttClient.setSocketTimeout(10);

    for (const auto &entry : deferredEntityRegistrations())
    {
        _entityCallbacks[entry.first] = entry.second;
    }
}

void Firmngin::setMQTTServer(const char *server, int port)
{
    _mqttServer = server;
    _mqttPort = port;
}

void Firmngin::setInsecure(bool insecure)
{
    _insecure = insecure;
}

void Firmngin::setTimezone(int timezone)
{
    if (timezone < -12 || timezone > 12)
        return;
    GMT_OFFSET_SEC = timezone * 3600;
}

void Firmngin::setNtpServer(const char *ntpServer)
{
    NTP_SERVER = ntpServer;
}

void Firmngin::setClient(Client &client)
{
    _mqttClient.setClient(client);
}

void Firmngin::setDaylightOffsetSec(int daylightOffsetSec)
{
    DAYLIGHT_OFFSET_SEC = daylightOffsetSec;
}

void Firmngin::syncTime()
{
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    time_t now = time(nullptr);
    int timeout = 0;
    while (now < 8 * 3600 * 2 && timeout < 100)
    {
        delay(100);
        now = time(nullptr);
        timeout++;
    }
}

void Firmngin::on(const char *state, StateCallbackFunction callback)
{
    _callbacks[String(state)] = callback;
}

void Firmngin::on(DeviceStateType state, StateCallbackFunction callback)
{
    _callbacks[String(STATE_NAMES[state])] = callback;
}

void Firmngin::on(DeviceStateType state, VerificationCallbackFunction callback)
{
    (void)state;
    _verificationCallback = callback;
}

void Firmngin::on(DeviceStateType state, PaymentCallbackFunction callback)
{
    (void)state;
    _paymentsCallback = callback;
}

// Verifications constructor: parses dpin or vr JSON automatically
Verifications::Verifications(const String &jsonPayload)
{
    _valid = false;
    _pin = "";
    _sessionId = "";
    _ttl = 0;
    _pinMet = false;
    _preconditionMet = false;
    _rawPayload = jsonPayload;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonPayload);
    if (error)
        return;

    // dpin fields
    if (doc["pi"].is<const char *>())
    {
        _pin = doc["pi"].as<String>();
    }
    if (doc["si"].is<const char *>())
    {
        _sessionId = doc["si"].as<String>();
    }
    if (doc["ttl"].is<int>())
    {
        _ttl = doc["ttl"].as<int>();
    }

    // vr fields
    if (doc["pn"].is<bool>())
    {
        _pinMet = doc["pn"].as<bool>();
    }
    if (doc["pr"].is<bool>())
    {
        _preconditionMet = doc["pr"].as<bool>();
    }

    _valid = _pin.length() > 0 || doc["pn"].is<bool>();
}

// Payments constructor: parses pp or pm JSON automatically
Payments::Payments(const String &jsonPayload)
{
    _valid = false;
    _itemTitle = "";
    _price = "";
    _orderId = "";
    _isPending = false;
    _isSuccess = false;
    _rawPayload = jsonPayload;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonPayload);
    if (error)
        return;

    if (doc["it"].is<const char *>())
    {
        _itemTitle = doc["it"].as<String>();
    }
    if (doc["pc"].is<const char *>())
    {
        _price = doc["pc"].as<String>();
    }
    if (doc["oid"].is<const char *>())
    {
        _orderId = doc["oid"].as<String>();
    }

    _valid = _itemTitle.length() > 0 || _orderId.length() > 0;
}

void Firmngin::on(DeviceStateType state, UsageCallbackFunction callback)
{
    (void)state;
    _usagesCallback = callback;
}

void Firmngin::on(DeviceStateType state, DeviceStateCallbackFunction callback)
{
    (void)state;
    _deviceStateCallback = callback;
}

void Firmngin::on(DeviceStateType state, InitCallbackFunction callback)
{
    (void)state;
    _initCallback = callback;
}

void Firmngin::on(DeviceStateType state, EntityCommandCallbackFunction callback)
{
    (void)state;
    _entityCallback = callback;
}

void Firmngin::onEntity(const char *key, EntityCommandCallbackFunction callback)
{
    _entityCallbacks[String(key)] = callback;
}

// Inits constructor: parses init JSON automatically
Inits::Inits(const String &jsonPayload)
{
    _valid = false;
    _entitiesJson = "";
    _merchantStatus = "";
    _verificationFlag = 0;
    _rawPayload = jsonPayload;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonPayload);
    if (error)
        return;

    if (doc["e"].is<JsonArray>() || doc["e"].is<JsonObject>())
    {
        String entitiesStr;
        serializeJson(doc["e"], entitiesStr);
        _entitiesJson = entitiesStr;
    }
    if (doc["m"].is<const char *>())
    {
        _merchantStatus = doc["m"].as<String>();
    }
    if (doc["vf"].is<int>())
    {
        _verificationFlag = doc["vf"].as<int>();
    }

    _valid = doc["m"].is<const char *>();
}

// DeviceStates constructor: parses ds JSON automatically
DeviceStates::DeviceStates(const String &jsonPayload)
{
    _valid = false;
    _state = "";
    _rawPayload = jsonPayload;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonPayload);
    if (error)
        return;

    if (doc["s"].is<const char *>())
    {
        _state = doc["s"].as<String>();
        _valid = true;
    }
}

// Usages constructor: parses ur, le, or nl JSON automatically
Usages::Usages(const String &jsonPayload)
{
    _valid = false;
    _used = 0;
    _limit = 0;
    _remaining = 0;
    _percentage = 0;
    _resetAt = "";
    _granularity = "";
    _isNearLimit = false;
    _isLimitExceeded = false;
    _rawPayload = jsonPayload;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonPayload);
    if (error)
        return;

    if (doc["u"].is<int>())
    {
        _used = doc["u"].as<int>();
    }
    if (doc["l"].is<int>())
    {
        _limit = doc["l"].as<int>();
    }
    if (doc["r"].is<int>())
    {
        _remaining = doc["r"].as<int>();
    }
    if (doc["pct"].is<int>())
    {
        _percentage = doc["pct"].as<int>();
    }
    if (doc["ra"].is<const char *>())
    {
        _resetAt = doc["ra"].as<String>();
    }
    if (doc["g"].is<const char *>())
    {
        _granularity = doc["g"].as<String>();
    }

    _valid = doc["u"].is<int>() || doc["l"].is<int>();
}

void Firmngin::setupLWT()
{
    String willTopic = "/d/" + String(_deviceId) + "/lwt";
}

void Firmngin::loop()
{
    if (!PLATFORM_SUPPORTED || WiFi.status() != WL_CONNECTED)
        return;

    static unsigned long lastReconnectAttempt = 0;
    static int backoffDelay = 5000;
    static bool firstConnect = true;

    if (!_mqttClient.connected())
    {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > backoffDelay)
        {
            lastReconnectAttempt = now;
            backoffDelay = min(backoffDelay * 2, 60000);
            if (connectServer())
            {
                backoffDelay = 5000;
                firstConnect = false;
            }
        }
    }
    else
    {
        _mqttClient.loop();
    }
}

void Firmngin::_Debug(String message, bool newLine)
{
    if (_debug)
    {
        if (newLine)
            Serial.println(message);
        else
            Serial.print(message);
    }
}

void Firmngin::setDebug(bool debug)
{
    _debug = debug;
}

bool Firmngin::isPlatformSupported()
{
    return PLATFORM_SUPPORTED;
}

bool Firmngin::connectServer()
{
    setupLWT();

    int retryCount = 0;
    while (!_mqttClient.connected() && retryCount < maxRetryMQTT)
    {
        unsigned long now = millis();
        if (now - _lastMQTTAttempt >= _delayRetryMQTT)
        {
            _lastMQTTAttempt = now;
            Serial.print("Connecting to server...");
            Serial.print(" (attempt ");
            Serial.print(retryCount + 1);
            Serial.println(")");

            String willTopic = "/d/" + String(_deviceId) + "/lwt";
            String willMessage = "0";

            _mqttClient.disconnect();
            _wifiClient.stop();
            delay(100);
            bool connected = _mqttClient.connect(_deviceId, _deviceId, _deviceKey, willTopic.c_str(), 1, true, willMessage.c_str());

            if (connected)
            {
                _mqttClient.subscribe(getPathPayment(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getPathDeviceStatus(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getPathPendingPayment(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getPathMetadataOnPending(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getPathMetadataOnExpired(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getPathMetadataOnSuccess(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getTopicInit(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getTopicDisplayPIN(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getTopicVerificationResult(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getTopicUsageResponse(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getTopicLimitExceeded(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getTopicNearLimit(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getTopicEntityCommand(_deviceId).c_str(), defaultQos);

                _mqttClient.publish(willTopic.c_str(), "", true);
                delay(10);
                _mqttClient.publish(willTopic.c_str(), "1", true);

                Serial.println("Connected.");
                return true;
            }
            else
            {
                int mqttState = _mqttClient.state();
                Serial.print("Failed, rc=");
                Serial.print(mqttState);
                if (_debug)
                {
                    Serial.print(" (");
                    switch (mqttState)
                    {
                    case -4:
                        Serial.print("MQTT_CONNECTION_TIMEOUT");
                        break;
                    case -3:
                        Serial.print("MQTT_CONNECTION_LOST");
                        break;
                    case -2:
                        Serial.print("MQTT_CONNECT_FAILED - TLS/certificate error");
                        break;
                    case -1:
                        Serial.print("MQTT_DISCONNECTED");
                        break;
                    case 1:
                        Serial.print("MQTT_CONNECT_BAD_PROTOCOL");
                        break;
                    case 2:
                        Serial.print("MQTT_CONNECT_BAD_CLIENT_ID");
                        break;
                    case 3:
                        Serial.print("MQTT_CONNECT_UNAVAILABLE");
                        break;
                    case 4:
                        Serial.print("MQTT_CONNECT_BAD_CREDENTIALS");
                        break;
                    case 5:
                        Serial.print("MQTT_CONNECT_UNAUTHORIZED");
                        break;
                    default:
                        Serial.print("UNKNOWN");
                        break;
                    }
                    Serial.print(")");
                }
                Serial.println();
                retryCount++;
            }
        }
    }

    if (!_mqttClient.connected())
    {
        Serial.println("Connection failed, restarting...");
        delay(1000);
        ESP.restart();
    }

    return false;
}

void Firmngin::mqttCallback(char *topic, byte *payload, unsigned int length)
{
    String payloadStr;
    payloadStr.reserve(length);
    for (unsigned int i = 0; i < length; i++)
    {
        payloadStr += (char)payload[i];
    }

    String topicStr = String(topic);
    String stateType = "";

    int lastSlash = topicStr.lastIndexOf('/');
    if (lastSlash >= 0)
    {
        stateType = topicStr.substring(lastSlash + 1);
    }

    DeviceState state(stateType, payloadStr);

    if (_callbacks.count(stateType) > 0)
    {
        _callbacks[stateType](state);
    }

    // Typed callbacks: auto-parse, no manual JSON needed
    if ((stateType == "dpin" || stateType == "vr") && _verificationCallback)
    {
        Verifications v(payloadStr);
        if (v.isValid())
        {
            _verificationCallback(v);
        }
    }

    if ((stateType == "pp" || stateType == "pm") && _paymentsCallback)
    {
        Payments p(payloadStr);
        if (p.isValid())
        {
            p.setPending(stateType == "pp");
            p.setSuccess(stateType == "pm");
            _paymentsCallback(p);
        }
    }

    if ((stateType == "ur" || stateType == "le" || stateType == "nl") && _usagesCallback)
    {
        Usages u(payloadStr);
        if (u.isValid())
        {
            u.setNearLimit(stateType == "nl");
            u.setLimitExceeded(stateType == "le");
            _usagesCallback(u);
        }
    }

    if (stateType == "ds" && _deviceStateCallback)
    {
        DeviceStates ds(payloadStr);
        if (ds.isValid())
        {
            _deviceStateCallback(ds);
        }
    }

    if (stateType == "init" && _initCallback)
    {
        Inits i(payloadStr);
        if (i.isValid())
        {
            _initCallback(i);
        }
    }

    if (topicStr.indexOf("/rs/") >= 0)
    {
        int rsIndex = topicStr.indexOf("/rs/");
        String entityKey = topicStr.substring(rsIndex + 4); // after "/rs/"
        EntityCommand cmd(entityKey, payloadStr);

        // Check per-key callback first, fallback to global ENTITIES callback
        if (_entityCallbacks.count(entityKey) > 0)
        {
            _entityCallbacks[entityKey](cmd);
        }
        else if (_entityCallback)
        {
            _entityCallback(cmd);
        }
    }
}

bool Firmngin::pushEntity(const char *key, const char *value)
{
    if (!_mqttClient.connected())
        return false;

    JsonDocument doc;
    doc["key"] = key;
    doc["value"] = value;

    String payload;
    serializeJson(doc, payload);

    return _mqttClient.publish(getTopicUpdateEntity(_deviceId).c_str(), payload.c_str());
}

bool Firmngin::pushEntity(Entity &entity, const char *value)
{
    return pushEntity(entity.key().c_str(), value);
}

bool Firmngin::updateEntities(const char *jsonPayload)
{
    if (!_mqttClient.connected())
        return false;
    return _mqttClient.publish(getTopicUpdateEntities(_deviceId).c_str(), jsonPayload);
}

bool Firmngin::requestInit()
{
    if (!_mqttClient.connected())
        return false;
    return _mqttClient.publish(getTopicRequestInit(_deviceId).c_str(), "{}");
}

BatchState Firmngin::pushBatchEntities()
{
    return BatchState(this);
}

bool BatchState::send()
{
    if (_instance == nullptr)
        return false;
    String payload;
    serializeJson(_doc, payload);
    return _instance->updateEntities(payload.c_str());
}
