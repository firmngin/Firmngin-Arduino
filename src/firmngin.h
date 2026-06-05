#ifndef FIRMNGINKIT_H
#define FIRMNGINKIT_H

#include <Arduino.h>
#include "PubSubClient/PubSubClient.h"
#include "json.h"
#include <time.h>
#include <map>
#include <vector>
#include <functional>

#if defined(ESP32)
#include <mbedtls/gcm.h>
#elif defined(ESP8266)
#include <ChaChaPoly.h>
#endif

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <bearssl/bearssl_hash.h>
#include <LittleFS.h>
#define PLATFORM_SUPPORTED true
#define PLATFORM_NAME "ESP8266"
#define FIRMWARE_BUFFER_SIZE 4096
#elif defined(ESP32)
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <mbedtls/md.h>
#include <LittleFS.h>
#define PLATFORM_SUPPORTED true
#define PLATFORM_NAME "ESP32"
#define FIRMWARE_BUFFER_SIZE 8192
#else
#define PLATFORM_SUPPORTED false
#define PLATFORM_NAME "UNKNOWN"
#define FIRMWARE_BUFFER_SIZE 2048
#endif

// Persistent queue (LITTLEFS-backed circular buffer for offline publish retry)
#define FIRMNGIN_QUEUE_FILE_NAME "/firmngin_q.dat"
#define FIRMNGIN_QUEUE_MAGIC 0x464E5151U
#define FIRMNGIN_QUEUE_VERSION 1
#define FIRMNGIN_QUEUE_HEADER_SIZE 16
#define FIRMNGIN_QUEUE_RECORD_SIZE 256
#define FIRMNGIN_QUEUE_TOPIC_MAX 96
#define FIRMNGIN_QUEUE_PAYLOAD_MAX 140
#define FIRMNGIN_QUEUE_DEFAULT_CAPACITY 32
#define FIRMNGIN_QUEUE_MIN_CAPACITY 4
#define FIRMNGIN_QUEUE_MAX_CAPACITY 128
#define FIRMNGIN_QUEUE_DRAIN_INTERVAL_MS 5000UL

// User-managed MQTT stack (separate broker). Exposes vendored PubSubClient via mqttClient().
// Platform MQTT (_mqttClient) also uses PubSubClient — Firmngin is built on that dependency.
#ifndef FIRMNGIN_MQTT_CLIENT_DEFAULT_BUFFER_SIZE
#define FIRMNGIN_MQTT_CLIENT_DEFAULT_BUFFER_SIZE 512
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

#if !KEYS_H_AVAILABLE
#warning "keys.h not found — copy keys.h.template to your sketch folder and set DEVICE_ID / DEVICE_KEY"
#endif

#ifndef DEVICE_ID
#warning "DEVICE_ID not defined in keys.h"
#define DEVICE_ID "YOUR_DEVICE_ID"
#endif

#ifndef DEVICE_KEY
#warning "DEVICE_KEY not defined in keys.h"
#define DEVICE_KEY "YOUR_DEVICE_SECRET_KEY"
#endif

#ifndef FIRMNGIN_SERVER_ADDR
#define FIRMNGIN_SERVER_ADDR DEFAULT_MQTT_SERVER
#endif

#ifndef FIRMNGIN_SERVER_PORT
#define FIRMNGIN_SERVER_PORT DEFAULT_MQTT_PORT
#endif

#ifndef FIRMNGIN_FIRMWARE_VERSION
#define FIRMNGIN_FIRMWARE_VERSION "0.0.0"
#endif

#ifndef FIRMNGIN_FIRMWARE_TARGET_BOARD
#define FIRMNGIN_FIRMWARE_TARGET_BOARD PLATFORM_NAME
#endif

#ifndef FIRMNGIN_FIRMWARE_TARGET_MODEL
#define FIRMNGIN_FIRMWARE_TARGET_MODEL ""
#endif

