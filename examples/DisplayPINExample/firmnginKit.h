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

// Check if keys.h exists for mTLS support
#if __has_include("keys.h")
#define KEYS_H_AVAILABLE 1
#include "keys.h"
#else
#define KEYS_H_AVAILABLE 0
#endif

#define OK "on_ok"

// MQTT Topics
#define TOPIC_PAYMENT "pm"
#define TOPIC_DEVICE_STATUS "ds"
#define TOPIC_PAYMENT_PROCESS "pp"
#define TOPIC_MACHINE_OPERATION "mop"
#define TOPIC_MACHINE_OPERATION_END "moe"
#define TOPIC_MACHINE_OPERATION_START "mos"

// Readable State Constants (enum)
enum DeviceStateType
{
    PAYMENT,
    DEVICE_STATUS,
    PAYMENT_PROCESS,
    MACHINE_OPERATION,
    MACHINE_OPERATION_END,
    MACHINE_OPERATION_START,
    INIT,
    DISPLAY_PIN,
    VERIFICATION_RESULT,
    MENU_ITEMS,
    USAGE_RESPONSE,
    LIMIT_EXCEEDED,
    NEAR_LIMIT
};

// State name mapping for MQTT topics
static const char *STATE_NAMES[] = {
    "pm",   // PAYMENT
    "ds",   // DEVICE_STATUS
    "pp",   // PAYMENT_PROCESS
    "mop",  // MACHINE_OPERATION
    "moe",  // MACHINE_OPERATION_END
    "mos",  // MACHINE_OPERATION_START
    "init", // INIT
    "dpin", // DISPLAY_PIN_NUMBER
    "vr",   // VERIFICATION_RESULT
    "mi",   // MENU_ITEMS
    "ur",   // USAGE_RESPONSE
    "le",   // LIMIT_EXCEEDED
    "nl"    // NEAR_LIMIT
};

// Minimal DeviceState class - returns raw payload only
class DeviceState
{
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
    FirmnginKit(const char *deviceId, const char *deviceKey);
    ~FirmnginKit();

    void begin();
    void loop();
    void setDebug(bool debug);
    void setTimezone(int timezone);
    void setDaylightOffsetSec(int daylightOffsetSec);
    void setNtpServer(const char *ntpServer);
    void setMQTTServer(const char *server, int port);
    bool isPlatformSupported();

    void onState(const char *state, StateCallbackFunction callback);
    void onState(DeviceStateType state, StateCallbackFunction callback);
    void onCommand(const char *command, StateCallbackFunction callback);
    void onCommand(DeviceStateType command, StateCallbackFunction callback);

private:
    const char *_deviceId;
    const char *_deviceKey;
    bool _debug;
    unsigned long _lastMQTTAttempt;

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

#if defined(ESP8266) && KEYS_H_AVAILABLE
    BearSSL::X509List *_clientCertList = nullptr;
    BearSSL::PrivateKey *_clientPrivKey = nullptr;
#endif

    std::map<String, StateCallbackFunction> _stateCallbacks;
    std::map<String, StateCallbackFunction> _commandCallbacks;

    void _Debug(String message, bool newLine = true);
    bool connectServer();
    void mqttCallback(char *topic, byte *payload, unsigned int length);
    String getTopicPayment(String deviceId);
    String getTopicDeviceStatus(String deviceId);
    String getTopicPaymentProcess(String deviceId);
    String getTopicMachineOperation(String deviceId);
    String getTopicMachineOperationEnd(String deviceId);
    String getTopicMachineOperationStart(String deviceId);
    String getTopicInit(String deviceId);
    String getTopicDisplayPIN(String deviceId);
    String getTopicVerificationResult(String deviceId);
    String getTopicMenuItems(String deviceId);
    String getTopicUsageResponse(String deviceId);
    String getTopicLimitExceeded(String deviceId);
    String getTopicNearLimit(String deviceId);
    void syncTime();
    void setupLWT();
};

#endif // FIRMNGINKIT_H
