#ifndef FIRMNGINKIT_H
#define FIRMNGINKIT_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <time.h>
#include <map>
#include <vector>
#include <functional>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#define PLATFORM_SUPPORTED true
#define PLATFORM_NAME "ESP8266"
#elif defined(ESP32)
#include <WiFi.h>
#include <WiFiClientSecure.h>
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

#ifndef FIRMNGIN_SERVER_ADDR
#define FIRMNGIN_SERVER_ADDR DEFAULT_MQTT_SERVER
#endif

#ifndef FIRMNGIN_SERVER_PORT
#define FIRMNGIN_SERVER_PORT DEFAULT_MQTT_PORT
#endif

#define OK "on_ok"

#define PATH_PAYMENT "pm"
#define PATH_DEVICE_STATUS "ds"
#define PATH_PENDING_PAYMENT "pp"
#define PATH_METADATA_ON_PENDING "mop"
#define PATH_METADATA_ON_EXPIRED "moe"
#define PATH_METADATA_ON_SUCCESS "mos"

// Readable State Constants (enum)
enum DeviceStateType
{
    PAYMENT,
    DEVICE_STATUS,
    PENDING_PAYMENT,
    METADATA_ON_PENDING,
    METADATA_ON_EXPIRED,
    METADATA_ON_SUCCESS,
    INIT,
    DISPLAY_PIN,
    VERIFICATION_RESULT,
    USAGE_RESPONSE,
    LIMIT_EXCEEDED,
    NEAR_LIMIT,
    VERIFICATIONS,
    PAYMENTS,
    USAGES,
    ENTITIES
};

