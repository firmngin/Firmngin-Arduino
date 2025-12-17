#ifndef FIRMNGINKIT_H
#define FIRMNGINKIT_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <time.h>
#include <map>
#include <functional>

// ArduinoJson v6/v7 compatibility
#if ARDUINOJSON_VERSION_MAJOR >= 7
  #define JSON_DOC JsonDocument
  #define JSON_ARRAY_ADD_OBJECT() add<JsonObject>()
#else
  #define JSON_DOC DynamicJsonDocument
  #define JSON_ARRAY_ADD_OBJECT() createNestedObject()
#endif

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
typedef std::function<void(String)> VirtualPinCallbackFunction;

class FirmnginKit;
class BatchState;
extern FirmnginKit* _globalFirmnginKitInstance;

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
    void setClient(Client& client);
    bool isPlatformSupported();
    FirmnginKit &endSession();

    void onStateMonetize(DeviceStateType state, StateCallbackFunction callback);
    void onCommand(const char* command, StateCallbackFunction callback);
    void onCommand(DeviceStateType command, StateCallbackFunction callback);
    void pushState(String key, String value);
    void pushState(int key, String value);
    void pushState(String key, int value);
    void pushState(int key, int value);
    void pushState(String key, float value);
    void pushState(int key, float value);
    void pushState(String key, double value);
    void pushState(int key, double value);
    BatchState pushBatchState();
    bool publishBatchState(String payload);
    void onVirtualPin(int pinId, VirtualPinCallbackFunction callback);
    void registerVirtualPin(int pinId, VirtualPinCallbackFunction callback);

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
    Client* _externalClient = nullptr;
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
    std::map<int, VirtualPinCallbackFunction> _virtualPinCallbacks;

    void _Debug(String message, bool newLine = true);
    bool connectServer();
    void mqttCallback(char *topic, byte *payload, unsigned int length);
    String getPaymentSuccess(String deviceId);
    String getDeviceStatus(String deviceId);
    String getPaymentPending(String deviceId);
    String getPmOnPayment(String deviceId);
    String getPmOnExpired(String deviceId);
    String getPmOnSuccess(String deviceId);
    String getDownstreamTopic(String deviceId);
    String getVirtualPinTopic(String deviceId, int vpin);
    String getPushStateTopic(String deviceId);
    String getPushBatchStateTopic(String deviceId);
    void syncTime();
    void setupLWT();
};

void onVirtualPin(int pinId, VirtualPinCallbackFunction callback);

// BatchState: Builder pattern for batch push
class BatchState {
private:
#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument _doc;
#else
  DynamicJsonDocument _doc;
#endif
  JsonArray _array;
  
public:
#if ARDUINOJSON_VERSION_MAJOR >= 7
  BatchState() {
    _array = _doc.to<JsonArray>();
  }
#else
  BatchState() : _doc(2048) {
    _array = _doc.to<JsonArray>();
  }
#endif
  
  BatchState& add(String key, String value) {
    JsonObject obj = _array.JSON_ARRAY_ADD_OBJECT();
    obj["key"] = key;
    obj["value"] = value;
    return *this;
  }
  
  // String literal overloads
  BatchState& add(const char* key, const char* value) {
    return add(String(key), String(value));
  }
  
  BatchState& add(String key, const char* value) {
    return add(key, String(value));
  }
  
  // Numeric value overloads (auto convert to String)
  BatchState& add(String key, int value) {
    return add(key, String(value));
  }
  
  BatchState& add(String key, double value) {
    return add(key, String(value));
  }
  
  BatchState& add(int key, int value) {
    return add(String(key), String(value));
  }
  
  BatchState& add(int key, double value) {
    return add(String(key), String(value));
  }
  
  BatchState& add(const char* key, int value) {
    return add(String(key), String(value));
  }
  
  BatchState& add(const char* key, double value) {
    return add(String(key), String(value));
  }
  
  // String object overloads (for String variables)
  BatchState& add(int key, String value) {
    return add(String(key), value);
  }
  
  BatchState& add(const char* key, String value) {
    return add(String(key), value);
  }
  
  bool send() {
    if (_globalFirmnginKitInstance) {
      String payload;
      serializeJson(_doc, payload);
      return _globalFirmnginKitInstance->publishBatchState(payload);
    }
    return false;
  }
  
  int count() { return _array.size(); }
  void clear() { _array.clear(); }
};

// GPIO mode enum
enum PinMode { NONE, DIGITAL, PWM, ACTIVE_LOW };

// VPin: Unified Virtual Pin for both receive and push
// - Receive: control GPIO from server commands
// - Push: send sensor data with smart conditions
class VPin {
private:
  int _vpin;
  int _gpio;
  PinMode _mode;
  
