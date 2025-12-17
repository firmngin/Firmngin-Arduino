#include "firmnginKit.h"

FirmnginKit* _globalFirmnginKitInstance = nullptr;

const char *NTP_SERVER = "pool.ntp.org";
int GMT_OFFSET_SEC = 7 * 3600;
int DAYLIGHT_OFFSET_SEC = 0;

const char* MQTT_SERVER_ADDR = DEFAULT_MQTT_SERVER;
const int MQTT_SERVER_PORT = DEFAULT_MQTT_PORT;

String MQ_USERNAME = "";
String MQ_PASSWORD = "";

static void printBanner() {
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
    Serial.println(F("Powered by firmngin.dev IoT Platform    "));
    Serial.println();
}

#if defined(ESP8266)
FirmnginKit::FirmnginKit(const char *deviceId, const char *deviceKey, const char* clientCert, const char* privateKey, const uint8_t* fingerprint)
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
    _globalFirmnginKitInstance = this;
}
#elif defined(ESP32)
FirmnginKit::FirmnginKit(const char *deviceId, const char *deviceKey, const char* caCert, const char* clientCert, const char* privateKey)
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
    _globalFirmnginKitInstance = this;
}

FirmnginKit::FirmnginKit(const char *deviceId, const char *deviceKey, const uint8_t* fingerprint, const char* clientCert, const char* privateKey)
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
    _globalFirmnginKitInstance = this;
}
#else
FirmnginKit::FirmnginKit(const char *deviceId, const char *deviceKey)
    : _deviceId(deviceId),
      _deviceKey(deviceKey),
      _debug(false),
      _lastMQTTAttempt(0),
      _mqttClient(_wifiClient),
      _mqttServer(MQTT_SERVER_ADDR),
      _mqttPort(MQTT_SERVER_PORT)
{
    Serial.println("ERROR: Platform not supported");
}
#endif

FirmnginKit::~FirmnginKit() {
#if defined(ESP8266)
    delete _clientCertList;
    delete _clientPrivKey;
#endif
}

// Example: getPaymentSuccess("dev-1764691334-daa58e77") returns "/c/dev-1764691334-daa58e77/pm"
String FirmnginKit::getPaymentSuccess(String deviceId) {
    return String("/c/") + deviceId + "/" + T_PAYMENT_SUCCESS;
}

// Example: getDeviceStatus("dev-1764691334-daa58e77") returns "/c/dev-1764691334-daa58e77/ds"
String FirmnginKit::getDeviceStatus(String deviceId) {
    return String("/c/") + deviceId + "/" + T_DEVICE_STATUS;
}

// Example: getPaymentPending("dev-1764691334-daa58e77") returns "/c/dev-1764691334-daa58e77/pp"
String FirmnginKit::getPaymentPending(String deviceId) {
    return String("/c/") + deviceId + "/" + T_PAYMENT_PENDING;
}

// Example: getPmOnPayment("dev-1764691334-daa58e77") returns "/c/dev-1764691334-daa58e77/mop"
String FirmnginKit::getPmOnPayment(String deviceId) {
    return String("/c/") + deviceId + "/" + T_PM_ON_PAYMENT;
}

// Example: getPmOnExpired("dev-1764691334-daa58e77") returns "/c/dev-1764691334-daa58e77/moe"
String FirmnginKit::getPmOnExpired(String deviceId) {
    return String("/c/") + deviceId + "/" + T_PM_ON_EXPIRED;
}

// Example: getPmOnSuccess("dev-1764691334-daa58e77") returns "/c/dev-1764691334-daa58e77/mos"
String FirmnginKit::getPmOnSuccess(String deviceId) {
    return String("/c/") + deviceId + "/" + T_PM_ON_SUCCESS;
}

String FirmnginKit::getDownstreamTopic(String deviceId) {
    return String("/d/") + deviceId + "/rs/+";
}

String FirmnginKit::getVirtualPinTopic(String deviceId, int vpin) {
    return String("/d/") + deviceId + "/rs/" + String(vpin);
}

