#include "firmnginKit.h"

const char *NTP_SERVER = "pool.ntp.org";
int GMT_OFFSET_SEC = 7 * 3600;
int DAYLIGHT_OFFSET_SEC = 0;

// Use mqtt_server from keys.h if available, otherwise use default from header
#if KEYS_H_AVAILABLE
const char* MQTT_SERVER_ADDR = mqtt_server;
const int MQTT_SERVER_PORT = mqtt_port;
#else
const char* MQTT_SERVER_ADDR = DEFAULT_MQTT_SERVER;
const int MQTT_SERVER_PORT = DEFAULT_MQTT_PORT;
#endif

FirmnginKit::FirmnginKit(const char *deviceId, const char *deviceKey)
    : _deviceId(deviceId),
      _deviceKey(deviceKey),
      _debug(false),
      _lastMQTTAttempt(0),
      _mqttClient(_wifiClient),
      _mqttServer(MQTT_SERVER_ADDR),
      _mqttPort(MQTT_SERVER_PORT)
{
    if (!PLATFORM_SUPPORTED) {
        Serial.println("ERROR: Platform not supported");
    }
}

FirmnginKit::~FirmnginKit() {
#if defined(ESP8266) && KEYS_H_AVAILABLE
    delete _clientCertList;
    delete _clientPrivKey;
#endif
}

String FirmnginKit::getTopicPayment(String deviceId) {
    return String("/c/") + deviceId + "/" + TOPIC_PAYMENT;
}

String FirmnginKit::getTopicDeviceStatus(String deviceId) {
    return String("/c/") + deviceId + "/" + TOPIC_DEVICE_STATUS;
}

String FirmnginKit::getTopicPaymentProcess(String deviceId) {
    return String("/c/") + deviceId + "/" + TOPIC_PAYMENT_PROCESS;
}

String FirmnginKit::getTopicMachineOperation(String deviceId) {
    return String("/c/") + deviceId + "/" + TOPIC_MACHINE_OPERATION;
}

String FirmnginKit::getTopicMachineOperationEnd(String deviceId) {
    return String("/c/") + deviceId + "/" + TOPIC_MACHINE_OPERATION_END;
}

String FirmnginKit::getTopicMachineOperationStart(String deviceId) {
    return String("/c/") + deviceId + "/" + TOPIC_MACHINE_OPERATION_START;
}

String FirmnginKit::getTopicInit(String deviceId) {
    return String("/c/") + deviceId + "/init";
}

String FirmnginKit::getTopicDisplayPIN(String deviceId) {
    return String("/c/") + deviceId + "/dpin";
}

String FirmnginKit::getTopicVerificationResult(String deviceId) {
    return String("/c/") + deviceId + "/vr";
}

String FirmnginKit::getTopicMenuItems(String deviceId) {
    return String("/c/") + deviceId + "/mi";
}

String FirmnginKit::getTopicUsageResponse(String deviceId) {
    return String("/c/") + deviceId + "/ur";
}

String FirmnginKit::getTopicLimitExceeded(String deviceId) {
    return String("/c/") + deviceId + "/le";
}

String FirmnginKit::getTopicNearLimit(String deviceId) {
    return String("/c/") + deviceId + "/nl";
}

void FirmnginKit::begin() {
    if (!PLATFORM_SUPPORTED) return;

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("ERROR: WiFi not connected");
        delay(2000);
        ESP.restart();
        return;
    }

    syncTime();

#if defined(ESP8266)
    #if KEYS_H_AVAILABLE
        _clientCertList = new BearSSL::X509List(CLIENT_CERT);
        _clientPrivKey = new BearSSL::PrivateKey(PRIVATE_KEY);
        _wifiClient.setClientRSACert(_clientCertList, _clientPrivKey);
        _wifiClient.setBufferSizes(512, 512);
        _wifiClient.setFingerprint(SERVER_FINGERPRINT_BYTES);
    #else
        Serial.println("ERROR: keys.h not found");
        return;
    #endif