  // Push state variables
  float _lastValue = 0;
  unsigned long _lastPush = 0;
  bool _onChangeEnabled = false;
  unsigned long _intervalMs = 0;
  float _threshold = 0;
  bool _firstPush = true;

public:
  // Constructor for push only (no GPIO)
  VPin(int vpin) : _vpin(vpin), _gpio(-1), _mode(NONE) {}
  
  // Constructor for receive (with GPIO)
  VPin(int vpin, int gpio, PinMode mode = DIGITAL) 
    : _vpin(vpin), _gpio(gpio), _mode(mode) {
    if (_gpio >= 0) {
      pinMode(_gpio, OUTPUT);
      if (_mode == ACTIVE_LOW) {
        digitalWrite(_gpio, HIGH);
      } else if (_mode == DIGITAL) {
        digitalWrite(_gpio, LOW);
      } else if (_mode == PWM) {
        analogWrite(_gpio, 0);
      }
    }
  }
  
  // === RECEIVE: Handle incoming payload from server ===
  void handle(String payload) {
    if (_gpio < 0) return;
    payload.toUpperCase();
    if (_mode == PWM) {
      int value = payload.toInt();
      value = constrain(value, 0, 255);
      analogWrite(_gpio, value);
    } else {
      bool state = (payload == "ON" || payload == "1" || payload == "HIGH");
      digitalWrite(_gpio, (_mode == ACTIVE_LOW) ? !state : state);
    }
  }
  
  // === RECEIVE: Manual GPIO control ===
  void set(bool state) {
    if (_gpio < 0) return;
    digitalWrite(_gpio, (_mode == ACTIVE_LOW) ? !state : state);
  }
  
  void setValue(int value) {
    if (_gpio < 0) return;
    analogWrite(_gpio, constrain(value, 0, 255));
  }
  
  void on() { set(true); }
  void off() { set(false); }
  
  // === PUSH: Fluent API for conditions ===
  VPin& onChange() {
    _onChangeEnabled = true;
    return *this;
  }
  
  VPin& interval(unsigned long ms) {
    _intervalMs = ms;
    return *this;
  }
  
  VPin& threshold(float delta) {
    _threshold = delta;
    return *this;
  }
  
  // === PUSH: Send value with conditions check ===
  bool push(float value) {
    unsigned long now = millis();
    bool shouldPush = false;
    
    if (_firstPush) {
      _firstPush = false;
      shouldPush = true;
    }
    else if (!_onChangeEnabled && _intervalMs == 0 && _threshold == 0) {
      shouldPush = true;
    }
    else {
      bool changeOk = !_onChangeEnabled || (value != _lastValue);
      bool intervalOk = _intervalMs == 0 || (now - _lastPush >= _intervalMs);
      bool thresholdOk = _threshold == 0 || (abs(value - _lastValue) >= _threshold);
      shouldPush = changeOk && intervalOk && thresholdOk;
    }
    
    if (shouldPush) {
      _lastValue = value;
      _lastPush = now;
      if (_globalFirmnginKitInstance) {
        _globalFirmnginKitInstance->pushState(_vpin, String(value));
      }
      return true;
    }
    return false;
  }
  
  void push(String value) {
    if (_globalFirmnginKitInstance) {
      _globalFirmnginKitInstance->pushState(_vpin, value);
    }
  }
  
  void forcePush(float value) {
    _lastValue = value;
    _lastPush = millis();
    if (_globalFirmnginKitInstance) {
      _globalFirmnginKitInstance->pushState(_vpin, String(value));
    }
  }
  
  // === Getters ===
  int getVpin() { return _vpin; }
  int getGpio() { return _gpio; }
  float getLastValue() { return _lastValue; }
  bool hasGpio() { return _gpio >= 0; }
};

// Alias for backward compatibility
typedef VPin PinMap;
typedef VPin StatePin;

// Macro for global Virtual Pin registration
#define ON_VPIN_1(pm) \
  static struct __VPinReg_##pm { \
    __VPinReg_##pm() { fngin.onVirtualPin(pm.getVpin(), [](String p) { pm.handle(p); }); } \
  } __vpinReg_##pm

#define ON_VPIN_2(pin, handler) \
  static struct __VPinReg_##pin { \
    __VPinReg_##pin() { fngin.onVirtualPin(pin, handler); } \
  } __vpinReg_##pin

#define GET_ON_VPIN(_1, _2, NAME, ...) NAME
#define ON_VPIN(...) GET_ON_VPIN(__VA_ARGS__, ON_VPIN_2, ON_VPIN_1)(__VA_ARGS__)

#endif // FIRMNGINKIT_H