String FirmnginKit::getPushStateTopic(String deviceId) {
    return String("/d/") + deviceId + "/ps";
}

String FirmnginKit::getPushBatchStateTopic(String deviceId) {
    return String("/d/") + deviceId + "/psb";
}

void FirmnginKit::begin() {
    if (!PLATFORM_SUPPORTED) return;

    printBanner();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("ERROR: WiFi not connected");
        delay(2000);
        ESP.restart();
        return;
    }

    syncTime();
    
    // Verify time is synced (critical for certificate validation)
    time_t now = time(nullptr);
    struct tm timeinfo;
    if (localtime_r(&now, &timeinfo)) {
        if (_debug) {
            Serial.print("System time: ");
            Serial.print(timeinfo.tm_year + 1900);
            Serial.print("-");
            Serial.print(timeinfo.tm_mon + 1);
            Serial.print("-");
            Serial.print(timeinfo.tm_mday);
            Serial.print(" ");
            Serial.print(timeinfo.tm_hour);
            Serial.print(":");
            Serial.print(timeinfo.tm_min);
            Serial.print(":");
            Serial.println(timeinfo.tm_sec);
        }
        // Check if time is reasonable (after 2020)
        if (timeinfo.tm_year < 120) {
            Serial.println("WARNING: System time may not be synced correctly!");
        }
    }
    
    if (_debug) {
        Serial.print("MQTT Server: ");
        Serial.print(_mqttServer);
        Serial.print(":");
        Serial.println(_mqttPort);
    }

#if defined(ESP8266)
    Serial.println("Configuring TLS...");
    
    // Validate client certificate and private key
    if (!_clientCert || strlen(_clientCert) < 50 || !_privateKey || strlen(_privateKey) < 50) {
        Serial.println("ERROR: Client certificate and private key are required but empty or invalid");
        return;
    }
    
    // Validate certificate format
    String clientCertStr = String(_clientCert);
    String privateKeyStr = String(_privateKey);
    if (clientCertStr.indexOf("-----BEGIN CERTIFICATE-----") == -1 ||
        clientCertStr.indexOf("-----END CERTIFICATE-----") == -1) {
        Serial.println("ERROR: Client certificate format invalid (missing BEGIN/END markers)");
        return;
    }
    if (privateKeyStr.indexOf("-----BEGIN") == -1 ||
        privateKeyStr.indexOf("-----END") == -1) {
        Serial.println("ERROR: Private key format invalid (missing BEGIN/END markers)");
        return;
    }
    
    // Validate fingerprint
    if (!_fingerprint) {
        Serial.println("ERROR: Server fingerprint is required but empty or invalid");
        return;
    }
    
    _clientCertList = new BearSSL::X509List(_clientCert);
    _clientPrivKey = new BearSSL::PrivateKey(_privateKey);
    _wifiClient.setClientRSACert(_clientCertList, _clientPrivKey);
    _wifiClient.setBufferSizes(512, 512);
    _wifiClient.setFingerprint(_fingerprint);