#elif defined(ESP32)
    #if KEYS_H_AVAILABLE
        _wifiClient.setCACert(CA_CERT);
        _wifiClient.setCertificate(CLIENT_CERT);
        _wifiClient.setPrivateKey(PRIVATE_KEY);
    #else
        Serial.println("ERROR: keys.h not found");
        return;
    #endif
#endif

    _mqttClient.setServer(_mqttServer.c_str(), _mqttPort);
    _mqttClient.setCallback([this](char *topic, byte *payload, unsigned int length) {
        this->mqttCallback(topic, payload, length);
    });
    _mqttClient.setBufferSize(2048);
    _mqttClient.setKeepAlive(30);
    _mqttClient.setSocketTimeout(10);
}

void FirmnginKit::setMQTTServer(const char* server, int port) {
    _mqttServer = server;
    _mqttPort = port;
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
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    time_t now = time(nullptr);
    int timeout = 0;
    while (now < 8 * 3600 * 2 && timeout < 100) {
        delay(100);
        now = time(nullptr);
        timeout++;
    }
}

void FirmnginKit::onState(const char* state, StateCallbackFunction callback) {
    _stateCallbacks[String(state)] = callback;
}

void FirmnginKit::onState(DeviceStateType state, StateCallbackFunction callback) {
    _stateCallbacks[String(STATE_NAMES[state])] = callback;
}

void FirmnginKit::onCommand(const char* command, StateCallbackFunction callback) {
    _commandCallbacks[String(command)] = callback;
}

void FirmnginKit::onCommand(DeviceStateType command, StateCallbackFunction callback) {
    _commandCallbacks[String(STATE_NAMES[command])] = callback;
}

void FirmnginKit::setupLWT() {
    String willTopic = "/d/" + String(_deviceId) + "/lwt";
    _Debug("LWT: " + willTopic);
}

void FirmnginKit::loop() {
    if (!PLATFORM_SUPPORTED || WiFi.status() != WL_CONNECTED) return;

    static unsigned long lastReconnectAttempt = 0;
    static int backoffDelay = 5000;
    static bool firstConnect = true;

    if (!_mqttClient.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > backoffDelay) {
            lastReconnectAttempt = now;
            backoffDelay = min(backoffDelay * 2, 60000);
            if (connectServer()) {
                backoffDelay = 5000;
                firstConnect = false;
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
    while (!_mqttClient.connected() && retryCount < maxRetryMQTT) {
        unsigned long now = millis();
        if (now - _lastMQTTAttempt >= _delayRetryMQTT) {
            _lastMQTTAttempt = now;
            Serial.print("Connecting to MQTT (");
            Serial.print(retryCount + 1);
            Serial.println(")");

            String willTopic = "/d/" + String(_deviceId) + "/lwt";
            String willMessage = "0";
            
            bool connected = _mqttClient.connect(_deviceId, willTopic.c_str(), 1, true, willMessage.c_str());

            if (connected) {
                _mqttClient.subscribe(getTopicPayment(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getTopicDeviceStatus(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getTopicPaymentProcess(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getTopicMachineOperation(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getTopicMachineOperationEnd(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getTopicMachineOperationStart(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getTopicInit(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getTopicDisplayPIN(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getTopicVerificationResult(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getTopicMenuItems(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getTopicUsageResponse(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getTopicLimitExceeded(_deviceId).c_str(), defaultQos);
                _mqttClient.subscribe(getTopicNearLimit(_deviceId).c_str(), defaultQos);

                _mqttClient.publish(willTopic.c_str(), "1", true);

                Serial.println("Connected!");
                return true;
            } else {
                Serial.print("Failed, rc=");
                Serial.println(_mqttClient.state());
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

    if (_debug) {
        Serial.print("[");
        Serial.print(topic);
        Serial.print("]: ");
        Serial.println(payloadStr);
    }

    String topicStr = String(topic);
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


