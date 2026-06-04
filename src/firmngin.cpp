#include "firmngin.h"

#if defined(ESP32)
#include <esp_system.h>
#elif defined(ESP8266)
extern "C"
{
#include "user_interface.h"
}
#endif

#if defined(ESP32) || defined(ESP8266)
#include <Update.h>
#endif

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
      _mqttPort(MQTT_SERVER_PORT),
      _e2eeEnabled(false)
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
      _e2eeEnabled(false),
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
      _mqttPort(MQTT_SERVER_PORT),
      _e2eeEnabled(false)
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
      _e2eeEnabled(false),
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
      _e2eeEnabled(false),
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
      _mqttPort(MQTT_SERVER_PORT),
      _e2eeEnabled(false)
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
    delete _trustAnchors;
    delete _otaTrustAnchors;
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

String Firmngin::getPathInit(String deviceId)
{
    return String("/c/") + deviceId + "/init";
}

String Firmngin::getPathDisplayPIN(String deviceId)
{
    return String("/c/") + deviceId + "/dpin";
}

String Firmngin::getPathVerificationResult(String deviceId)
{
    return String("/c/") + deviceId + "/vr";
}

String Firmngin::getPathUsageResponse(String deviceId)
{
    return String("/c/") + deviceId + "/ur";
}

String Firmngin::getPathLimitExceeded(String deviceId)
{
    return String("/c/") + deviceId + "/le";
}

String Firmngin::getPathNearLimit(String deviceId)
{
    return String("/c/") + deviceId + "/nl";
}

String Firmngin::getPathUpdateEntity(String deviceId)
{
    return String("/d/") + deviceId + "/ps";
}

String Firmngin::getPathUpdateEntities(String deviceId)
{
    return String("/d/") + deviceId + "/psb";
}

String Firmngin::getPathRequestInit(String deviceId)
{
    return String("/d/") + deviceId + "/a/init";
}

String Firmngin::getPathSessionEnd(String deviceId)
{
    return String("/d/") + deviceId + "/a/ses-n";
}

String Firmngin::getPathEntityCommand(String deviceId)
{
    return String("/d/") + deviceId + "/rs/+";
}

String Firmngin::getPathPing(String deviceId)
{
    return String("/c/") + deviceId + "/" + PATH_PING;
}

String Firmngin::getOTATriggerPath(String deviceId)
{
    return String("/c/") + deviceId + "/ot/trg";
}

static void printBanner()
{
    Serial.println();
    Serial.println(F("   __ _                            _            _            "));
    Serial.println(F("  / _(_)                          (_)          | |           "));
    Serial.println(F(" | |_ _ _ __ _ __ ___  _ __   __ _ _ _ __    __| | _____   __"));
    Serial.println(F(" |  _| | '__| '_ ` _ \\| '_ \\ / _` | | '_ \\  / _` |/ _ \\ \\ / /"));
    Serial.println(F(" | | | | |  | | | | | | | | | (_| | | | | || (_| |  __/\\ V / "));
    Serial.println(F(" |_| |_|_|  |_| |_| |_|_| |_|\\__, |_|_| |_(_)__,_|\\___| \\_/  "));
    Serial.println(F("                              __/ |                          "));
    Serial.println(F(" firmngin.dev AIoT Platform  |___/                           "));
    // Serial.println(F("firmngin.dev AIoT Platform"));
    Serial.println();
}

