#ifndef FIRMNGINKIT_H
#define FIRMNGINKIT_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <time.h>
#include <map>
#include <functional>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#define PLATFORM_SUPPORTED true
#define PLATFORM_NAME "ESP8266"
#elif defined(ESP32)
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#define PLATFORM_SUPPORTED true
#define PLATFORM_NAME "ESP32"
#else
#define PLATFORM_SUPPORTED false
#define PLATFORM_NAME "UNKNOWN"
#endif

// Default MQTT Server Configuration
#define DEFAULT_MQTT_SERVER "asia-jkt1.firmngin.dev"
#define DEFAULT_MQTT_PORT 8883

#define OK "on_ok"

// MQTT Topics
#define T_PAYMENT_SUCCESS "pm"
#define T_DEVICE_STATUS "ds"
#define T_PAYMENT_PENDING "pp"
#define T_PM_ON_PAYMENT "mop"
#define T_PM_ON_EXPIRED "moe"
#define T_PM_ON_SUCCESS "mos"

enum DeviceStateType {
    PAYMENT_SUCCESS,
    DEVICE_STATUS,
    PAYMENT_PENDING,
    CUSTOM_ON_PENDING_PAYMENTS,
    CUSTOM_ON_EXPIRED_PAYMENTS,
    CUSTOM_ON_SUCCESS_PAYMENTS
};

__attribute__((unused)) static const char* STATE_NAMES[] = {
    "pm",
    "ds",
    "pp",
    "mop",
    "moe",
    "mos"
};

class DeviceState {
private:
    String _state;
    String _rawPayload;

public:
    DeviceState(String state, String rawPayload) : _state(state), _rawPayload(rawPayload) {}
    String getState() const { return _state; }
    String getPayload() const { return _rawPayload; }
};

typedef std::function<void(DeviceState)> StateCallbackFunction;

class FirmnginKit
{
public:
#if defined(ESP8266)
    FirmnginKit(const char *deviceId, const char *deviceKey, const char* clientCert, const char* privateKey, const uint8_t* fingerprint);
#elif defined(ESP32)
    FirmnginKit(const char *deviceId, const char *deviceKey, const char* caCert, const char* clientCert, const char* privateKey);
    FirmnginKit(const char *deviceId, const char *deviceKey, const uint8_t* fingerprint, const char* clientCert, const char* privateKey);
#else
    FirmnginKit(const char *deviceId, const char *deviceKey);
#endif
    ~FirmnginKit();

    void begin();
    void loop();
    void setDebug(bool debug);
    void setTimezone(int timezone);
    void setDaylightOffsetSec(int daylightOffsetSec);
    void setNtpServer(const char *ntpServer);
    void setMQTTServer(const char* server, int port);
    bool isPlatformSupported();
    FirmnginKit &endSession();

    void onState(const char* state, StateCallbackFunction callback);
    void onState(DeviceStateType state, StateCallbackFunction callback);
    void onCommand(const char* command, StateCallbackFunction callback);
    void onCommand(DeviceStateType command, StateCallbackFunction callback);

private:
    const char *_deviceId;
    const char *_deviceKey;
    bool _debug;
    unsigned long _lastMQTTAttempt;
    unsigned long _lastHeartbeat;
    const unsigned long _heartbeatInterval = 20000;
    const char *BASE_URL = "https://apikit.firmngin.cloud";

#if defined(ESP8266) || defined(ESP32)
    WiFiClientSecure _wifiClient;
#else
    WiFiClient _wifiClient;
#endif
    PubSubClient _mqttClient;
    unsigned long _delayRetryMQTT = 5000;
    int maxRetryMQTT = 3;
    int defaultQos = 1;

    String _mqttServer;
    int _mqttPort;

#if defined(ESP8266)
    const char* _clientCert = nullptr;
    const char* _privateKey = nullptr;
    const uint8_t* _fingerprint = nullptr;
    BearSSL::X509List *_clientCertList = nullptr;
    BearSSL::PrivateKey *_clientPrivKey = nullptr;
#elif defined(ESP32)
    const char* _caCert = nullptr;
    const char* _clientCert = nullptr;
    const char* _privateKey = nullptr;
    const uint8_t* _fingerprint = nullptr;
#endif

    std::map<String, StateCallbackFunction> _stateCallbacks;
    std::map<String, StateCallbackFunction> _commandCallbacks;

    void _Debug(String message, bool newLine = true);
    bool connectServer();
    void mqttCallback(char *topic, byte *payload, unsigned int length);
    String getPaymentSuccess(String deviceId);
    String getDeviceStatus(String deviceId);
    String getPaymentPending(String deviceId);
    String getPmOnPayment(String deviceId);
    String getPmOnExpired(String deviceId);
    String getPmOnSuccess(String deviceId);
    void syncTime();
    void setupLWT();
};

#endif // FIRMNGINKIT_H