#ifndef FIRMNGIN_OTA_BASE_URL
#define FIRMNGIN_OTA_BASE_URL "https://api.firmngin.dev/api/v1/ota"
#endif

#define OK "on_ok"
#define PATH_PAYMENT "pm"
#define PATH_DEVICE_STATUS "ds"
#define PATH_PENDING_PAYMENT "pp"
#define PATH_METADATA_ON_PENDING "mop"
#define PATH_METADATA_ON_EXPIRED "moe"
#define PATH_METADATA_ON_SUCCESS "mos"
#define PATH_PING "pi"
#define PATH_PONG "po"

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
    ENTITIES,
    ACTIVE_SESSION,
    ON_ACTIVE_SESSION = ACTIVE_SESSION
};

enum OTAAsyncState
{
    OTA_ASYNC_IDLE,
    OTA_ASYNC_DOWNLOADING,
    OTA_ASYNC_VERIFYING,
    OTA_ASYNC_INSTALLING,
    OTA_ASYNC_FAILED
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
    "verif",// VERIFICATIONS
    "pay",  // PAYMENTS
    "usg",  // USAGES
    "rs",   // ENTITIES
    "active_session", // ACTIVE_SESSION
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

// Typed payload for verification flow
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

// Typed payload for payment flow
class Payments
{
private:
    String _itemTitle;
    String _price;
    String _orderId;
    int _quantity;
    bool _isPending;
    bool _isSuccess;
    bool _valid;
    String _rawPayload;

public:
    Payments() : _itemTitle(""), _price(""), _orderId(""), _quantity(1), _isPending(false), _isSuccess(false), _valid(false) {}
    Payments(const String &jsonPayload);

    bool isValid() const { return _valid; }
    bool isPending() const { return _isPending; }
    bool isSuccess() const { return _isSuccess; }
    String itemTitle() const { return _itemTitle; }
    String price() const { return _price; }
    String orderId() const { return _orderId; }
    int quantity() const { return _quantity; }
    String metadata() const { return _rawPayload; }

    void setPending(bool pending) { _isPending = pending; }
    void setSuccess(bool success) { _isSuccess = success; }
};

// Typed payload for usage flow
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

// Typed payload for device state
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
    String _activeOrderId;
    int _verificationFlag;
    bool _valid;
    String _rawPayload;

public:
    Inits() : _entitiesJson(""), _merchantStatus(""), _activeOrderId(""), _verificationFlag(0), _valid(false) {}
    Inits(const String &jsonPayload);

    bool isValid() const { return _valid; }
    String entities() const { return _entitiesJson; }
    String merchantStatus() const { return _merchantStatus; }
    String activeOrderId() const { return _activeOrderId; }
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

class Firmngin;
class ActiveSession;

typedef std::function<void(DeviceState)> StateCallbackFunction;
typedef std::function<void(Verifications &)> VerificationCallbackFunction;
typedef std::function<void(Payments &)> PaymentCallbackFunction;
typedef std::function<void(Usages &)> UsageCallbackFunction;
typedef std::function<void(DeviceStates &)> DeviceStateCallbackFunction;
typedef std::function<void(Inits &)> InitCallbackFunction;
typedef std::function<void(EntityCommand &)> EntityCommandCallbackFunction;
typedef std::function<void(ActiveSession &)> ActiveSessionCallbackFunction;
typedef std::function<void(const char *status, const char *message)> OTACallbackFunction;

typedef std::pair<String, StateCallbackFunction> StateRegEntry;
typedef std::pair<String, EntityCommandCallbackFunction> EntityRegEntry;

class EntityValue
{
private:
    String _value;

public:
    EntityValue() : _value("") {}
    EntityValue(const String &value) : _value(value) {}