// State name mapping for paths
static const char *STATE_NAMES[] = {
    "pm",   // PAYMENT
    "ds",   // DEVICE_STATUS
    "pp",   // PENDING_PAYMENT
    "mop",  // METADATA_ON_PENDING
    "moe",  // METADATA_ON_EXPIRED
    "mos",  // METADATA_ON_SUCCESS
    "init", // INIT
    "dpin", // DISPLAY_PIN_NUMBER
    "vr",   // VERIFICATION_RESULT
    "ur",   // USAGE_RESPONSE
    "le",   // LIMIT_EXCEEDED
    "nl",   // NEAR_LIMIT
    "vr",   // VERIFICATIONS
    "pay",  // PAYMENTS
    "usg",  // USAGES
    "rs",   // ENTITIES
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

// Typed payload for verification flow (dpin + vr)
class Verifications
{
private:
    String _pin;
    String _sessionId;
    int _ttl;
    bool _pinMet;
    bool _preconditionMet;
    bool _valid;
    String _rawPayload;

public:
    Verifications() : _pin(""), _sessionId(""), _ttl(0), _pinMet(false), _preconditionMet(false), _valid(false) {}
    Verifications(const String &jsonPayload);

    bool isValid() const { return _valid; }
    bool hasPinNumber() const { return _pin.length() > 0; }
    bool hasResult() const { return _valid && _pin.length() == 0; }
    String pin() const { return _pin; }
    String sessionId() const { return _sessionId; }
    int ttl() const { return _ttl; }
    bool pinMet() const { return _pinMet; }
    bool preconditionMet() const { return _preconditionMet; }
    String metadata() const { return _rawPayload; }
};

// Typed payload for payment flow (pp + pm)
class Payments
{
private:
    String _itemTitle;
    String _price;
    String _orderId;
    bool _isPending;
    bool _isSuccess;
    bool _valid;
    String _rawPayload;

public:
    Payments() : _itemTitle(""), _price(""), _orderId(""), _isPending(false), _isSuccess(false), _valid(false) {}
    Payments(const String &jsonPayload);

    bool isValid() const { return _valid; }
    bool isPending() const { return _isPending; }
    bool isSuccess() const { return _isSuccess; }
    String itemTitle() const { return _itemTitle; }
    String price() const { return _price; }
    String orderId() const { return _orderId; }
    String metadata() const { return _rawPayload; }

    void setPending(bool pending) { _isPending = pending; }
    void setSuccess(bool success) { _isSuccess = success; }
};

// Typed payload for usage flow (ur + le + nl)
class Usages
{
private:
    int _used;
    int _limit;
    int _remaining;
    int _percentage;
    String _resetAt;
    String _granularity;
    bool _isNearLimit;
    bool _isLimitExceeded;
    bool _valid;
    String _rawPayload;

public:
    Usages() : _used(0), _limit(0), _remaining(0), _percentage(0), _resetAt(""), _granularity(""), _isNearLimit(false), _isLimitExceeded(false), _valid(false) {}
    Usages(const String &jsonPayload);

    bool isValid() const { return _valid; }
    bool isNormal() const { return _valid && !_isNearLimit && !_isLimitExceeded; }
    bool isNearLimit() const { return _isNearLimit; }
    bool isLimitExceeded() const { return _isLimitExceeded; }
    int used() const { return _used; }
    int limit() const { return _limit; }
    int remaining() const { return _remaining; }
    int percentage() const { return _percentage; }
    String resetAt() const { return _resetAt; }
    String granularity() const { return _granularity; }
    String metadata() const { return _rawPayload; }

    void setNearLimit(bool near) { _isNearLimit = near; }
    void setLimitExceeded(bool exceeded) { _isLimitExceeded = exceeded; }
};

// Typed payload for device state (ds)
class DeviceStates
{
private:
    String _state;
    bool _valid;
    String _rawPayload;

public:
    DeviceStates() : _state(""), _valid(false) {}
    DeviceStates(const String &jsonPayload);

    bool isValid() const { return _valid; }
    String state() const { return _state; }
    String metadata() const { return _rawPayload; }

    bool isIdle() const { return _state == "idle"; }
    bool isPendingPayment() const { return _state == "pending_payment"; }
    bool isExpiredPayment() const { return _state == "expired_payment"; }
    bool isSuccessPayment() const { return _state == "success_payment"; }
    bool isMaintenance() const { return _state == "maintenance"; }
    bool isOnActiveService() const { return _state == "on_active_service"; }
};

// Typed payload for init response
class Inits
{
private:
    String _entitiesJson;
    String _merchantStatus;
    int _verificationFlag;
    bool _valid;
    String _rawPayload;

public:
    Inits() : _entitiesJson(""), _merchantStatus(""), _verificationFlag(0), _valid(false) {}
    Inits(const String &jsonPayload);

    bool isValid() const { return _valid; }
    String entities() const { return _entitiesJson; }
    String merchantStatus() const { return _merchantStatus; }
    int verificationFlag() const { return _verificationFlag; }
    String metadata() const { return _rawPayload; }

    bool isIdle() const { return _merchantStatus == "idle"; }
    bool isPendingPayment() const { return _merchantStatus == "pending_payment"; }
    bool isExpiredPayment() const { return _merchantStatus == "expired_payment"; }
    bool isSuccessPayment() const { return _merchantStatus == "success_payment"; }
    bool isMaintenance() const { return _merchantStatus == "maintenance"; }
    bool isOnActiveService() const { return _merchantStatus == "on_active_service"; }

    bool isPinEnabled() const { return _verificationFlag == 1 || _verificationFlag == 3; }
    bool isPreconditionEnabled() const { return _verificationFlag == 2 || _verificationFlag == 3; }
    bool isVerificationRequired() const { return _verificationFlag > 0; }
};

class EntityCommand
{
private:
    String _key;
    String _value;
    bool _valid;
    String _rawPayload;

public:
    EntityCommand() : _key(""), _value(""), _valid(false) {}
    EntityCommand(const String &key, const String &value)
        : _key(key), _value(value), _valid(true), _rawPayload(value) {}

    bool isValid() const { return _valid; }
    String key() const { return _key; }
    String value() const { return _value; }
    String metadata() const { return _rawPayload; }
};

typedef std::function<void(DeviceState)> StateCallbackFunction;
typedef std::function<void(Verifications &)> VerificationCallbackFunction;
typedef std::function<void(Payments &)> PaymentCallbackFunction;
typedef std::function<void(Usages &)> UsageCallbackFunction;
typedef std::function<void(DeviceStates &)> DeviceStateCallbackFunction;
typedef std::function<void(Inits &)> InitCallbackFunction;
typedef std::function<void(EntityCommand &)> EntityCommandCallbackFunction;

class Firmngin;

// Entity: Simple key wrapper for entity references
class Entity
{
private:
    String _key;

public:
    Entity(const char *key)
        : _key(key) {}
    Entity(const String &key)
        : _key(key) {}

    // Getters
    String key() { return _key; }
};

// Forward declaration for deferred registration
typedef std::pair<String, EntityCommandCallbackFunction> EntityRegEntry;
inline std::vector<EntityRegEntry> &deferredEntityRegistrations()
{
    static std::vector<EntityRegEntry> registrations;
    return registrations;
}

// Macro for global Entity registration — two forms:
// 1. ON_ENTITY(entityObject, callback)    — register callback for entity object
// 2. ON_ENTITY_S("key", callback)         — register callback for string key
#define FNGIN_CONCAT_IMPL(a, b) a##b
#define FNGIN_CONCAT(a, b) FNGIN_CONCAT_IMPL(a, b)

#define ON_ENTITY(entity, handler) \
    static const bool FNGIN_CONCAT(_fngin_entity_reg_, __LINE__) = ([]() {  \
        deferredEntityRegistrations().emplace_back(entity.key(), handler);   \
        return true; })();

#define ON_ENTITY_S(key, handler) \
    static const bool FNGIN_CONCAT(_fngin_entity_reg_, __LINE__) = ([]() {  \
        deferredEntityRegistrations().emplace_back(key, handler);            \
        return true; })();

// BatchState: Builder pattern for batch entity updates
class BatchState
{
private:
    Firmngin *_instance;
    JsonDocument _doc;
    JsonArray _array;

public:
    BatchState(Firmngin *instance) : _instance(instance)
    {
        _array = _doc.to<JsonArray>();
    }

    BatchState &add(String key, String value)
    {
        JsonObject obj = _array.add<JsonObject>();
        obj["key"] = key;
        obj["value"] = value;
        return *this;
    }

    // Overloads for convenience
    BatchState &add(const char *key, const char *value)
    {
        return add(String(key), String(value));
    }

    BatchState &add(String key, const char *value)
    {
        return add(key, String(value));
    }

    BatchState &add(int key, String value)
    {
        return add(String(key), value);
    }

    BatchState &add(int key, const char *value)
    {
        return add(String(key), String(value));
    }

    BatchState &add(String key, int value)
    {
        return add(key, String(value));
    }

    BatchState &add(int key, int value)
    {
        return add(String(key), String(value));
    }

    BatchState &add(String key, float value)
    {
        return add(key, String(value));
    }

    BatchState &add(int key, float value)
    {
        return add(String(key), String(value));
    }

    BatchState &add(String key, double value)
    {
        return add(key, String(value));
    }

    BatchState &add(int key, double value)
    {
        return add(String(key), String(value));
    }

    bool send();
    int count() { return _array.size(); }
    void clear() { _array.clear(); }
};

class Firmngin
{
public:
#if defined(ESP8266)
    Firmngin(const char *deviceId, const char *deviceKey);
    Firmngin(const char *deviceId, const char *deviceKey, const char *clientCert, const char *privateKey, const uint8_t *fingerprint);
#elif defined(ESP32)
    Firmngin(const char *deviceId, const char *deviceKey);
    Firmngin(const char *deviceId, const char *deviceKey, const char *caCert, const char *clientCert, const char *privateKey);
    Firmngin(const char *deviceId, const char *deviceKey, const uint8_t *fingerprint, const char *clientCert, const char *privateKey);
#else
    Firmngin(const char *deviceId, const char *deviceKey);
#endif
    ~Firmngin();

    void begin();
    void loop();
    void setDebug(bool debug);
    void setTimezone(int timezone);
    void setDaylightOffsetSec(int daylightOffsetSec);
    void setNtpServer(const char *ntpServer);
    void setClient(Client &client);
    void setMQTTServer(const char *server, int port);
    void setInsecure(bool insecure = true);
    bool isPlatformSupported();

    void on(const char *state, StateCallbackFunction callback);
    void on(DeviceStateType state, StateCallbackFunction callback);
    void on(DeviceStateType state, VerificationCallbackFunction callback);
    void on(DeviceStateType state, PaymentCallbackFunction callback);
    void on(DeviceStateType state, UsageCallbackFunction callback);
    void on(DeviceStateType state, DeviceStateCallbackFunction callback);
    void on(DeviceStateType state, InitCallbackFunction callback);
    void on(DeviceStateType state, EntityCommandCallbackFunction callback);
    void onEntity(const char *key, EntityCommandCallbackFunction callback);

    bool pushEntity(const char *key, const char *value);
    bool pushEntity(Entity &entity, const char *value);
    bool updateEntities(const char *jsonPayload);
    bool requestInit();
    BatchState pushBatchEntities();

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
    bool _insecure = false;

#if defined(ESP8266)
    const char *_clientCert = nullptr;
    const char *_privateKey = nullptr;
    const uint8_t *_fingerprint = nullptr;
    BearSSL::X509List *_clientCertList = nullptr;
    BearSSL::PrivateKey *_clientPrivKey = nullptr;
#elif defined(ESP32)
    const char *_caCert = nullptr;
    const char *_clientCert = nullptr;
    const char *_privateKey = nullptr;
    const uint8_t *_fingerprint = nullptr;
#endif

    std::map<String, StateCallbackFunction> _callbacks;
    VerificationCallbackFunction _verificationCallback;
    PaymentCallbackFunction _paymentsCallback;
    UsageCallbackFunction _usagesCallback;
    DeviceStateCallbackFunction _deviceStateCallback;
    InitCallbackFunction _initCallback;
    EntityCommandCallbackFunction _entityCallback;
    std::map<String, EntityCommandCallbackFunction> _entityCallbacks;

    void _Debug(String message, bool newLine = true);
    bool connectServer();
    void mqttCallback(char *topic, byte *payload, unsigned int length);
    String getPathPayment(String deviceId);
    String getPathDeviceStatus(String deviceId);
    String getPathPendingPayment(String deviceId);
    String getPathMetadataOnPending(String deviceId);
    String getPathMetadataOnExpired(String deviceId);
    String getPathMetadataOnSuccess(String deviceId);
    String getTopicInit(String deviceId);
    String getTopicDisplayPIN(String deviceId);
    String getTopicVerificationResult(String deviceId);
    String getTopicUsageResponse(String deviceId);
    String getTopicLimitExceeded(String deviceId);
    String getTopicNearLimit(String deviceId);
    String getTopicUpdateEntity(String deviceId);
    String getTopicUpdateEntities(String deviceId);
    String getTopicRequestInit(String deviceId);
    String getTopicEntityCommand(String deviceId);
    void syncTime();
    void setupLWT();
};

#endif // FIRMNGINKIT_H