#elif defined(ESP32)
    Serial.println("Configuring TLS...");

    // Configure Root CA for server verification
    if (_caCert && strlen(_caCert) > 50) {
        // Validate certificate format
        String caCertStr = String(_caCert);
        if (caCertStr.indexOf("-----BEGIN CERTIFICATE-----") == -1 ||
            caCertStr.indexOf("-----END CERTIFICATE-----") == -1) {
            Serial.println("ERROR: CA certificate format invalid (missing BEGIN/END markers)");
            return;
        }
        _wifiClient.setCACert(_caCert);
        if (_debug) {
            Serial.println("Root CA certificate set for server verification");
        }
    } else if (_fingerprint) {
        _wifiClient.setInsecure();
    } else {
        Serial.println("ERROR: Either CA certificate or fingerprint is required");
        return;
    }

    // Configure client certificate and private key for mTLS
    if (_clientCert && strlen(_clientCert) > 50 && _privateKey && strlen(_privateKey) > 50) {
        // Validate certificate format
        String clientCertStr = String(_clientCert);
        String privateKeyStr = String(_privateKey);
        if (clientCertStr.indexOf("-----BEGIN CERTIFICATE-----") == -1 ||
            clientCertStr.indexOf("-----END CERTIFICATE-----") == -1) {
            Serial.println("ERROR: Client certificate format invalid (missing BEGIN/END markers)");
            return;
        }
        if (privateKeyStr.indexOf("-----BEGIN") == -1 ||
            privateKeyStr.indexOf("-----END") == -1) {
            Serial.println("ERROR: Private key format invalid (missing BEGIN/END markers)");
            return;
        }
        // TODO: just for a while
        _wifiClient.setInsecure();
        _wifiClient.setCertificate(_clientCert);
        _wifiClient.setPrivateKey(_privateKey);
        if (_debug) {
            Serial.println("Client certificate and private key set for TLS authentication");
        }
    } else {
        Serial.println("ERROR: Client certificate and private key are required but empty or invalid");
        return;
    }

    // Set TLS handshake timeout (30 seconds)
    _wifiClient.setTimeout(30);

    Serial.println("TLS configuration completed");
#endif

    _mqttClient.setServer(_mqttServer.c_str(), _mqttPort);
    _mqttClient.setCallback([this](char *topic, byte *payload, unsigned int length) {
        this->mqttCallback(topic, payload, length);
    });
    _mqttClient.setBufferSize(2048);
    _mqttClient.setKeepAlive(15);
    _mqttClient.setSocketTimeout(20);
}

void FirmnginKit::setMQTTServer(const char* server, int port) {
    _mqttServer = server;
    _mqttPort = port;
}

void FirmnginKit::setClient(Client& client) {
    _externalClient = &client;
    _mqttClient.setClient(client);
}

void FirmnginKit::setTimezone(int timezone) {
    if (timezone < -12 || timezone > 12) return;
    GMT_OFFSET_SEC = timezone * 3600;
}

void FirmnginKit::setNtpServer(const char *ntpServer) {
    NTP_SERVER = ntpServer;
}

void FirmnginKit::setDaylightOffsetSec(int daylightOffsetSec) {
    DAYLIGHT_OFFSET_SEC = daylightOffsetSec;
}

void FirmnginKit::syncTime() {
    if (_debug) {
        Serial.print("Syncing time from NTP server: ");
        Serial.println(NTP_SERVER);
    }
    
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    time_t now = time(nullptr);
    int timeout = 0;
    int maxWait = 20; // Wait up to 2 seconds (20 * 100ms)
    
    // Wait for time to be set (time should be > Jan 1, 2020)
    while (now < 1577836800 && timeout < maxWait) {
        delay(100);
        now = time(nullptr);
        timeout++;
    }
    
    if (now < 1577836800) {
        Serial.println("WARNING: Time sync may have failed, certificate validation might fail!");
    } else if (_debug) {
        Serial.println("Time synchronized");
    }
}

void FirmnginKit::onStateMonetize(DeviceStateType state, StateCallbackFunction callback) {
    _stateCallbacks[String(STATE_NAMES[state])] = callback;
}

void FirmnginKit::onCommand(const char* command, StateCallbackFunction callback) {
    _commandCallbacks[String(command)] = callback;
}

void FirmnginKit::onCommand(DeviceStateType command, StateCallbackFunction callback) {
    _commandCallbacks[String(STATE_NAMES[command])] = callback;
}

void FirmnginKit::onVirtualPin(int pinId, VirtualPinCallbackFunction callback) {
    _virtualPinCallbacks[pinId] = callback;
}

void FirmnginKit::registerVirtualPin(int pinId, VirtualPinCallbackFunction callback) {
    _virtualPinCallbacks[pinId] = callback;
}

void onVirtualPin(int pinId, VirtualPinCallbackFunction callback) {
    if (_globalFirmnginKitInstance) {
        _globalFirmnginKitInstance->onVirtualPin(pinId, callback);
    }
}