    String toString() const { return _value; }
    float toFloat() const { return _value.toFloat(); }
    int toInt() const { return _value.toInt(); }
    bool isOn() const { return _value == "1" || _value == "true" || _value == "on"; }
};

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

class ActiveSession
{
private:
    Firmngin *_instance;
    String _orderId;
    bool _active;
    bool _canRun;

public:
    ActiveSession(Firmngin *instance, const String &orderId, bool active, bool canRun)
        : _instance(instance), _orderId(orderId), _active(active), _canRun(canRun) {}

    bool isActive() const { return _active; }
    bool canRun() const { return _canRun; }
    String orderId() const { return _orderId; }
    EntityValue entity(const char *key) const;
    EntityValue entity(Entity &entity) const;
    bool endSession();
};

// Forward declaration for deferred registration
inline std::vector<StateRegEntry> &deferredStateRegistrations()
{
    static std::vector<StateRegEntry> registrations;
    return registrations;
}

inline std::vector<EntityRegEntry> &deferredEntityRegistrations()
{
    static std::vector<EntityRegEntry> registrations;
    return registrations;
}

inline std::vector<EntityCommandCallbackFunction> &deferredEntitiesRegistrations()
{
    static std::vector<EntityCommandCallbackFunction> registrations;
    return registrations;
}

inline std::vector<VerificationCallbackFunction> &deferredVerificationRegistrations()
{
    static std::vector<VerificationCallbackFunction> registrations;
    return registrations;
}

inline std::vector<PaymentCallbackFunction> &deferredPaymentRegistrations()
{
    static std::vector<PaymentCallbackFunction> registrations;
    return registrations;
}

inline std::vector<UsageCallbackFunction> &deferredUsageRegistrations()
{
    static std::vector<UsageCallbackFunction> registrations;
    return registrations;
}

inline std::vector<DeviceStateCallbackFunction> &deferredDeviceStatusRegistrations()
{
    static std::vector<DeviceStateCallbackFunction> registrations;
    return registrations;
}

inline std::vector<InitCallbackFunction> &deferredInitRegistrations()
{
    static std::vector<InitCallbackFunction> registrations;
    return registrations;
}

inline std::vector<ActiveSessionCallbackFunction> &deferredActiveSessionRegistrations()
{
    static std::vector<ActiveSessionCallbackFunction> registrations;
    return registrations;
}

inline std::vector<OTACallbackFunction> &deferredOTAStatusRegistrations()
{
    static std::vector<OTACallbackFunction> registrations;
    return registrations;
}

// Macro for global Entity registration: two forms:
// 1. ON_ENTITY(entityObject, callback)    : register callback for entity object
// 2. ON_ENTITY_S("key", callback)         : register callback for string key
#define FNGIN_CONCAT_IMPL(a, b) a##b
#define FNGIN_CONCAT(a, b) FNGIN_CONCAT_IMPL(a, b)

#define FNGIN_ON_STATE(stateName, statePath, stateVar) \
    static void FNGIN_CONCAT(_fngin_state_handler_, __LINE__)(DeviceState stateVar); \
    static const bool FNGIN_CONCAT(_fngin_state_reg_, __LINE__) = ([]() { \
        deferredStateRegistrations().emplace_back(statePath, FNGIN_CONCAT(_fngin_state_handler_, __LINE__)); \
        return true; })(); \
    static void FNGIN_CONCAT(_fngin_state_handler_, __LINE__)(DeviceState stateVar)

#define ON_PAYMENT(stateVar) FNGIN_ON_STATE(PAYMENT, "pm", stateVar)
#define ON_PENDING_PAYMENT(stateVar) FNGIN_ON_STATE(PENDING_PAYMENT, "pp", stateVar)
#define ON_METADATA_ON_PENDING(stateVar) FNGIN_ON_STATE(METADATA_ON_PENDING, "mop", stateVar)
#define ON_METADATA_ON_EXPIRED(stateVar) FNGIN_ON_STATE(METADATA_ON_EXPIRED, "moe", stateVar)
#define ON_METADATA_ON_SUCCESS(stateVar) FNGIN_ON_STATE(METADATA_ON_SUCCESS, "mos", stateVar)
#define ON_DISPLAY_PIN(stateVar) FNGIN_ON_STATE(DISPLAY_PIN, "dpin", stateVar)
#define ON_VERIFICATION_RESULT(stateVar) FNGIN_ON_STATE(VERIFICATION_RESULT, "vr", stateVar)
#define ON_USAGE_RESPONSE(stateVar) FNGIN_ON_STATE(USAGE_RESPONSE, "ur", stateVar)
#define ON_LIMIT_EXCEEDED(stateVar) FNGIN_ON_STATE(LIMIT_EXCEEDED, "le", stateVar)
#define ON_NEAR_LIMIT(stateVar) FNGIN_ON_STATE(NEAR_LIMIT, "nl", stateVar)
#define ON_DEVICE_STATUS_RAW(stateVar) FNGIN_ON_STATE(DEVICE_STATUS, "ds", stateVar)
#define ON_INIT_RAW(stateVar) FNGIN_ON_STATE(INIT, "init", stateVar)
#define ON_ENTITY(entity, handler) \
    static const bool FNGIN_CONCAT(_fngin_entity_reg_, __LINE__) = ([]() {  \
        deferredEntityRegistrations().emplace_back(entity.key(), handler);   \
        return true; })();

#define ON_ENTITY_S(key, handler) \
    static const bool FNGIN_CONCAT(_fngin_entity_reg_, __LINE__) = ([]() {  \
        deferredEntityRegistrations().emplace_back(key, handler);            \
        return true; })();

#define ON_ENTITIES(entityCommandVar) \
    static void FNGIN_CONCAT(_fngin_entities_handler_, __LINE__)(EntityCommand &entityCommandVar); \
    static const bool FNGIN_CONCAT(_fngin_entities_reg_, __LINE__) = ([]() { \
        deferredEntitiesRegistrations().push_back(FNGIN_CONCAT(_fngin_entities_handler_, __LINE__)); \
        return true; })(); \
    static void FNGIN_CONCAT(_fngin_entities_handler_, __LINE__)(EntityCommand &entityCommandVar)

#define ON_VERIFICATIONS(verificationVar) \
    static void FNGIN_CONCAT(_fngin_verification_handler_, __LINE__)(Verifications &verificationVar); \
    static const bool FNGIN_CONCAT(_fngin_verification_reg_, __LINE__) = ([]() { \
        deferredVerificationRegistrations().push_back(FNGIN_CONCAT(_fngin_verification_handler_, __LINE__)); \
        return true; })(); \
    static void FNGIN_CONCAT(_fngin_verification_handler_, __LINE__)(Verifications &verificationVar)

#define ON_PAYMENTS(paymentVar) \
    static void FNGIN_CONCAT(_fngin_payment_handler_, __LINE__)(Payments &paymentVar); \
    static const bool FNGIN_CONCAT(_fngin_payment_reg_, __LINE__) = ([]() { \
        deferredPaymentRegistrations().push_back(FNGIN_CONCAT(_fngin_payment_handler_, __LINE__)); \
        return true; })(); \
    static void FNGIN_CONCAT(_fngin_payment_handler_, __LINE__)(Payments &paymentVar)

#define ON_USAGES(usageVar) \
    static void FNGIN_CONCAT(_fngin_usage_handler_, __LINE__)(Usages &usageVar); \
    static const bool FNGIN_CONCAT(_fngin_usage_reg_, __LINE__) = ([]() { \
        deferredUsageRegistrations().push_back(FNGIN_CONCAT(_fngin_usage_handler_, __LINE__)); \
        return true; })(); \
    static void FNGIN_CONCAT(_fngin_usage_handler_, __LINE__)(Usages &usageVar)

#define ON_DEVICE_STATUS(deviceStatusVar) \
    static void FNGIN_CONCAT(_fngin_device_status_handler_, __LINE__)(DeviceStates &deviceStatusVar); \
    static const bool FNGIN_CONCAT(_fngin_device_status_reg_, __LINE__) = ([]() { \
        deferredDeviceStatusRegistrations().push_back(FNGIN_CONCAT(_fngin_device_status_handler_, __LINE__)); \
        return true; })(); \
    static void FNGIN_CONCAT(_fngin_device_status_handler_, __LINE__)(DeviceStates &deviceStatusVar)

#define ON_INIT(initVar) \
    static void FNGIN_CONCAT(_fngin_init_handler_, __LINE__)(Inits &initVar); \
    static const bool FNGIN_CONCAT(_fngin_init_reg_, __LINE__) = ([]() { \
        deferredInitRegistrations().push_back(FNGIN_CONCAT(_fngin_init_handler_, __LINE__)); \
        return true; })(); \
    static void FNGIN_CONCAT(_fngin_init_handler_, __LINE__)(Inits &initVar)

#define ON_ACTIVE_SESSION(sessionVar) \
    static void FNGIN_CONCAT(_fngin_active_session_handler_, __LINE__)(ActiveSession &sessionVar); \
    static const bool FNGIN_CONCAT(_fngin_active_session_reg_, __LINE__) = ([]() { \
        deferredActiveSessionRegistrations().push_back(FNGIN_CONCAT(_fngin_active_session_handler_, __LINE__)); \
        return true; })(); \
    static void FNGIN_CONCAT(_fngin_active_session_handler_, __LINE__)(ActiveSession &sessionVar)

#define ON_OTA_STATUS(statusVar, messageVar) \
    static void FNGIN_CONCAT(_fngin_ota_status_handler_, __LINE__)(const char *statusVar, const char *messageVar); \
    static const bool FNGIN_CONCAT(_fngin_ota_status_reg_, __LINE__) = ([]() { \
        deferredOTAStatusRegistrations().push_back(FNGIN_CONCAT(_fngin_ota_status_handler_, __LINE__)); \
        return true; })(); \
    static void FNGIN_CONCAT(_fngin_ota_status_handler_, __LINE__)(const char *statusVar, const char *messageVar)

// BatchState: Builder pattern for batch entity updates
#define FIRMNGIN_BATCH_BUFFER_SIZE 1024
#define FIRMNGIN_E2EE_BUFFER_SIZE (FIRMNGIN_BATCH_BUFFER_SIZE + 32)

class BatchState
{
private:
    Firmngin *_instance;
    char _buffer[FIRMNGIN_BATCH_BUFFER_SIZE];
    firmngin_json::ArrayBuilder _builder;

public:
    BatchState(Firmngin *instance) : _instance(instance), _builder(_buffer, sizeof(_buffer))
    {
        _builder.reset();
    }