void Firmngin::begin()
{
    if (!PLATFORM_SUPPORTED)
        return;

    _topicUpdateEntity = "/d/" + String(_deviceId) + "/ps";
    _topicUpdateEntities = "/d/" + String(_deviceId) + "/psb";

    if (WiFi.status() != WL_CONNECTED)
    {
        _Debug("wifi not connected, call begin() after WiFi is up");
        return;
    }

    syncTime();
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

    if (_clientCertList != nullptr)
    {
        delete _clientCertList;
    }
    if (_clientPrivKey != nullptr)
    {
        delete _clientPrivKey;
    }
    _clientCertList = new BearSSL::X509List(clientCert);
    _clientPrivKey = new BearSSL::PrivateKey(privateKey);
    _wifiClient.setClientRSACert(_clientCertList, _clientPrivKey);
    _wifiClient.setBufferSizes(512, 512);

#if defined(USE_CA_CERT) && KEYS_H_AVAILABLE
    if (_trustAnchors != nullptr)
    {
        delete _trustAnchors;
    }
    _trustAnchors = new BearSSL::X509List(CA_CERT);
    _wifiClient.setTrustAnchors(_trustAnchors);
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

    _caCert = caCert;
    _clientCert = clientCert;
    _privateKey = privateKey;

    if (_insecure)
    {
        _wifiClient.setInsecure();
        Serial.println("Server validation: DISABLED (setInsecure)");
    }
    else if (caCert != nullptr)
    {
        _wifiClient.setCACert(caCert);
    }
    else
    {
        _wifiClient.setInsecure();
        Serial.println("Server validation: DISABLED (no CA cert)");
    }
    _wifiClient.setCertificate(clientCert);
    _wifiClient.setPrivateKey(privateKey);
#endif

#if KEYS_H_AVAILABLE && defined(DECRYPTOR)
    // Auto-load E2EE key from keys.h
    if (strlen(DECRYPTOR) > 0)
    {
        size_t hexLen = strlen(DECRYPTOR);
        size_t keyLen = hexLen / 2;
        if (keyLen > 32)
            keyLen = 32;
        for (size_t i = 0; i < keyLen; i++)
        {
            char c1 = DECRYPTOR[i * 2];
            char c2 = DECRYPTOR[i * 2 + 1];
            uint8_t d1 = (c1 >= 'a') ? (c1 - 'a' + 10) : (c1 >= 'A' ? c1 - 'A' + 10 : c1 - '0');
            uint8_t d2 = (c2 >= 'a') ? (c2 - 'a' + 10) : (c2 >= 'A' ? c2 - 'A' + 10 : c2 - '0');
            _e2eeKeyBytes[i] = (d1 << 4) | d2;
        }
        _e2eeKeyLen = keyLen;
        _e2eeEnabled = true;
    }
#endif

    if (!_e2eeEnabled && _debug)
    {
        Serial.println("E2EE: encryption disabled — payload publish blocked for security");
    }

    _mqttClient.setServer(_mqttServer.c_str(), _mqttPort);
    _mqttClient.setCallback([this](char *path, byte *payload, unsigned int length)
                            { this->mqttCallback(path, payload, length); });
    _mqttClient.setBufferSize(2048);
    _mqttClient.setKeepAlive(30);
    _mqttClient.setSocketTimeout(10);

    if (!_deferredCallbacksLoaded)
    {
        for (const auto &entry : deferredStateRegistrations())
        {
            _stateCallbacks.push_back(entry);
        }
        for (const auto &entry : deferredEntityRegistrations())
        {
            _entityCallbacks[entry.first] = entry.second;
        }
        for (const auto &callback : deferredEntitiesRegistrations())
        {
            _entityGlobalCallbacks.push_back(callback);
        }
        for (const auto &callback : deferredVerificationRegistrations())
        {
            _verificationCallbacks.push_back(callback);
        }
        for (const auto &callback : deferredPaymentRegistrations())
        {
            _paymentCallbacks.push_back(callback);
        }
        for (const auto &callback : deferredUsageRegistrations())
        {
            _usageCallbacks.push_back(callback);
        }
        for (const auto &callback : deferredDeviceStatusRegistrations())
        {
            _deviceStateCallbacks.push_back(callback);
        }
        for (const auto &callback : deferredInitRegistrations())
        {
            _initCallbacks.push_back(callback);
        }
        for (const auto &callback : deferredActiveSessionRegistrations())
        {
            _activeSessionCallbacks.push_back(callback);
        }
        for (const auto &callback : deferredOTAStatusRegistrations())
        {
            _otaCallbacks.push_back(callback);
        }
        _deferredCallbacksLoaded = true;
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
    _ntpSyncStartMs = millis();
    _ntpSynced = false;
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

    firmngin_json::Parser p(jsonPayload.c_str(), jsonPayload.length());
    char buf[128];

    if (p.getString("pi", buf, sizeof(buf)) > 0)
    {
        _pin = String(buf);
    }
    if (p.getString("si", buf, sizeof(buf)) > 0)
    {
        _sessionId = String(buf);
    }
    _ttl = p.getInt("ttl", 0);
    _pinMet = p.getBool("pn", false);
    _preconditionMet = p.getBool("pr", false);

    _valid = _pin.length() > 0 || p.has("pn");
}

// Payments constructor: parses pp or pm JSON automatically
Payments::Payments(const String &jsonPayload)
{
    _valid = false;
    _itemTitle = "";
    _price = "";
    _orderId = "";
    _quantity = 1;
    _isPending = false;
    _isSuccess = false;
    _rawPayload = jsonPayload;

    firmngin_json::Parser p(jsonPayload.c_str(), jsonPayload.length());
    char buf[128];

    if (p.getString("it", buf, sizeof(buf)) > 0)
    {
        _itemTitle = String(buf);
    }
    if (p.getString("pc", buf, sizeof(buf)) > 0)
    {
        _price = String(buf);
    }
    if (p.getString("oid", buf, sizeof(buf)) > 0)
    {
        _orderId = String(buf);
    }
    _quantity = p.getInt("q", 1);
    if (_quantity < 1)
        _quantity = 1;

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
    if (state == ENTITIES)
    {
        _entityGlobalCallbacks.push_back(callback);
        return;
    }
    _entityCallback = callback;
}

void Firmngin::on(DeviceStateType state, ActiveSessionCallbackFunction callback)
{
    if (state == ACTIVE_SESSION || state == ON_ACTIVE_SESSION)
    {
        _activeSessionCallbacks.push_back(callback);
    }
}

void Firmngin::onEntity(const char *key, EntityCommandCallbackFunction callback)
{
    _entityCallbacks[String(key)] = callback;
}

EntityValue ActiveSession::entity(const char *key) const
{
    if (_instance == nullptr)
        return EntityValue();
    return _instance->entity(key);
}

EntityValue ActiveSession::entity(Entity &entity) const
{
    return this->entity(entity.key().c_str());
}

bool ActiveSession::endSession()
{
    if (_instance != nullptr)
    {
        return _instance->endSession();
    }
    return false;
}

// Inits constructor: parses init JSON automatically
Inits::Inits(const String &jsonPayload)
{
    _valid = false;
    _entitiesJson = "";
    _merchantStatus = "";
    _activeOrderId = "";
    _verificationFlag = 0;
    _rawPayload = jsonPayload;

    firmngin_json::Parser p(jsonPayload.c_str(), jsonPayload.length());
    char buf[128];

    size_t rawLen = 0;
    const char *raw = p.getRaw("e", rawLen);
    if (raw != nullptr && rawLen > 0)
    {
        _entitiesJson = String(raw).substring(0, rawLen);
    }
    if (p.getString("m", buf, sizeof(buf)) > 0)
    {
        _merchantStatus = String(buf);
    }
    if (p.getString("oid", buf, sizeof(buf)) > 0)
    {
        _activeOrderId = String(buf);
    }
    _verificationFlag = p.getInt("vf", 0);

    _valid = p.has("m");
}

// DeviceStates constructor: parses ds JSON automatically
DeviceStates::DeviceStates(const String &jsonPayload)
{
    _valid = false;
    _state = "";
    _rawPayload = jsonPayload;

    firmngin_json::Parser p(jsonPayload.c_str(), jsonPayload.length());
    char buf[64];

    if (p.getString("s", buf, sizeof(buf)) > 0)
    {
        _state = String(buf);
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

    firmngin_json::Parser p(jsonPayload.c_str(), jsonPayload.length());
    char buf[64];

    _used = p.getInt("u", 0);
    _limit = p.getInt("l", 0);
    _remaining = p.getInt("r", 0);
    _percentage = p.getInt("pct", 0);
    if (p.getString("ra", buf, sizeof(buf)) > 0)
    {
        _resetAt = String(buf);
    }
    if (p.getString("g", buf, sizeof(buf)) > 0)
    {
        _granularity = String(buf);
    }

    _valid = p.has("u") || p.has("l");
}

void Firmngin::setupLWT()
{
}

void Firmngin::loop()
{
    if (!PLATFORM_SUPPORTED || WiFi.status() != WL_CONNECTED)
        return;

    if (_ntpSyncStartMs > 0 && !_ntpSynced)
    {
        time_t now = time(nullptr);
        if (now >= 8 * 3600 * 2 || (unsigned long)(millis() - _ntpSyncStartMs) > 10000UL)
        {
            _ntpSynced = true;
        }
    }

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
        if (_queueEnabled && _queueFileReady && _queueCount > 0)
        {
            _drainQueue();
        }
    }

    _processOTA();
    runActiveSessionHandlers();
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

            String willPath = "/d/" + String(_deviceId) + "/lwt";
            String willMessage = "0";

            _mqttClient.disconnect();
            _wifiClient.stop();
            delay(100);
            bool connected = _mqttClient.connect(_deviceId, _deviceId, _deviceKey, willPath.c_str(), 1, true, willMessage.c_str());

            if (connected)
            {
                char topicBuf[64];
                const int qosState = 0;
                const int qosPayment = 1;

                snprintf(topicBuf, sizeof(topicBuf), "/c/%s/pm", _deviceId);
                _mqttClient.subscribe(topicBuf, qosPayment);
                snprintf(topicBuf, sizeof(topicBuf), "/c/%s/pp", _deviceId);
                _mqttClient.subscribe(topicBuf, qosPayment);
                snprintf(topicBuf, sizeof(topicBuf), "/c/%s/mi", _deviceId);
                _mqttClient.subscribe(topicBuf, qosState);
                snprintf(topicBuf, sizeof(topicBuf), "/c/%s/ds", _deviceId);
                _mqttClient.subscribe(topicBuf, qosState);
                snprintf(topicBuf, sizeof(topicBuf), "/c/%s/init", _deviceId);
                _mqttClient.subscribe(topicBuf, qosState);
                snprintf(topicBuf, sizeof(topicBuf), "/c/%s/dpin", _deviceId);
                _mqttClient.subscribe(topicBuf, qosState);
                snprintf(topicBuf, sizeof(topicBuf), "/c/%s/vr", _deviceId);
                _mqttClient.subscribe(topicBuf, qosState);
                snprintf(topicBuf, sizeof(topicBuf), "/c/%s/ur", _deviceId);
                _mqttClient.subscribe(topicBuf, qosState);
                snprintf(topicBuf, sizeof(topicBuf), "/c/%s/le", _deviceId);
                _mqttClient.subscribe(topicBuf, qosState);
                snprintf(topicBuf, sizeof(topicBuf), "/c/%s/nl", _deviceId);
                _mqttClient.subscribe(topicBuf, qosState);
                snprintf(topicBuf, sizeof(topicBuf), "/c/%s/mop", _deviceId);
                _mqttClient.subscribe(topicBuf, qosState);
                snprintf(topicBuf, sizeof(topicBuf), "/c/%s/moe", _deviceId);
                _mqttClient.subscribe(topicBuf, qosState);
                snprintf(topicBuf, sizeof(topicBuf), "/c/%s/mos", _deviceId);
                _mqttClient.subscribe(topicBuf, qosState);
                snprintf(topicBuf, sizeof(topicBuf), "/c/%s/pi", _deviceId);
                _mqttClient.subscribe(topicBuf, qosState);
                snprintf(topicBuf, sizeof(topicBuf), "/c/%s/ot/trg", _deviceId);
                _mqttClient.subscribe(topicBuf, qosState);
                snprintf(topicBuf, sizeof(topicBuf), "/d/%s/rs/+", _deviceId);
                _mqttClient.subscribe(topicBuf, qosState);

                _mqttClient.publish(willPath.c_str(), "1", true);
                if (_debug)
                    Serial.println("Successfully connected");
                syncFirmwareInfo();

                if (_queueEnabled && _queueFileReady && _queueCount > 0)
                {
                    _lastQueueDrainMs = 0;
                    _drainQueue();
                }

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
        _Debug("Server connection failed, will retry on next loop");
    }

    return false;
}

static void fillE2EENonce(uint8_t *iv, size_t len)
{
#if defined(ESP32)
    esp_fill_random(iv, len);
#elif defined(ESP8266)
    if (!os_get_random(iv, len))
    {
        for (size_t i = 0; i < len; i++)
        {
            iv[i] = (uint8_t)random(0, 256);
        }
    }
#else
    for (size_t i = 0; i < len; i++)
    {
        iv[i] = (uint8_t)random(0, 256);
    }
#endif
}

static bool encryptE2EE(const uint8_t *plaintext, size_t plainLen, const uint8_t *key, size_t keyLen, uint8_t *packet, size_t *packetLen)
{
    *packetLen = 0;

    uint8_t *iv = packet;
    uint8_t *ciphertext = packet + 12;
    uint8_t *tag = packet + 12 + plainLen;
    fillE2EENonce(iv, 12);

#if defined(ESP32)
    if (keyLen != 16 && keyLen != 32)
        return false;

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);

    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, keyLen == 16 ? 128 : 256);
    if (ret != 0)
    {
        mbedtls_gcm_free(&gcm);
        return false;
    }

    ret = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, plainLen, iv, 12, NULL, 0, plaintext, ciphertext, 16, tag);
    mbedtls_gcm_free(&gcm);

    if (ret != 0)
        return false;

    *packetLen = 12 + plainLen + 16;
    return true;