void FirmnginKit::pushState(String key, String value) {
    if (!_mqttClient.connected()) {
        if (_debug) {
            Serial.println("Cannot push state: MQTT not connected");
        }
        return;
    }

    String topic = getPushStateTopic(_deviceId);
#if ARDUINOJSON_VERSION_MAJOR >= 7
    JsonDocument doc;
#else
    DynamicJsonDocument doc(512);
#endif
    doc["key"] = key;
    doc["value"] = value;
    
    String payload;
    serializeJson(doc, payload);
    
    bool published = _mqttClient.publish(topic.c_str(), payload.c_str());
    
    if (_debug) {
        if (!published) {
            Serial.print("Failed to push state: ");
            Serial.print(key);
            Serial.print(" = ");
            Serial.print(value);
            Serial.println();
        }
    }
}

void FirmnginKit::pushState(int key, String value) {
    pushState(String(key), value);
}

void FirmnginKit::pushState(String key, int value) {
    pushState(key, String(value));
}

void FirmnginKit::pushState(int key, int value) {
    pushState(String(key), String(value));
}

void FirmnginKit::pushState(String key, float value) {
    pushState(key, String(value));
}

void FirmnginKit::pushState(int key, float value) {
    pushState(String(key), String(value));
}

void FirmnginKit::pushState(String key, double value) {
    pushState(key, String(value));
}

void FirmnginKit::pushState(int key, double value) {
    pushState(String(key), String(value));
}

bool FirmnginKit::publishBatchState(String payload) {
    if (!_mqttClient.connected()) {
        return false;
    }

    String topic = getPushBatchStateTopic(_deviceId);
    bool published = _mqttClient.publish(topic.c_str(), payload.c_str());
    
    if (_debug) {
        if (!published) {
            Serial.print("Failed to push batch state: ");
            Serial.println(payload);
        }
    }
    return published;
}

BatchState FirmnginKit::pushBatchState() {
    return BatchState();
}

void FirmnginKit::setupLWT() {
    String willTopic = "/d/" + String(_deviceId) + "/lwt";
}

void FirmnginKit::loop() {
    if (!PLATFORM_SUPPORTED || WiFi.status() != WL_CONNECTED) return;

    static unsigned long lastReconnectAttempt = 0;
    static unsigned long backoffDelay = 5000;

    if (!_mqttClient.connected())
    {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > backoffDelay)
        {
            lastReconnectAttempt = now;
            backoffDelay = min(backoffDelay * 2, 60000UL);
            if (connectServer()) {
                backoffDelay = 5000;
            }
        }
    } else {
        _mqttClient.loop();
    }
}

void FirmnginKit::_Debug(String message, bool newLine) {
    if (_debug) {
        if (newLine) Serial.println(message);
        else Serial.print(message);
    }
}

void FirmnginKit::setDebug(bool debug) {
    _debug = debug;
}

bool FirmnginKit::isPlatformSupported() {
    return PLATFORM_SUPPORTED;
}