    BatchState &add(String key, String value)
    {
        _builder.add(key.c_str(), value.c_str());
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
    int count() { return _builder.count(); }
    void clear() { _builder.clear(); }
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

    PubSubClient &mqttClient();
    Client &mqttTransport();
    void mqttClientPrepare();
    void mqttClientSetBufferSize(uint16_t size);
    void mqttClientSetInsecure(bool insecure = true);
    void mqttClientLoop();
    void mqttClientDisconnect();

    // Persistent offline publish queue (LITTLEFS-backed)
    void setQueueEnabled(bool enabled = true);
    void setMaxQueueEntries(uint16_t maxEntries);
    uint16_t getQueueSize() const;
    void clearQueue();

    void on(const char *state, StateCallbackFunction callback);
    void on(DeviceStateType state, StateCallbackFunction callback);
    void on(DeviceStateType state, VerificationCallbackFunction callback);
    void on(DeviceStateType state, PaymentCallbackFunction callback);
    void on(DeviceStateType state, UsageCallbackFunction callback);
    void on(DeviceStateType state, DeviceStateCallbackFunction callback);
    void on(DeviceStateType state, InitCallbackFunction callback);
    void on(DeviceStateType state, EntityCommandCallbackFunction callback);
    void on(DeviceStateType state, ActiveSessionCallbackFunction callback);
    void onEntity(const char *key, EntityCommandCallbackFunction callback);

    bool pushEntity(const char *key, const char *value);
    bool pushEntity(Entity &entity, const char *value);
    bool updateEntities(const char *jsonPayload);
    EntityValue entity(const char *key);
    EntityValue entity(Entity &entity);
    bool endSession();
    bool requestInit();
    BatchState pushBatchEntities();
    void setFirmwareInfo(const char *version, const char *targetBoard, const char *targetModel);
    void setFirmwareInfo(const char *version);
    const char *getFirmwareVersion();
    const char *getFirmwareTargetBoard();
    const char *getFirmwareTargetModel();
    bool syncFirmwareInfo();
    void setOTABaseURL(const char *baseURL);
    void setEnableOTA(bool enabled = true);
    void enableOTA(bool enabled = true);
    bool checkOTA();
    bool performOTA(const char *manifestUrl = nullptr);
    void onOTAStatus(OTACallbackFunction callback);

    // Camera image upload
    typedef void (*UploadCallbackFunction)(const char *response);
    typedef void (*UploadErrorCallbackFunction)(int code, const char *message);
    bool uploadImage(const char *entityKey, uint8_t *data, size_t len, const char *contentType,
                     UploadCallbackFunction onSuccess = nullptr,
                     UploadErrorCallbackFunction onError = nullptr);

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

#if defined(ESP8266) || defined(ESP32)
    WiFiClientSecure _userMqttTransport;
#else
    WiFiClient _userMqttTransport;
#endif
    PubSubClient _userMqttClient;
    uint16_t _userMqttBufferSize = FIRMNGIN_MQTT_CLIENT_DEFAULT_BUFFER_SIZE;
    bool _userMqttInsecure = true;
    bool _userMqttPrepared = false;

    unsigned long _delayRetryMQTT = 5000;
    int maxRetryMQTT = 3;
    int defaultQos = 1;

    String _mqttServer;
    int _mqttPort;
    bool _insecure = false;
    bool _e2eeEnabled = false;
    uint8_t _e2eeKeyBytes[32] = {0};
    uint8_t _e2eeKeyLen = 0;
    String _firmwareVersion = FIRMNGIN_FIRMWARE_VERSION;
    String _firmwareTargetBoard = FIRMNGIN_FIRMWARE_TARGET_BOARD;
    String _firmwareTargetModel = FIRMNGIN_FIRMWARE_TARGET_MODEL;
    String _lastSyncedFirmwareVersion;
    String _topicUpdateEntity;
    String _topicUpdateEntities;
    unsigned long _ntpSyncStartMs = 0;
    bool _ntpSynced = false;

    // Persistent offline publish queue state (LITTLEFS circular buffer)
    bool _queueEnabled = false;
    uint16_t _queueCapacity = FIRMNGIN_QUEUE_DEFAULT_CAPACITY;
    uint16_t _queueHead = 0;
    uint16_t _queueTail = 0;
    uint16_t _queueCount = 0;
    bool _queueFileReady = false;
    unsigned long _lastQueueDrainMs = 0;
    bool _otaEnabled = true;
    String _otaBaseUrl = FIRMNGIN_OTA_BASE_URL;
    String _otaFirmwareID;
    String _otaFirmwareSHA256;

    OTAAsyncState _otaAsyncState = OTA_ASYNC_IDLE;
    HTTPClient _otaHttp;
    int _otaContentLength = 0;
    int _otaDownloaded = 0;
    int _otaLastProgressBucket = -1;
    int _otaLastPublishedProgressBucket = -1;
    unsigned long _otaLastDebugAt = 0;
    uint8_t *_otaBuffer = nullptr;
#if defined(ESP32)
    WiFiClientSecure _otaWifiClient;
    mbedtls_md_context_t _otaSha256Ctx;
#elif defined(ESP8266)
    BearSSL::WiFiClientSecure _otaWifiClient;
    br_sha256_context _otaSha256Ctx;
#endif

    bool otaHTTPGet(const char *path, const char *queryParams, String &responseBody);

#if defined(ESP8266)
    const char *_clientCert = nullptr;
    const char *_privateKey = nullptr;
    const uint8_t *_fingerprint = nullptr;
    BearSSL::X509List *_clientCertList = nullptr;
    BearSSL::PrivateKey *_clientPrivKey = nullptr;
    BearSSL::X509List *_trustAnchors = nullptr;
    BearSSL::X509List *_otaTrustAnchors = nullptr;
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
    std::vector<VerificationCallbackFunction> _verificationCallbacks;
    std::vector<PaymentCallbackFunction> _paymentCallbacks;
    std::vector<UsageCallbackFunction> _usageCallbacks;
    std::vector<DeviceStateCallbackFunction> _deviceStateCallbacks;
    std::vector<InitCallbackFunction> _initCallbacks;
    std::map<String, EntityCommandCallbackFunction> _entityCallbacks;
    std::vector<EntityCommandCallbackFunction> _entityGlobalCallbacks;
    OTACallbackFunction _otaCallback;
    std::vector<OTACallbackFunction> _otaCallbacks;
    std::vector<ActiveSessionCallbackFunction> _activeSessionCallbacks;
    std::map<String, String> _localEntityValues;
    String _currentOrderId;
    String _merchantStatus;
    bool _sessionEndRequested = false;
    unsigned long _lastSessionEndAtMs = 0;
    bool _deferredCallbacksLoaded = false;
    std::vector<StateRegEntry> _stateCallbacks;

    void _Debug(String message, bool newLine = true);
    bool connectServer();
    void mqttCallback(char *path, byte *payload, unsigned int length);
    String getPathPayment(String deviceId);
    String getPathDeviceStatus(String deviceId);
    String getPathPendingPayment(String deviceId);
    String getPathMetadataOnPending(String deviceId);
    String getPathMetadataOnExpired(String deviceId);
    String getPathMetadataOnSuccess(String deviceId);
    String getPathInit(String deviceId);
    String getPathDisplayPIN(String deviceId);
    String getPathVerificationResult(String deviceId);
    String getPathUsageResponse(String deviceId);
    String getPathLimitExceeded(String deviceId);
    String getPathNearLimit(String deviceId);
    String getPathUpdateEntity(String deviceId);
    String getPathUpdateEntities(String deviceId);
    String getPathRequestInit(String deviceId);
    String getPathEntityCommand(String deviceId);
    String getPathSessionEnd(String deviceId);
    String getPathPing(String deviceId);
    String getOTATriggerPath(String deviceId);
    void syncTime();
    void setupLWT();
    bool publishPayload(const char *path, const char *payload, bool retained = false);
    bool publishOTAStatus(const char *status, const char *message);
    int normalizeOTAProgress(const char *status, const char *message);

    // Persistent queue helpers
    bool _initQueue();
    void _shutdownQueue();
    bool _enqueueToQueue(const char *topic, const uint8_t *payload, size_t payloadLen, bool retained);
    bool _peekQueueHead(String &topic, String &payload, bool &retained);
    void _dropQueueHead();
    void _drainQueue();
    bool _readQueueHeader(uint16_t &head, uint16_t &tail, uint16_t &count);
    bool _writeQueueHeader(uint16_t head, uint16_t tail, uint16_t count);
    void _resetQueueFile();
    void _processOTA();
    void _otaCleanup();
    void runActiveSessionHandlers();
    void setMerchantStatus(const String &status);
    void setCurrentOrder(const String &orderId);
    void clearCurrentSession();
};

#endif // FIRMNGINKIT_H