#elif defined(ESP8266)
    if (keyLen != 32)
        return false;

    ChaChaPoly chacha;
    if (!chacha.setKey(key, 32))
        return false;
    if (!chacha.setIV(iv, 12))
        return false;

    chacha.encrypt(ciphertext, plaintext, plainLen);
    chacha.computeTag(tag, 16);

    *packetLen = 12 + plainLen + 16;
    return true;
#else
    (void)plaintext;
    (void)plainLen;
    (void)key;
    (void)keyLen;
    (void)packet;
    return false;
#endif
}

static bool decryptE2EE(const uint8_t *packet, size_t packetLen, const uint8_t *key, size_t keyLen, uint8_t *plaintext, size_t *plainLen)
{
    *plainLen = 0;

    // Minimum: 12 bytes nonce + 16 bytes tag
    if (packetLen < 28)
        return false;

    const uint8_t *iv = packet;
    size_t cipherLen = packetLen - 12 - 16;
    const uint8_t *ciphertext = packet + 12;
    const uint8_t *tag = packet + 12 + cipherLen;

#if defined(ESP32)
    if (keyLen == 16)
    {
        // AES-128-GCM
        mbedtls_gcm_context gcm;
        mbedtls_gcm_init(&gcm);

        int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 128);
        if (ret != 0)
        {
            mbedtls_gcm_free(&gcm);
            return false;
        }

        ret = mbedtls_gcm_auth_decrypt(&gcm, cipherLen, iv, 12, NULL, 0, tag, 16, ciphertext, plaintext);
        mbedtls_gcm_free(&gcm);

        if (ret != 0)
            return false;

        *plainLen = cipherLen;
        return true;
    }
    else if (keyLen == 32)
    {
        // AES-256-GCM
        mbedtls_gcm_context gcm;
        mbedtls_gcm_init(&gcm);

        int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
        if (ret != 0)
        {
            mbedtls_gcm_free(&gcm);
            return false;
        }

        ret = mbedtls_gcm_auth_decrypt(&gcm, cipherLen, iv, 12, NULL, 0, tag, 16, ciphertext, plaintext);
        mbedtls_gcm_free(&gcm);

        if (ret != 0)
            return false;

        *plainLen = cipherLen;
        return true;
    }

    return false;

#elif defined(ESP8266)
    if (keyLen != 32)
        return false;

    ChaChaPoly chacha;
    if (!chacha.setKey(key, 32))
        return false;
    if (!chacha.setIV(iv, 12))
        return false;

    chacha.decrypt(plaintext, ciphertext, cipherLen);

    uint8_t computedTag[16];
    chacha.computeTag(computedTag, 16);

    if (!chacha.checkTag(tag, 16))
    {
        memset(plaintext, 0, cipherLen);
        return false;
    }

    *plainLen = cipherLen;
    return true;
#else
    (void)iv;
    (void)ciphertext;
    (void)tag;
    return false;
#endif
}