bool FirmnginKit::connectServer() {
    setupLWT();
    
    int retryCount = 0;
    while (!_mqttClient.connected() && retryCount < maxRetryMQTT)
    {
        unsigned long now = millis();
        if (now - _lastMQTTAttempt >= _delayRetryMQTT)
        {
            _lastMQTTAttempt = now;
            if (_debug) {
                Serial.print("connecting to Server (");
                Serial.print(retryCount + 1);
                Serial.println(")");
            }
            String willTopic = "/d/" + String(_deviceId) + "/lwt";
            String willMessage = "0";
            
            if (_debug) {
                Serial.print("Attempting connection to server");
                Serial.print(" with ");
                Serial.println(_deviceId);
            }
            
            _mqttClient.disconnect();
            bool connected = _mqttClient.connect(_deviceId, willTopic.c_str(), 1, true, willMessage.c_str());

            if (connected) {
                _mqttClient.subscribe(getPaymentSuccess(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getDeviceStatus(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getPaymentPending(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getPmOnPayment(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getPmOnExpired(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getPmOnSuccess(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getDownstreamTopic(_deviceId).c_str(), defaultQos);

                _mqttClient.publish(willTopic.c_str(), "", true);
                delay(10);
                
                _mqttClient.publish(willTopic.c_str(), "1", true);
                if (_debug) {
                    Serial.println("Connected to firmngin.dev");
                }
                Serial.println("Ready...");
                return true;
            } else {
                int mqttState = _mqttClient.state();
                Serial.print("Connection failed, rc=");
                Serial.print(mqttState);

                if (_debug) {
                    Serial.print(" (");
                    switch(mqttState) {
                        case -4: Serial.print("MQTT_CONNECTION_TIMEOUT"); break;
                        case -3: Serial.print("MQTT_CONNECTION_LOST"); break;
                        case -2: {
                            Serial.print("MQTT_CONNECT_FAILED - TLS/certificate error");
                            break;
                        }
                        case -1: Serial.print("MQTT_DISCONNECTED"); break;
                        case 1: Serial.print("MQTT_CONNECT_BAD_PROTOCOL"); break;
                        case 2: Serial.print("MQTT_CONNECT_BAD_CLIENT_ID"); break;
                        case 3: Serial.print("MQTT_CONNECT_UNAVAILABLE"); break;
                        case 4: Serial.print("MQTT_CONNECT_BAD_CREDENTIALS"); break;
                        case 5: Serial.print("MQTT_CONNECT_UNAUTHORIZED"); break;
                        default: Serial.print("UNKNOWN"); break;
                    }
                    Serial.print(")");
                }
                Serial.println();
                retryCount++;
            }
        }
    }

    if (!_mqttClient.connected()) {
        Serial.println("Connection failed, restarting...");
        delay(1000);
        ESP.restart();
    }

    return false;
}

void FirmnginKit::mqttCallback(char *topic, byte *payload, unsigned int length) {
    String payloadStr;
    payloadStr.reserve(length);
    for (unsigned int i = 0; i < length; i++) {
        payloadStr += (char)payload[i];
    }

    String topicStr = String(topic);
    String expectedPrefix = String("/d/") + String(_deviceId) + "/rs/";
    
    // Check if topic matches pattern: /d/{deviceId}/rs/{vpin}
    if (topicStr.startsWith(expectedPrefix)) {
        // Extract VPIN ID from topic: /d/{deviceId}/rs/{vpin}
        String vpinStr = topicStr.substring(expectedPrefix.length());
        int vpinId = vpinStr.toInt();
        
        if (vpinId > 0) {
            if (_virtualPinCallbacks.count(vpinId) > 0) {
                // Payload is directly a string, pass it to handler
                _virtualPinCallbacks[vpinId](payloadStr);
            } else if (_debug) {
                Serial.print("No handler registered for virtual pin: ");
                Serial.println(vpinId);
            }
        } else if (_debug) {
            Serial.print("Invalid virtual pin ID in topic: ");
            Serial.println(topicStr);
        }
        return;
    }

    String stateType = "";
    
    int lastSlash = topicStr.lastIndexOf('/');
    if (lastSlash >= 0) {
        stateType = topicStr.substring(lastSlash + 1);
    }

    DeviceState state(stateType, payloadStr);

    if (_stateCallbacks.count(stateType) > 0) {
        _stateCallbacks[stateType](state);
    }

    if (_commandCallbacks.count(stateType) > 0) {
        _commandCallbacks[stateType](state);
    }
}

FirmnginKit &FirmnginKit::endSession() {
    String topic = "fngin/";
    topic += _deviceId;

    if (_mqttClient.connected()) {
#if ARDUINOJSON_VERSION_MAJOR >= 7
        JsonDocument doc;
#else
        DynamicJsonDocument doc(256);
#endif
        doc["state"] = "end_session";
        String payload;
        serializeJson(doc, payload);
        _mqttClient.publish(topic.c_str(), payload.c_str());
    }
    return *this;
}