void Firmngin::mqttCallback(char *path, byte *payload, unsigned int length)
{
    String payloadStr;

    // E2EE: attempt decryption if enabled and key is set
    if (_e2eeEnabled)
    {
        uint8_t plainBuf[512];
        size_t plainLen = 0;

        if (decryptE2EE(payload, length, _e2eeKeyBytes, _e2eeKeyLen, plainBuf, &plainLen))
        {
            payloadStr.reserve(plainLen);
            for (size_t i = 0; i < plainLen; i++)
            {
                payloadStr += (char)plainBuf[i];
            }
        }
        else
        {
            Serial.println("E2EE: decryption failed — verify DECRYPTOR key in keys.h matches the server key");
            return;
        }
    }
    else
    {
        payloadStr.reserve(length);
        for (unsigned int i = 0; i < length; i++)
        {
            payloadStr += (char)payload[i];
        }
    }

    String pathStr = String(path);
    String stateType = "";

    int lastSlash = pathStr.lastIndexOf('/');
    if (lastSlash >= 0)
    {
        stateType = pathStr.substring(lastSlash + 1);
    }

    DeviceState state(stateType, payloadStr);

    if (_callbacks.count(stateType) > 0)
    {
        _callbacks[stateType](state);
    }
    for (const auto &entry : _stateCallbacks)
    {
        if (entry.first == stateType && entry.second)
        {
            entry.second(state);
        }
    }

    // Typed callbacks: auto-parse, no manual JSON needed
    if ((stateType == "dpin" || stateType == "vr") && (_verificationCallback || !_verificationCallbacks.empty()))
    {
        Verifications v(payloadStr);
        if (v.isValid())
        {
            if (_verificationCallback)
            {
                _verificationCallback(v);
            }
            for (const auto &callback : _verificationCallbacks)
            {
                if (callback)
                {
                    callback(v);
                }
            }
        }
    }

    if ((stateType == "pp" || stateType == "pm") && (_paymentsCallback || !_paymentCallbacks.empty()))
    {
        Payments p(payloadStr);
        if (p.isValid())
        {
            p.setPending(stateType == "pp");
            p.setSuccess(stateType == "pm");
            if (stateType == "pm")
            {
                setCurrentOrder(p.orderId());
            }
            if (_paymentsCallback)
            {
                _paymentsCallback(p);
            }
            for (const auto &callback : _paymentCallbacks)
            {
                if (callback)
                {
                    callback(p);
                }
            }
        }
    }
    else if (stateType == "pm")
    {
        Payments p(payloadStr);
        if (p.isValid())
        {
            setCurrentOrder(p.orderId());
        }
    }

    if ((stateType == "ur" || stateType == "le" || stateType == "nl") && (_usagesCallback || !_usageCallbacks.empty()))
    {
        Usages u(payloadStr);
        if (u.isValid())
        {
            u.setNearLimit(stateType == "nl");
            u.setLimitExceeded(stateType == "le");
            if (_usagesCallback)
            {
                _usagesCallback(u);
            }
            for (const auto &callback : _usageCallbacks)
            {
                if (callback)
                {
                    callback(u);
                }
            }
        }
    }

    if (stateType == "ds" && (_deviceStateCallback || !_deviceStateCallbacks.empty()))
    {
        DeviceStates ds(payloadStr);
        if (ds.isValid())
        {
            setMerchantStatus(ds.state());
            if (_deviceStateCallback)
            {
                _deviceStateCallback(ds);
            }
            for (const auto &callback : _deviceStateCallbacks)
            {
                if (callback)
                {
                    callback(ds);
                }
            }
        }
    }
    else if (stateType == "ds")
    {
        DeviceStates ds(payloadStr);
        if (ds.isValid())
        {
            setMerchantStatus(ds.state());
        }
    }

    if (stateType == "init" && (_initCallback || !_initCallbacks.empty()))
    {
        Inits i(payloadStr);
        if (i.isValid())
        {
            setMerchantStatus(i.merchantStatus());
            if (i.isOnActiveService() && i.activeOrderId().length() > 0)
            {
                setCurrentOrder(i.activeOrderId());
            }
            if (_initCallback)
            {
                _initCallback(i);
            }
            for (const auto &callback : _initCallbacks)
            {
                if (callback)
                {
                    callback(i);
                }
            }
        }
    }
    else if (stateType == "init")
    {
        Inits i(payloadStr);
        if (i.isValid())
        {
            setMerchantStatus(i.merchantStatus());
            if (i.isOnActiveService() && i.activeOrderId().length() > 0)
            {
                setCurrentOrder(i.activeOrderId());
            }
        }
    }

    // Auto-reply: echo back ping payload as pong
    if (stateType == "pi")
    {
        String pongPath = "/d/" + String(_deviceId) + "/" + PATH_PONG;
        _mqttClient.publish(pongPath.c_str(), payloadStr.c_str());
    }

    if (pathStr.indexOf("/ot/trg") >= 0)
    {
        if (_otaAsyncState != OTA_ASYNC_IDLE)
        {
            _Debug("OTA trigger ignored, download already in progress");
            return;
        }

        _Debug("OTA signal received, performing OTA update...", true);
        _otaFirmwareID = "";
        publishOTAStatus("triggered", "Remote OTA triggered by dashboard");
        performOTA();
    }

    if (pathStr.indexOf("/rs/") >= 0)
    {
        int rsIndex = pathStr.indexOf("/rs/");
        String entityKey = pathStr.substring(rsIndex + 4); // after "/rs/"
        EntityCommand cmd(entityKey, payloadStr);
        _localEntityValues[entityKey] = payloadStr;

        for (const auto &callback : _entityGlobalCallbacks)
        {
            if (callback)
            {
                callback(cmd);
            }
        }

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

bool Firmngin::publishPayload(const char *path, const char *payload, bool retained)
{
    if (!_e2eeEnabled || _e2eeKeyLen == 0)
    {
        _Debug("E2EE not configured — payload publish blocked for security", true);
        return false;
    }

    size_t payloadLen = strlen(payload);
    if (payloadLen + 28 > FIRMNGIN_E2EE_BUFFER_SIZE)
        return false;

    uint8_t encrypted[FIRMNGIN_E2EE_BUFFER_SIZE];
    size_t encryptedLen = 0;
    if (!encryptE2EE((const uint8_t *)payload, payloadLen, _e2eeKeyBytes, _e2eeKeyLen, encrypted, &encryptedLen))
        return false;

    if (!_mqttClient.connected())
    {
        if (_queueEnabled)
            return _enqueueToQueue(path, encrypted, encryptedLen, retained);
        return false;
    }

    bool published = _mqttClient.publish(path, encrypted, encryptedLen, retained);
    if (!published && _queueEnabled)
    {
        return _enqueueToQueue(path, encrypted, encryptedLen, retained);
    }
    return published;
}

bool Firmngin::pushEntity(const char *key, const char *value)
{
    if (_topicUpdateEntity.length() == 0)
        return false;

    _localEntityValues[String(key)] = String(value);

    char buf[256];
    firmngin_json::Builder b(buf, sizeof(buf));
    b.reset();
    b.add("k", key);
    b.add("v", value);

    return publishPayload(_topicUpdateEntity.c_str(), b.build());
}

bool Firmngin::pushEntity(Entity &entity, const char *value)
{
    return pushEntity(entity.key().c_str(), value);
}

bool Firmngin::updateEntities(const char *jsonPayload)
{
    if (_topicUpdateEntities.length() == 0)
        return false;
    return publishPayload(_topicUpdateEntities.c_str(), jsonPayload);
}

EntityValue Firmngin::entity(const char *key)
{
    String entityKey(key);
    if (_localEntityValues.count(entityKey) == 0)
        return EntityValue();
    return EntityValue(_localEntityValues[entityKey]);
}

EntityValue Firmngin::entity(Entity &entity)
{
    return this->entity(entity.key().c_str());
}

bool Firmngin::endSession()
{
    if (_currentOrderId.length() == 0 || _merchantStatus != "on_active_service")
        return false;

    unsigned long now = millis();
    if (_sessionEndRequested || (now - _lastSessionEndAtMs < 3000))
        return false;
    _lastSessionEndAtMs = now;

    char mid[24];
    snprintf(mid, sizeof(mid), "%lu", now);

    char buf[160];
    firmngin_json::Builder b(buf, sizeof(buf));
    b.reset();
    b.add("oid", _currentOrderId.c_str());
    b.add("src", "dev");
    b.add("mid", mid);

    bool sent = publishPayload(getPathSessionEnd(_deviceId).c_str(), b.build());
    if (sent)
    {
        _sessionEndRequested = true;
    }
    return sent;
}

bool Firmngin::requestInit()
{
    return publishPayload(getPathRequestInit(_deviceId).c_str(), "{}");
}

void Firmngin::runActiveSessionHandlers()
{
    if (_activeSessionCallbacks.empty())
        return;
    if (_currentOrderId.length() == 0 || _merchantStatus != "on_active_service")
        return;

    ActiveSession session(this, _currentOrderId, true, true);
    for (auto handler : _activeSessionCallbacks)
    {
        if (handler != nullptr)
        {
            handler(session);
        }
    }
}

void Firmngin::setMerchantStatus(const String &status)
{
    _merchantStatus = status;
    if (status != "on_active_service")
    {
        clearCurrentSession();
    }
}

void Firmngin::setCurrentOrder(const String &orderId)
{
    if (orderId.length() == 0)
        return;
    if (_currentOrderId != orderId)
    {
        _currentOrderId = orderId;
        _sessionEndRequested = false;
        _lastSessionEndAtMs = 0;
    }
    _merchantStatus = "on_active_service";
}

void Firmngin::clearCurrentSession()
{
    _currentOrderId = "";
    _sessionEndRequested = false;
    _lastSessionEndAtMs = 0;
}

void Firmngin::setFirmwareInfo(const char *version, const char *targetBoard, const char *targetModel)
{
    if (version != nullptr)
        _firmwareVersion = version;
    if (targetBoard != nullptr)
        _firmwareTargetBoard = targetBoard;
    if (targetModel != nullptr)
        _firmwareTargetModel = targetModel;

    // Auto-sync to server if already connected so the versioning_firmware
    // entity is updated immediately without waiting for a reconnect.
    if (_mqttClient.connected())
    {
        syncFirmwareInfo();
    }
}

void Firmngin::setFirmwareInfo(const char *version)
{
    if (version != nullptr)
        _firmwareVersion = version;

    // Auto-sync to server if already connected.
    if (_mqttClient.connected())
    {
        syncFirmwareInfo();
    }
}

const char *Firmngin::getFirmwareVersion()
{
    return _firmwareVersion.c_str();
}

const char *Firmngin::getFirmwareTargetBoard()
{
    return _firmwareTargetBoard.c_str();
}

const char *Firmngin::getFirmwareTargetModel()
{
    return _firmwareTargetModel.c_str();
}

bool Firmngin::syncFirmwareInfo()
{
    if (!_mqttClient.connected())
    {
        if (_debug)
        {
            Serial.println(F("[firmngin] syncFirmwareInfo: skipped, not connected"));
        }
        return false;
    }

    if (_firmwareVersion == _lastSyncedFirmwareVersion)
    {
        if (_debug)
        {
            Serial.println(F("[firmngin] syncFirmwareInfo: skipped, same version"));
        }
        return true;
    }

    String value = "{\"v\":\"" + _firmwareVersion + "\",\"b\":\"" + _firmwareTargetBoard + "\",\"m\":\"" + _firmwareTargetModel + "\"}";

    bool ok = pushEntity("versioning_firmware", value.c_str());

    if (ok)
    {
        _lastSyncedFirmwareVersion = _firmwareVersion;
    }

    if (_debug && ok)
    {
        Serial.print(F("Syncing Firmware Info: V"));
        Serial.print(_firmwareVersion);
        Serial.print(F(", Target Board: "));
        Serial.print(_firmwareTargetBoard);
        Serial.print(F(", Model: "));
        Serial.println(_firmwareTargetModel);
    }

    return ok;
}

#if defined(ESP32)

static String hmacSHA256(const char *key, const char *message)
{
    uint8_t hmacResult[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&ctx, (const unsigned char *)key, strlen(key));
    mbedtls_md_hmac_update(&ctx, (const unsigned char *)message, strlen(message));
    mbedtls_md_hmac_finish(&ctx, hmacResult);
    mbedtls_md_free(&ctx);

    char hex[65];
    for (int i = 0; i < 32; i++)
        sprintf(hex + (i * 2), "%02x", hmacResult[i]);
    hex[64] = '\0';
    return String(hex);
}

#elif defined(ESP8266)

static String hmacSHA256(const char *key, const char *message)
{
    uint8_t keyBlock[64];
    memset(keyBlock, 0, sizeof(keyBlock));
    size_t keyLen = strlen(key);
    if (keyLen > 64)
    {
        br_sha256_context sha;
        br_sha256_init(&sha);
        br_sha256_update(&sha, key, keyLen);
        br_sha256_out(&sha, keyBlock);
        keyLen = 32;
    }
    else
    {
        memcpy(keyBlock, key, keyLen);
    }

    uint8_t oKeyPad[64];
    uint8_t iKeyPad[64];
    for (int i = 0; i < 64; i++)
    {
        oKeyPad[i] = keyBlock[i] ^ 0x5c;
        iKeyPad[i] = keyBlock[i] ^ 0x36;
    }

    br_sha256_context sha;
    uint8_t innerHash[32];
    br_sha256_init(&sha);
    br_sha256_update(&sha, iKeyPad, 64);
    br_sha256_update(&sha, message, strlen(message));
    br_sha256_out(&sha, innerHash);

    uint8_t hmacResult[32];
    br_sha256_init(&sha);
    br_sha256_update(&sha, oKeyPad, 64);
    br_sha256_update(&sha, innerHash, 32);
    br_sha256_out(&sha, hmacResult);

    char hex[65];
    for (int i = 0; i < 32; i++)
        sprintf(hex + (i * 2), "%02x", hmacResult[i]);
    hex[64] = '\0';
    return String(hex);
}

#endif

#if 0
// OTA implementation moved to ota.cpp.
void Firmngin::setOTABaseURL(const char *baseURL)
{
    _otaBaseUrl = baseURL != nullptr ? baseURL : "";
}

void Firmngin::setEnableOTA(bool enabled)
{
    _otaEnabled = enabled;
}

void Firmngin::enableOTA(bool enabled)
{
    setEnableOTA(enabled);
}

bool Firmngin::otaHTTPGet(const char *path, const char *queryParams, String &responseBody)
{
    String fullPath = String(path);
    if (queryParams != nullptr && strlen(queryParams) > 0)
    {
        fullPath += "?";
        fullPath += queryParams;
    }

    String url = _otaBaseUrl + fullPath;

    unsigned long ts = time(nullptr);
    String message = String(_deviceId) + "." + String(ts) + ".GET." + fullPath;

#if defined(ESP8266) || defined(ESP32)
    HTTPClient http;
#if defined(ESP32)
    WiFiClientSecure otaClient;
    if (_insecure)
    {
        otaClient.setInsecure();
    }
    else
    {
#if defined(HAS_SERVICE_CA_CERT)
        otaClient.setCACert(SERVICE_CA_CERT);
#else
        publishOTAStatus("failed", "Service CA certificate is not configured");
        return false;
#endif
    }
    if (_clientCert != nullptr)
        otaClient.setCertificate(_clientCert);
    if (_privateKey != nullptr)
        otaClient.setPrivateKey(_privateKey);
#elif defined(ESP8266)
    BearSSL::WiFiClientSecure otaClient;
    otaClient.setBufferSizes(512, 512);
    if (_clientCertList != nullptr && _clientPrivKey != nullptr)
        otaClient.setClientRSACert(_clientCertList, _clientPrivKey);
#if defined(HAS_SERVICE_CA_CERT)
    BearSSL::X509List serviceCACert(SERVICE_CA_CERT);
    otaClient.setTrustAnchors(&serviceCACert);
#else
    publishOTAStatus("failed", "Service CA certificate is not configured");
    return false;
#endif
#endif
    if (!http.begin(otaClient, url))
    {
        publishOTAStatus("failed", "Manifest HTTP client initialization failed");
        return false;
    }
    http.addHeader("X-Device-ID", String(_deviceId));
    http.addHeader("X-Device-Timestamp", String(ts));
    http.addHeader("X-Device-Signature", hmacSHA256(_deviceKey, message.c_str()));
    http.addHeader("Accept", "application/json");

    int httpCode = http.GET();
    bool ok = (httpCode == HTTP_CODE_OK);
    if (ok)
        responseBody = http.getString();
    else
    {
        _Debug("OTA HTTP " + String(httpCode) + ": " + url);
        _Debug("OTA HTTP error: " + http.errorToString(httpCode));
    }

    http.end();
    return ok;
#else
    (void)responseBody;
    return false;
#endif
}

bool Firmngin::checkOTA()
{
    if (!_otaEnabled)
        return publishOTAStatus("disabled", "OTA is not enabled");

    if (_otaBaseUrl.length() == 0)
        return publishOTAStatus("skipped", "OTA base URL is not configured");

    publishOTAStatus("checking", "Requesting manifest");

    char query[256];
    int pos = 0;
    if (_firmwareTargetBoard.length() > 0)
    {
        pos += snprintf(query + pos, sizeof(query) - pos,
                        "target_board=%s&", _firmwareTargetBoard.c_str());
    }
    if (_firmwareTargetModel.length() > 0)
    {
        pos += snprintf(query + pos, sizeof(query) - pos,
                        "target_model=%s&", _firmwareTargetModel.c_str());
    }
    pos += snprintf(query + pos, sizeof(query) - pos,
                    "current_version=%s", _firmwareVersion.c_str());

    String body;
    if (!otaHTTPGet("/manifest", query, body))
    {
        publishOTAStatus("failed", "Manifest request failed");
        return false;
    }

    int idStart = body.indexOf("\"firmware_id\":\"");
    int verStart = body.indexOf("\"version\":\"");
    int shaStart = body.indexOf("\"sha256\":\"");

    if (idStart < 0)
    {
        publishOTAStatus("uptodate", "No update available");
        return true;
    }

    idStart += 15;
    _otaFirmwareID = body.substring(idStart, body.indexOf("\"", idStart));
    verStart += 11;
    String newVer = body.substring(verStart, body.indexOf("\"", verStart));
    shaStart += 10;
    _otaFirmwareSHA256 = body.substring(shaStart, body.indexOf("\"", shaStart));

    char msg[128];
    snprintf(msg, sizeof(msg), "Update available: %s", newVer.c_str());
    publishOTAStatus("available", msg);
    return true;
}

bool Firmngin::performOTA(const char *manifestUrl)
{
    (void)manifestUrl;

    if (_otaAsyncState != OTA_ASYNC_IDLE)
        return publishOTAStatus("skipped", "OTA already in progress");

    if (!_otaEnabled)
    {
        _otaAsyncState = OTA_ASYNC_IDLE;
        _otaFirmwareID = "";
        return publishOTAStatus("disabled", "OTA is not enabled");
    }

    if (_otaFirmwareID.length() == 0)
    {
        if (!checkOTA())
        {
            _otaAsyncState = OTA_ASYNC_IDLE;
            _otaFirmwareID = "";
            return false;
        }
    }

    if (_otaFirmwareID.length() == 0)
    {
        _otaAsyncState = OTA_ASYNC_IDLE;
        _otaFirmwareID = "";
        return publishOTAStatus("skipped", "No firmware available");
    }

    publishOTAStatus("downloading", "Starting firmware download");

    String downloadPath = String("/firmware/") + _otaFirmwareID + "/download";

#if defined(ESP8266) || defined(ESP32)
    String url = _otaBaseUrl + downloadPath;
#if defined(ESP32)
    if (_insecure)
    {
        _otaWifiClient.setInsecure();
    }
    else
    {
#if defined(HAS_SERVICE_CA_CERT)
        _otaWifiClient.setCACert(SERVICE_CA_CERT);
#else
        _otaAsyncState = OTA_ASYNC_IDLE;
        publishOTAStatus("failed", "Service CA certificate is not configured");
        _otaFirmwareID = "";
        return false;
#endif
    }
    if (_clientCert != nullptr)
        _otaWifiClient.setCertificate(_clientCert);
    if (_privateKey != nullptr)
        _otaWifiClient.setPrivateKey(_privateKey);
#elif defined(ESP8266)
    _otaWifiClient.setBufferSizes(512, 512);
    if (_clientCertList != nullptr && _clientPrivKey != nullptr)
        _otaWifiClient.setClientRSACert(_clientCertList, _clientPrivKey);
#if defined(HAS_SERVICE_CA_CERT)
    if (_otaTrustAnchors != nullptr)
    {
        delete _otaTrustAnchors;
    }
    _otaTrustAnchors = new BearSSL::X509List(SERVICE_CA_CERT);
    _otaWifiClient.setTrustAnchors(_otaTrustAnchors);
#else
    _otaAsyncState = OTA_ASYNC_IDLE;
    publishOTAStatus("failed", "Service CA certificate is not configured");
    _otaFirmwareID = "";
    return false;
#endif
#endif
    if (!_otaHttp.begin(_otaWifiClient, url))
    {
        _otaAsyncState = OTA_ASYNC_IDLE;
        publishOTAStatus("failed", "Firmware HTTP client initialization failed");
        _otaFirmwareID = "";
        return false;
    }

    unsigned long ts = time(nullptr);
    String message = String(_deviceId) + "." + String(ts) + ".GET." + downloadPath;
    _otaHttp.addHeader("X-Device-ID", String(_deviceId));
    _otaHttp.addHeader("X-Device-Timestamp", String(ts));
    _otaHttp.addHeader("X-Device-Signature", hmacSHA256(_deviceKey, message.c_str()));

    int httpCode = _otaHttp.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        _Debug("OTA download HTTP " + String(httpCode));
        _Debug("OTA download error: " + _otaHttp.errorToString(httpCode));
        _otaHttp.end();
        _otaAsyncState = OTA_ASYNC_IDLE;
        publishOTAStatus("failed", "Firmware download failed");
        _otaFirmwareID = "";
        return false;
    }

    _otaContentLength = _otaHttp.getSize();
    if (_debug)
    {
        Serial.print("OTA download started (async). Size: ");
        Serial.print(_otaContentLength >= 0 ? String(_otaContentLength / 1024) : String("unknown"));
        if (_otaContentLength >= 0)
            Serial.println(" KB");
        else
            Serial.println();
    }

    _otaBuffer = (uint8_t *)malloc(FIRMWARE_BUFFER_SIZE);
    if (_otaBuffer == nullptr)
    {
        _otaHttp.end();
        _otaAsyncState = OTA_ASYNC_IDLE;
        publishOTAStatus("failed", "Firmware buffer allocation failed");
        _otaFirmwareID = "";
        return false;
    }
    _otaDownloaded = 0;
    _otaLastProgressBucket = -1;
    _otaLastPublishedProgressBucket = -1;
    _otaLastDebugAt = 0;

#if defined(ESP32)
    mbedtls_md_init(&_otaSha256Ctx);
    mbedtls_md_setup(&_otaSha256Ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
    mbedtls_md_starts(&_otaSha256Ctx);
#elif defined(ESP8266)
    br_sha256_init(&_otaSha256Ctx);
#endif

    if (_otaContentLength > 0 && !Update.begin(_otaContentLength))
    {
        if (_debug)
            Serial.println("OTA Update.begin() failed: " + String(Update.getError()));
        _otaCleanup();
        publishOTAStatus("failed", "Update.begin() failed");
        _otaFirmwareID = "";
        return false;
    }

    _otaAsyncState = OTA_ASYNC_DOWNLOADING;
    return true;
#else
    _otaAsyncState = OTA_ASYNC_IDLE;
    _otaFirmwareID = "";
    return publishOTAStatus("skipped", "Platform not supported for HTTP OTA");
#endif
}

void Firmngin::_processOTA()
{
    if (_otaAsyncState == OTA_ASYNC_IDLE || _otaAsyncState == OTA_ASYNC_FAILED)
        return;

    if (_otaAsyncState == OTA_ASYNC_DOWNLOADING)
    {
        WiFiClient *stream = _otaHttp.getStreamPtr();
        if (stream == nullptr)
        {
            _otaCleanup();
            publishOTAStatus("failed", "HTTP stream lost");
            _otaFirmwareID = "";
            return;
        }

        size_t available = stream->available();
        if (available == 0)
        {
            if (!_otaHttp.connected())
                _otaAsyncState = OTA_ASYNC_VERIFYING;
            return;
        }

        int toRead = min((int)available, FIRMWARE_BUFFER_SIZE);
        int readLen = stream->readBytes(_otaBuffer, toRead);
        if (readLen <= 0)
        {
            if (!_otaHttp.connected())
                _otaAsyncState = OTA_ASYNC_VERIFYING;
            return;
        }

        _otaDownloaded += readLen;

        yield();

#if defined(ESP32)
        mbedtls_md_update(&_otaSha256Ctx, _otaBuffer, readLen);
#elif defined(ESP8266)
        br_sha256_update(&_otaSha256Ctx, _otaBuffer, readLen);
#endif

        if (_otaContentLength > 0)
        {
            size_t written = Update.write(_otaBuffer, readLen);
            if (written != (size_t)readLen)
            {
                if (_debug)
                    Serial.println("OTA Update.write() failed: " + String(Update.getError()));
                Update.abort();
                _otaCleanup();
                publishOTAStatus("failed", "Firmware write failed");
                _otaFirmwareID = "";
                return;
            }

            int progress = (_otaDownloaded * 100) / _otaContentLength;
            int progressBucket = progress / 10;

            if (progressBucket != _otaLastPublishedProgressBucket || progress == 100)
            {
                _otaLastPublishedProgressBucket = progressBucket;
                char progressValue[8];
                snprintf(progressValue, sizeof(progressValue), "%d", progress);
                char progressPayload[160];
                firmngin_json::ArrayBuilder b(progressPayload, sizeof(progressPayload));
                b.reset();
                b.add("ots", "downloading");
                b.add("otp", progressValue);
                if (_otaFirmwareID.length() > 0)
                    b.add("ofd", _otaFirmwareID.c_str());
                updateEntities(b.build());
            }

            if (_debug)
            {
                if (progressBucket != _otaLastProgressBucket || progress == 100)
                {
                    _otaLastProgressBucket = progressBucket;
                    Serial.print("OTA download progress: ");
                    Serial.print(progress);
                    Serial.print("% (");
                    Serial.print(_otaDownloaded / 1024);
                    Serial.print("/");
                    Serial.print(_otaContentLength / 1024);
                    Serial.println(" KB)");
                }
            }
        }
        else if (_debug && millis() - _otaLastDebugAt > 1000)
        {
            _otaLastDebugAt = millis();
            Serial.print("OTA download received: ");
            Serial.print(_otaDownloaded / 1024);
            Serial.println(" KB");
        }
        else if (millis() - _otaLastDebugAt > 1000)
        {
            _otaLastDebugAt = millis();
            int estimated = _otaDownloaded / 32;
            if (estimated < 0)
                estimated = 0;
            if (estimated > 99)
                estimated = 99;
            char progressValue[8];
            snprintf(progressValue, sizeof(progressValue), "%d", estimated);
            char progressPayload[160];
            firmngin_json::ArrayBuilder b(progressPayload, sizeof(progressPayload));
            b.reset();
            b.add("ots", "downloading");
            b.add("otp", progressValue);
            if (_otaFirmwareID.length() > 0)
                b.add("ofd", _otaFirmwareID.c_str());
            updateEntities(b.build());

            if (_debug)
            {
                Serial.print("OTA download received: ");
                Serial.print(_otaDownloaded / 1024);
                Serial.println(" KB");
            }
        }

        if (_otaContentLength > 0 && _otaDownloaded >= _otaContentLength)
        {
            _otaAsyncState = OTA_ASYNC_VERIFYING;
        }
        else
        {
            if (_otaHttp.connected() || _otaContentLength < 0 || _otaDownloaded < _otaContentLength)
                return;

            _otaAsyncState = OTA_ASYNC_VERIFYING;
        }
    }

    if (_otaAsyncState == OTA_ASYNC_VERIFYING)
    {
        _otaHttp.end();

        if (_otaContentLength > 0 && _otaDownloaded != _otaContentLength)
        {
            Update.abort();
            _otaCleanup();
            publishOTAStatus("failed", "Firmware download incomplete");
            _otaFirmwareID = "";
            return;
        }

#if defined(ESP32)
        uint8_t computedHash[32];
        mbedtls_md_finish(&_otaSha256Ctx, computedHash);
        mbedtls_md_free(&_otaSha256Ctx);
#elif defined(ESP8266)
        uint8_t computedHash[32];
        br_sha256_out(&_otaSha256Ctx, computedHash);
#endif

#if defined(ESP32) || defined(ESP8266)
        char computedHex[65];
        for (int i = 0; i < 32; i++)
            sprintf(computedHex + (i * 2), "%02x", computedHash[i]);
        computedHex[64] = '\0';

        if (_otaFirmwareSHA256.length() > 0 && _otaFirmwareSHA256 != computedHex)
        {
            if (_debug)
            {
                Serial.print("OTA SHA256 mismatch: expected ");
                Serial.print(_otaFirmwareSHA256);
                Serial.print(", got ");
                Serial.println(computedHex);
            }
            Update.abort();
            _otaCleanup();
            publishOTAStatus("failed", "SHA256 mismatch");
            _otaFirmwareID = "";
            return;
        }

        if (_debug)
        {
            Serial.print("OTA SHA256 verified OK: ");
            Serial.println(computedHex);
        }
#endif
        publishOTAStatus("installing", "Installing firmware");
        _otaAsyncState = OTA_ASYNC_INSTALLING;
    }

    if (_otaAsyncState == OTA_ASYNC_INSTALLING)
    {
        if (_debug)
        {
            Serial.print("OTA download completed: ");
            Serial.print(_otaDownloaded / 1024);
            Serial.println(" KB");
        }

        if (!Update.end())
        {
            if (_debug)
                Serial.println("OTA Update.end() failed: " + String(Update.getError()));
            _otaCleanup();
            publishOTAStatus("failed", "Update.end() failed");
            _otaFirmwareID = "";
            return;
        }

        publishOTAStatus("installing", "Finalizing firmware install");
        publishOTAStatus("installed", "Firmware installed");
        publishOTAStatus("rebooting", "Rebooting device");
        _otaFirmwareID = "";

        free(_otaBuffer);
        _otaBuffer = nullptr;
        _otaAsyncState = OTA_ASYNC_IDLE;

        if (_debug)
            Serial.println("OTA installed successfully, rebooting...");
        delay(500);
        ESP.restart();
    }
}

void Firmngin::_otaCleanup()
{
    _otaAsyncState = OTA_ASYNC_FAILED;
    _otaHttp.end();
    if (_otaBuffer != nullptr)
    {
        free(_otaBuffer);
        _otaBuffer = nullptr;
    }
#if defined(ESP32)
    mbedtls_md_free(&_otaSha256Ctx);
#endif
    _otaDownloaded = 0;
    _otaContentLength = 0;
}

void Firmngin::onOTAStatus(OTACallbackFunction callback)
{
    _otaCallback = callback;
}

int Firmngin::normalizeOTAProgress(const char *status, const char *message)
{
    int progress = 0;
    String statusStr = status != nullptr ? String(status) : String("");
    statusStr.toLowerCase();

    if (statusStr == "checking")
        progress = 5;
    else if (statusStr == "available")
        progress = 10;
    else if (statusStr == "downloading")
        progress = 25;
    else if (statusStr == "verifying")
        progress = 95;
    else if (statusStr == "installing")
        progress = 97;
    else if (statusStr == "rebooting" || statusStr == "booting")
        progress = 99;
    else if (statusStr == "installed" || statusStr == "completed" || statusStr == "uptodate" || statusStr == "skipped")
        progress = 100;
    else if (statusStr == "failed" || statusStr == "triggered")
        progress = 0;

    if (message != nullptr)
    {
        String msg = String(message);
        int start = -1;
        for (unsigned int i = 0; i < msg.length(); i++)
        {
            if (isDigit(msg.charAt(i)))
            {
                start = (int)i;
                break;
            }
        }

        if (start >= 0)
        {
            int end = start;
            while ((unsigned int)end < msg.length() && isDigit(msg.charAt(end)))
                end++;
            int parsed = msg.substring(start, end).toInt();
            if (parsed >= 0 && parsed <= 100)
                progress = parsed;
            else if (parsed > 100)
                progress = 100;
        }
    }

    if (progress < 0)
        progress = 0;
    if (progress > 100)
        progress = 100;
    return progress;
}

bool Firmngin::publishOTAStatus(const char *status, const char *message)
{
    if (_debug)
    {
        Serial.print("OTA status: ");
        Serial.print(status);
        Serial.print(" - ");
        Serial.println(message);
    }

    if (_otaCallback)
        _otaCallback(status, message);
    for (const auto &callback : _otaCallbacks)
    {
        if (callback)
        {
            callback(status, message);
        }
    }

    char buf[512];
    firmngin_json::ArrayBuilder b(buf, sizeof(buf));
    b.reset();
    b.add("ots", status);
    char progressValue[8];
    snprintf(progressValue, sizeof(progressValue), "%d", normalizeOTAProgress(status, message));
    b.add("otp", progressValue);
    if (_otaFirmwareID.length() > 0)
        b.add("ofd", _otaFirmwareID.c_str());

    return updateEntities(b.build());
}
#endif

// ==================== CAMERA IMAGE UPLOAD ====================

bool Firmngin::uploadImage(const char *entityKey, uint8_t *data, size_t len, const char *contentType,
                           UploadCallbackFunction onSuccess,
                           UploadErrorCallbackFunction onError)
{
    if (entityKey == nullptr || strlen(entityKey) == 0)
    {
        if (onError)
            onError(0, "entity_key is required");
        return false;
    }

    if (data == nullptr || len == 0)
    {
        if (onError)
            onError(0, "empty image data");
        return false;
    }

    if (_debug)
    {
        Serial.print("[CAMERA] Uploading ");
        Serial.print(len);
        Serial.print(" bytes as ");
        Serial.println(contentType);
    }

#if defined(ESP8266) || defined(ESP32)
    // Build URL: /api/v1/device-camera/upload
    String url = String("https://") + String(FIRMNGIN_SERVER_ADDR) + ":" + String(FIRMNGIN_SERVER_PORT) + "/api/v1/device-camera/upload";

    // Build HMAC signature: device_id.timestamp.POST./api/v1/device-camera/upload
    unsigned long ts = time(nullptr);
    String path = "/api/v1/device-camera/upload";
    String message = String(_deviceId) + "." + String(ts) + ".POST." + path;
    String signature = hmacSHA256(_deviceKey, message.c_str());

    HTTPClient http;
#if defined(ESP32)
    WiFiClientSecure cameraClient;
    if (_insecure)
    {
        cameraClient.setInsecure();
    }
    else
    {
#if defined(HAS_SERVICE_CA_CERT)
        cameraClient.setCACert(SERVICE_CA_CERT);
#else
        if (onError)
            onError(0, "service CA certificate not configured");
        return false;
#endif
    }
    if (_clientCert != nullptr)
        cameraClient.setCertificate(_clientCert);
    if (_privateKey != nullptr)
        cameraClient.setPrivateKey(_privateKey);
#elif defined(ESP8266)
    BearSSL::WiFiClientSecure cameraClient;
    cameraClient.setBufferSizes(512, 512);
    if (_clientCertList != nullptr && _clientPrivKey != nullptr)
        cameraClient.setClientRSACert(_clientCertList, _clientPrivKey);
#if defined(HAS_SERVICE_CA_CERT)
    BearSSL::X509List serviceCACert(SERVICE_CA_CERT);
    cameraClient.setTrustAnchors(&serviceCACert);
#else
    if (onError)
        onError(0, "service CA certificate not configured");
    return false;
#endif
#endif

    if (!http.begin(cameraClient, url))
    {
        if (onError)
            onError(0, "HTTP client init failed");
        return false;
    }

    // Add HMAC headers
    http.addHeader("X-Device-ID", String(_deviceId));
    http.addHeader("X-Device-Timestamp", String(ts));
    http.addHeader("X-Device-Signature", signature);

    // Build multipart form data
    String boundary = "----FirmnginBoundary" + String(millis());
    String contentTypeHeader = "multipart/form-data; boundary=" + boundary;
    http.addHeader("Content-Type", contentTypeHeader);

    // Build body
    String body = "";
    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"entity_key\"\r\n\r\n";
    body += String(entityKey) + "\r\n";
    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"image\"; filename=\"capture.jpg\"\r\n";
    body += "Content-Type: " + String(contentType) + "\r\n\r\n";

    // Calculate total content length
    size_t bodyHeaderLen = body.length();
    size_t bodyFooterLen = ("\r\n--" + boundary + "--\r\n").length();
    size_t totalLen = bodyHeaderLen + len + bodyFooterLen;

    // Use chunked transfer or content length
    http.addHeader("Content-Length", String(totalLen));

    if (_debug)
    {
        Serial.print("[CAMERA] URL: ");
        Serial.println(url);
        Serial.print("[CAMERA] Content-Length: ");
        Serial.println(totalLen);
    }

    // Send request - we need to write body manually for binary data
    int httpCode = http.sendRequest("POST", (uint8_t *)nullptr, 0);

    if (httpCode > 0)
    {
        // Write the multipart body
        WiFiClient *stream = http.getStreamPtr();

        // Write header part
        stream->write((const uint8_t *)body.c_str(), bodyHeaderLen);

        // Write image data
        stream->write(data, len);

        // Write footer
        String footer = "\r\n--" + boundary + "--\r\n";
        stream->write((const uint8_t *)footer.c_str(), footer.length());

        // Wait for response
        httpCode = http.GET();
    }

    if (httpCode > 0)
    {
        if (_debug)
        {
            Serial.print("[CAMERA] HTTP ");
            Serial.println(httpCode);
        }

        if (httpCode == HTTP_CODE_OK)
        {
            String response = http.getString();
            if (onSuccess)
                onSuccess(response.c_str());
            http.end();
            return true;
        }
        else
        {
            if (onError)
                onError(httpCode, http.errorToString(httpCode).c_str());
        }
    }
    else
    {
        if (onError)
            onError(httpCode, http.errorToString(httpCode).c_str());
    }

    http.end();
    return false;

#else
    if (onError)
        onError(0, "platform not supported");
    return false;
#endif
}

BatchState Firmngin::pushBatchEntities()
{
    return BatchState(this);
}

bool BatchState::send()
{
    if (_instance == nullptr)
        return false;
    return _instance->updateEntities(_builder.build());
}

#if 0
// Persistent queue implementation moved to queue.cpp.
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

    if (!LittleFS.begin(true))
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

    uint32_t magic = (uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8) | ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24);
    if (magic != FIRMNGIN_QUEUE_MAGIC)
        return false;

    uint16_t version = (uint16_t)hdr[4] | ((uint16_t)hdr[5] << 8);
    if (version != FIRMNGIN_QUEUE_VERSION)
        return false;

    head = (uint16_t)hdr[8] | ((uint16_t)hdr[9] << 8);
    tail = (uint16_t)hdr[10] | ((uint16_t)hdr[11] << 8);
    count = (uint16_t)hdr[12] | ((uint16_t)hdr[13] << 8);

    if (head >= _queueCapacity || tail >= _queueCapacity || count > _queueCapacity)
        return false;
    return true;
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
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = FIRMNGIN_QUEUE_MAGIC & 0xFF;
    hdr[1] = (FIRMNGIN_QUEUE_MAGIC >> 8) & 0xFF;
    hdr[2] = (FIRMNGIN_QUEUE_MAGIC >> 16) & 0xFF;
    hdr[3] = (FIRMNGIN_QUEUE_MAGIC >> 24) & 0xFF;
    hdr[4] = FIRMNGIN_QUEUE_VERSION & 0xFF;
    hdr[5] = (FIRMNGIN_QUEUE_VERSION >> 8) & 0xFF;
    hdr[6] = 0;
    hdr[7] = 0;
    hdr[8] = head & 0xFF;
    hdr[9] = (head >> 8) & 0xFF;
    hdr[10] = tail & 0xFF;
    hdr[11] = (tail >> 8) & 0xFF;
    hdr[12] = count & 0xFF;
    hdr[13] = (count >> 8) & 0xFF;
    hdr[14] = 0;
    hdr[15] = 0;

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

    size_t topicLen = strnlen(topic, FIRMNGIN_QUEUE_TOPIC_MAX + 1);
    if (topicLen == 0 || topicLen > FIRMNGIN_QUEUE_TOPIC_MAX || payloadLen > FIRMNGIN_QUEUE_PAYLOAD_MAX)
    {
        if (_debug)
        {
            Serial.print(F("[firmngin] queue: payload too large (topic="));
            Serial.print(topicLen);
            Serial.print(F(", payload="));
            Serial.print(payloadLen);
            Serial.println(F(")"));
        }
        return false;
    }

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
    memset(record, 0, sizeof(record));
    record[0] = topicLen & 0xFF;
    record[1] = (topicLen >> 8) & 0xFF;
    record[2] = payloadLen & 0xFF;
    record[3] = (payloadLen >> 8) & 0xFF;
    record[4] = retained ? 1 : 0;
    record[5] = 0;
    record[6] = 0;
    record[7] = 0;
    memcpy(record + 8, topic, topicLen);
    memcpy(record + 8 + FIRMNGIN_QUEUE_TOPIC_MAX, payload, payloadLen);

    size_t recordOffset = FIRMNGIN_QUEUE_HEADER_SIZE + (size_t)_queueTail * FIRMNGIN_QUEUE_RECORD_SIZE;
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

    size_t recordOffset = FIRMNGIN_QUEUE_HEADER_SIZE + (size_t)_queueHead * FIRMNGIN_QUEUE_RECORD_SIZE;
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

    uint16_t topicLen = (uint16_t)record[0] | ((uint16_t)record[1] << 8);
    uint16_t payloadLen = (uint16_t)record[2] | ((uint16_t)record[3] << 8);
    retained = record[4] != 0;

    if (topicLen == 0 || topicLen > FIRMNGIN_QUEUE_TOPIC_MAX || payloadLen > FIRMNGIN_QUEUE_PAYLOAD_MAX)
    {
        return false;
    }

    char topicBuf[FIRMNGIN_QUEUE_TOPIC_MAX + 1];
    char payloadBuf[FIRMNGIN_QUEUE_PAYLOAD_MAX + 1];
    memcpy(topicBuf, record + 8, topicLen);
    topicBuf[topicLen] = '\0';
    memcpy(payloadBuf, record + 8 + FIRMNGIN_QUEUE_TOPIC_MAX, payloadLen);
    payloadBuf[payloadLen] = '\0';

    topic = String(topicBuf);
    payload = String(payloadBuf, payloadLen);
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
#endif
