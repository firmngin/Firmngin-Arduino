#if !defined(FIRMNGINKIT_H)
#define FIRMNGINKIT_H

#include <Arduino.h>
#if __has_include("firmngin_version.h")
#include "firmngin_version.h"
#endif

#if !defined(FIRMNGIN_VERSION)
#define FIRMNGIN_VERSION "0.0.0"
#endif
#include "json.h"
#include "ota_rollback.h"
#include "audio_stream_protocol.h"
#include "firmngin_i2s.h"
#include "md5.h"
#include <time.h>
#include <map>
#include <vector>
#include <functional>

#if defined(ESP32)
#include <Preferences.h>
#endif

#if defined(ESP32)
#include <mbedtls/gcm.h>
#endif

#if defined(ESP32)
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <mbedtls/md.h>
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/stream_buffer.h>
#include <mqtt_client.h>
#if ESP_ARDUINO_VERSION_MAJOR >= 3
#include "esp_websocket_client_vendored.h" // vendored: core 3.x stopped bundling this IDF component
#else
#include <esp_websocket_client.h> // bundled with Arduino-ESP32 2.0.x
#endif
// The chunk callback hands the sketch I2S-ready PCM, so the matching I2S driver
// header comes from the library: core 3.x moved to the std driver, 2.0.x keeps the
// legacy one. The sketch only configures pins and writes frames.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
#include <driver/i2s_std.h>
#else
#include <driver/i2s.h>
#endif
#define PLATFORM_SUPPORTED true
#define PLATFORM_NAME "ESP32"
#define FIRMWARE_BUFFER_SIZE 8192
#else
#define PLATFORM_SUPPORTED false
#define PLATFORM_NAME "UNKNOWN"
#define FIRMWARE_BUFFER_SIZE 2048
#endif

// Persistent queue (LITTLEFS-backed circular buffer for offline message retry)
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

// Second server connection, same bundled client as the platform one.
#if !defined(FIRMNGIN_MQTT_CLIENT_DEFAULT_BUFFER_SIZE)
#define FIRMNGIN_MQTT_CLIENT_DEFAULT_BUFFER_SIZE 1024
#endif

typedef std::function<void(const char *topic, const uint8_t *payload, unsigned int length)> MqttMessageHandler;

// Message Error Codes
#define FMSG_ERR_NONE 0
#define FMSG_ERR_QUEUE_FULL 1001
#define FMSG_ERR_CONN_REFUSED 1002
#define FMSG_ERR_CONN_TIMEOUT 1003
#define FMSG_ERR_TLS_HANDSHAKE 1004
#define FMSG_ERR_TLS_CERT_VERIFY 1005
#define FMSG_ERR_TCP_CONNECT 1006
#define FMSG_ERR_DNS_LOOKUP 1007
#define FMSG_ERR_MESSAGE_TOO_LARGE 1008
#define FMSG_ERR_BUFFER_OVERFLOW 1009
#define FMSG_ERR_MUTEX_TIMEOUT 1010
#define FMSG_ERR_UNKNOWN 1999

// Default Server Configuration
#define DEFAULT_MQTT_SERVER "asia-jkt1.firmngin.dev"
#define DEFAULT_MQTT_PORT 58884

// Check if keys.h exists for mTLS support
#if __has_include("keys.h")
#define KEYS_H_AVAILABLE 1
#include "keys.h"
#else
#define KEYS_H_AVAILABLE 0
#endif

#if !KEYS_H_AVAILABLE && !(defined(FIRMNGIN_FACTORY_SERIAL) && (FIRMNGIN_FACTORY_SERIAL != 0))
#warning "keys.h not found — copy keys.h.template to your sketch folder and set DEVICE_ID / DEVICE_KEY"
#endif

#if !defined(DEVICE_ID)
#if !(defined(FIRMNGIN_FACTORY_SERIAL) && (FIRMNGIN_FACTORY_SERIAL != 0))
#warning "DEVICE_ID not defined in keys.h"
#endif
#define DEVICE_ID "YOUR_DEVICE_ID"
#endif

#if !defined(DEVICE_KEY)
#if !(defined(FIRMNGIN_FACTORY_SERIAL) && (FIRMNGIN_FACTORY_SERIAL != 0))
#warning "DEVICE_KEY not defined in keys.h"
#endif
#define DEVICE_KEY "YOUR_DEVICE_SECRET_KEY"
#endif

#if !defined(FIRMNGIN_BROKER_ADDR)
#if defined(FIRMNGIN_SERVER_ADDR)
#define FIRMNGIN_BROKER_ADDR FIRMNGIN_SERVER_ADDR
#else
#define FIRMNGIN_BROKER_ADDR DEFAULT_MQTT_SERVER
#endif
#endif

#if !defined(FIRMNGIN_BROKER_PORT)
#if defined(FIRMNGIN_SERVER_PORT)
#define FIRMNGIN_BROKER_PORT FIRMNGIN_SERVER_PORT
#else
#define FIRMNGIN_BROKER_PORT DEFAULT_MQTT_PORT
#endif
#endif

#if !defined(FIRMNGIN_SERVER_ADDR)
#define FIRMNGIN_SERVER_ADDR FIRMNGIN_BROKER_ADDR
#endif

#if !defined(FIRMNGIN_SERVER_PORT)
#define FIRMNGIN_SERVER_PORT FIRMNGIN_BROKER_PORT
#endif

#if !defined(FIRMNGIN_DEV_NO_TLS)
#define FIRMNGIN_DEV_NO_TLS 0
#endif

#if !defined(FIRMNGIN_NO_TLS)
#define FIRMNGIN_NO_TLS FIRMNGIN_DEV_NO_TLS
#endif

#if !defined(FIRMNGIN_FIRMWARE_VERSION)
#define FIRMNGIN_FIRMWARE_VERSION "0.0.0"
#endif

#if !defined(FIRMNGIN_FIRMWARE_TARGET_BOARD)
#define FIRMNGIN_FIRMWARE_TARGET_BOARD PLATFORM_NAME
#endif

#if !defined(FIRMNGIN_FIRMWARE_TARGET_MODEL)
#define FIRMNGIN_FIRMWARE_TARGET_MODEL ""
#endif

#if !defined(FIRMNGIN_API_BASE_URL)
#define FIRMNGIN_API_BASE_URL "https://ota.firmngin.dev/api/v1"
#endif

#if !defined(FIRMNGIN_OTA_BASE_URL)
#define FIRMNGIN_OTA_BASE_URL "https://ota.firmngin.dev/api/v1/ota"
#endif

#if !defined(FIRMNGIN_AUDIO_STREAM_BASE_URL)
#define FIRMNGIN_AUDIO_STREAM_BASE_URL "https://astream.firmngin.dev"
#endif

#define OK "on_ok"
#define PATH_PAYMENT "pm"
#define PATH_DEVICE_STATUS "ds"
#define PATH_PENDING_PAYMENT "pp"
#define PATH_POSTPAID_READY "pr"
#define PATH_DISPENSE "dp"
#define PATH_METADATA_ON_PENDING "mop"
#define PATH_METADATA_POSTPAID_READY "mpp"
#define PATH_METADATA_ON_ACTIVE_SERVICE "moa"
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
    POSTPAID_READY,
    METADATA_ON_PENDING,
    METADATA_POSTPAID_READY,
    METADATA_ON_ACTIVE_SERVICE,
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
    DISPENSES,
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

// Terminal QR wire abbreviations: st=status, amt=amount, od=order id (UUID),
// oc=order code, qr=raw QRIS content, exp=expiry; rid=request id (device
// request only).
// Raw QRIS content and payment lifecycle. Rendering belongs to the device UI.
class TerminalQR
{
public:
    String orderId;
    String orderCode;
    String status;
    String amount;
    String qrString;
    String expiresAt;

    explicit TerminalQR(const String &payload);
    bool isValid() const { return orderCode.length() > 0 || status.length() > 0; }
    bool isPending() const { return status == "pending"; }
    bool isPaid() const { return status == "paid"; }
    bool isIdle() const { return status == "idle"; }
    // A failure code arrives in status; there is no separate error field.
    bool hasError() const { return status.length() > 0 && status != "pending" && status != "paid" && status != "expired" && status != "canceled" && status != "idle"; }
    // A request rejection carries no od, so it stays a hasError() on the general
    // QR callback and never reports an ended transaction.
    bool isFailed() const { return orderCode.length() > 0 && (status == "expired" || status == "canceled" || hasError()); }
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
    bool _isPostPaid;
    bool _isPrePaid;
    bool _valid;
    String _rawPayload;

public:
    Payments() : _itemTitle(""), _price(""), _orderId(""), _quantity(1), _isPending(false), _isSuccess(false), _isPostPaid(false), _isPrePaid(false), _valid(false) {}
    Payments(const String &jsonPayload);

    bool isValid() const { return _valid; }
    bool isPending() const { return _isPending; }
    bool isSuccess() const { return _isSuccess; }
    bool isPostPaid() const { return _isPostPaid; }
    bool isPrePaid() const { return _isPrePaid; }
    String itemTitle() const { return _itemTitle; }
    String price() const { return _price; }
    String orderId() const { return _orderId; }
    int quantity() const { return _quantity; }
    String metadata() const { return _rawPayload; }

    void setPending(bool pending) { _isPending = pending; }
    void setSuccess(bool success) { _isSuccess = success; }
    void setPaymentType(bool postPaid) { _isPostPaid = postPaid; _isPrePaid = !postPaid; }
};

#if !defined(FIRMNGIN_DISPENSE_MAX_ITEMS)
#define FIRMNGIN_DISPENSE_MAX_ITEMS 20
#endif

// Typed payload for vending dispense: root JSON array of SKU strings
class Dispenses
{
private:
    String _skus[FIRMNGIN_DISPENSE_MAX_ITEMS];
    int _count;
    bool _valid;
    String _rawPayload;

public:
    Dispenses() : _count(0), _valid(false) {}
    Dispenses(const String &jsonPayload);

    bool isValid() const { return _valid; }
    int itemCount() const { return _count; }
    String skuAt(int index) const
    {
        if (index < 0 || index >= _count)
            return "";
        return _skus[index];
    }
    String metadata() const { return _rawPayload; }
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
    // The audio_output entity's value: this device's stream endpoint, when the device
    // has one.
    String _audioStreamUrl;
    int _verificationFlag;
    int _modeFlag;
    int _kioskFlag;
    int _deviceLocalEndFlag;
    bool _valid;
    String _rawPayload;

public:
    Inits() : _entitiesJson(""), _merchantStatus(""), _activeOrderId(""), _verificationFlag(0), _modeFlag(1), _kioskFlag(0), _deviceLocalEndFlag(0), _valid(false) {}
    Inits(const String &jsonPayload);

    bool isValid() const { return _valid; }
    String entities() const { return _entitiesJson; }
    String merchantStatus() const { return _merchantStatus; }
    String activeOrderId() const { return _activeOrderId; }
    // wss:// endpoint from the t=6 (audio_output) entity, empty when the device has
    // none. The server owns the host and path prefix, so this beats the derived URL.
    String audioStreamUrl() const { return _audioStreamUrl; }
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
    bool isServiceMode() const { return _modeFlag == 1; }
    bool isVendingMode() const { return _modeFlag == 2; }
    bool isTerminalMode() const { return _modeFlag == 3; }
    bool isKiosk() const { return _kioskFlag == 1; }
    bool isDeviceLocalEndEnabled() const { return _kioskFlag == 1 && _deviceLocalEndFlag == 1; }
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
typedef std::function<void(Dispenses &)> DispenseCallbackFunction;
typedef std::function<void(TerminalQR &)> TerminalQRCallbackFunction;
typedef std::function<void(Usages &)> UsageCallbackFunction;
typedef std::function<void(DeviceStates &)> DeviceStateCallbackFunction;
typedef std::function<void(Inits &)> InitCallbackFunction;
typedef std::function<void(EntityCommand &)> EntityCommandCallbackFunction;
typedef std::function<void(ActiveSession &)> ActiveSessionCallbackFunction;
typedef std::function<void(const char *status, const char *message)> OTACallbackFunction;

// Audio playback model (ESP32):
//   WebSocket task → reassembles 640 B mono frames → ring buffer (non-blocking
//                    enqueue: if the ring is full one frame is dropped so this
//                    task can keep reading — otherwise audio.flush on stop never
//                    arrives and the gateway tears down the socket)
//   loop()         → drains the ring into I2S. Call loop() often.
//   Clean audio.end → drain queued samples and fade the final frame to silence.
//   flush / disconnect → bump generation, reset ring, fade the short I2S DMA tail.
#define FIRMNGIN_AUDIO_BUFFER_FRAMES 48
#define FIRMNGIN_AUDIO_FRAMES_PER_LOOP 8
#define FIRMNGIN_AUDIO_TAIL_SILENCE_FRAMES 2
#define FIRMNGIN_AUDIO_EVENT_QUEUE_LEN 8
#define FIRMNGIN_AUDIO_WS_BUFFER_BYTES 1024
#define FIRMNGIN_AUDIO_WS_TASK_STACK 4096
#define FIRMNGIN_AUDIO_INPUT_TASK_STACK 6144
#define FIRMNGIN_AUDIO_INPUT_MAX_MS 30000UL
// How long openAudioStream waits for the first NTP sync (started by begin()) before
// signing the stream connection fails with a clear error.
#define FIRMNGIN_NTP_SYNC_TIMEOUT_MS 15000
// Longest stream URL an init entity may carry in its value.
#define FIRMNGIN_AUDIO_STREAM_URL_MAX 160

struct FirmnginAudioConfig
{
    // nullptr / empty URL derives this device's endpoint from provisioning. A nullptr
    // token uses the provisioned token; an empty token explicitly sends no bearer.
    const char *url = nullptr;
    const char *token = nullptr;

    // Ring capacity in 20 ms mono frames (~960 ms at 48).
    uint8_t bufferFrames = FIRMNGIN_AUDIO_BUFFER_FRAMES;
    // Max frames drained per loop() pass. Raise if loop() is called infrequently.
    uint8_t framesPerLoop = FIRMNGIN_AUDIO_FRAMES_PER_LOOP;
    uint16_t websocketBufferBytes = FIRMNGIN_AUDIO_WS_BUFFER_BYTES;
    uint16_t websocketTaskStack = FIRMNGIN_AUDIO_WS_TASK_STACK;

    // Deprecated no-op kept so existing sketches compile.
    uint16_t backpressureMs = 0;
};

// Receives one I2S-ready 16 kHz signed-16 stereo frame (L/R interleaved, 20 ms,
// AUDIO_PLAYBACK_FRAME_BYTES). The wire stays mono; Firmngin duplicates each
// sample internally to avoid ESP32's 16-bit mono TX sample reordering.
typedef std::function<bool(const uint8_t *data, size_t len)> AudioChunkCallbackFunction;
typedef std::function<void(FirmnginAudioEvent event, const char *detail)> AudioStreamEventCallbackFunction;

// One ON_AUDIO registration: server entity key -> stream event handler.
typedef std::pair<String, AudioStreamEventCallbackFunction> AudioEntityRegEntry;

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
    Entity(int key)
        : _key(String(key)) {}

    // Getters
    const String &key() const { return _key; }
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
    EntityValue entity(const Entity &entity) const;
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

inline std::vector<AudioEntityRegEntry> &deferredAudioEntityRegistrations()
{
    static std::vector<AudioEntityRegEntry> registrations;
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

inline std::vector<DispenseCallbackFunction> &deferredDispenseRegistrations()
{
    static std::vector<DispenseCallbackFunction> registrations;
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
#define ON_METADATA_POSTPAID_READY(stateVar) FNGIN_ON_STATE(METADATA_POSTPAID_READY, "mpp", stateVar)
#define ON_METADATA_ON_ACTIVE_SERVICE(stateVar) FNGIN_ON_STATE(METADATA_ON_ACTIVE_SERVICE, "moa", stateVar)
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

// Register an entity as this device's audio output. The handler gets the stream event
// and the entity_key the clip targets. Binding arms the stream (connect + standby) like
// a registered setAudioInterface(entity, chip).
#define ON_AUDIO(entity, handler) \
    static const bool FNGIN_CONCAT(_fngin_audio_reg_, __LINE__) = ([]() {  \
        deferredAudioEntityRegistrations().emplace_back(entity.key(), handler); \
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

#define ON_DISPENSES(dispenseVar) \
    static void FNGIN_CONCAT(_fngin_dispense_handler_, __LINE__)(Dispenses &dispenseVar); \
    static const bool FNGIN_CONCAT(_fngin_dispense_reg_, __LINE__) = ([]() { \
        deferredDispenseRegistrations().push_back(FNGIN_CONCAT(_fngin_dispense_handler_, __LINE__)); \
        return true; })(); \
    static void FNGIN_CONCAT(_fngin_dispense_handler_, __LINE__)(Dispenses &dispenseVar)

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
#define FIRMNGIN_E2EE_BUFFER_SIZE (FIRMNGIN_BATCH_BUFFER_SIZE + 64)

#define FIRMNGIN_ENTITY_KEY_LAT "lat"
#define FIRMNGIN_ENTITY_KEY_LON "lon"
#define FIRMNGIN_ENTITY_KEY_ACCURACY "accuracy"
#define FIRMNGIN_ENTITY_KEY_ALT "alt"
#define FIRMNGIN_ENTITY_KEY_SPEED "speed"

class LocationUpdate
{
private:
    Firmngin *_instance;
    char _buffer[FIRMNGIN_BATCH_BUFFER_SIZE];
    firmngin_json::ArrayBuilder _builder;
    double _lat;
    double _lon;
    float _accuracy;
    float _alt;
    float _speed;
    bool _hasLat;
    bool _hasLon;
    bool _hasAccuracy;
    bool _hasAlt;
    bool _hasSpeed;
    bool _sent;

    bool _dispatch();

public:
    explicit LocationUpdate(Firmngin *instance);
    ~LocationUpdate();

    LocationUpdate(const LocationUpdate &) = delete;
    LocationUpdate &operator=(const LocationUpdate &) = delete;
    LocationUpdate(LocationUpdate &&other) noexcept;
    LocationUpdate &operator=(LocationUpdate &&other) noexcept;

    LocationUpdate &lat(double value);
    LocationUpdate &lon(double value);
    LocationUpdate &accuracy(float value);
    LocationUpdate &alt(float value);
    LocationUpdate &speed(float value);
};

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

    ~BatchState();

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

    BatchState &add(const Entity &entity, const char *value)
    {
        return add(entity.key(), String(value));
    }

    BatchState &add(const Entity &entity, int value)
    {
        return add(entity.key(), String(value));
    }

    BatchState &add(const Entity &entity, float value)
    {
        return add(entity.key(), String(value));
    }

    BatchState &add(const Entity &entity, double value)
    {
        return add(entity.key(), String(value));
    }

    int count() { return _builder.count(); }
    void clear() { _builder.clear(); }
};

class Firmngin
{
public:
#if defined(ESP32)
    Firmngin(const char *deviceId, const char *deviceKey);
    Firmngin(const char *deviceId, const char *deviceKey, const char *caCert, const char *clientCert, const char *privateKey);
    Firmngin(const char *deviceId, const char *deviceKey, const uint8_t *fingerprint, const char *clientCert, const char *privateKey);
#else
    Firmngin(const char *deviceId, const char *deviceKey);
#endif
    ~Firmngin();

    void begin();
    void loop();
    static bool isProvisioned();
    void setDebug(bool debug);
    void setTimezone(int timezone);
    void setDaylightOffsetSec(int daylightOffsetSec);
    void setNtpServer(const char *ntpServer);
    void setBrokerServer(const char *server, int port, bool noTls = (FIRMNGIN_NO_TLS != 0));
    void setInsecure(bool insecure = true);
    bool isPlatformSupported();

    // Second server connection on the bundled client. Auto-reconnect, QoS 1 send,
    // persistent session. Listening is set up before or after begin, and restored
    // on every reconnect. Messages are delivered from loop().
    // TLS verifies caCert. Without a CA, call mqttClientSetInsecure(true) first.
    void mqttClientSetBufferSize(uint16_t size);
    void mqttClientSetInsecure(bool insecure = true);
    bool mqttClientBegin(const char *host, uint16_t port,
                         const char *clientId, const char *user, const char *pass,
                         const char *willTopic, const char *willMessage,
                         MqttMessageHandler onMessage, bool tls = true,
                         const char *caCert = nullptr);
    bool mqttClientSubscribe(const char *topic, int qos = 1);
    bool mqttClientPublish(const char *topic, const char *payload, bool retained = false);
    bool mqttClientConnected() const;
    void mqttClientLoop();
    void mqttClientDisconnect();

    // Message queue monitoring
    uint32_t getDroppedMessageCount() const;
    uint32_t getReconnectCount() const;
    void resetMessageStats();

    // Persistent queue for messages sent while offline (LITTLEFS-backed)
    void setQueueEnabled(bool enabled = true);
    void setMaxQueueEntries(uint16_t maxEntries);
    uint16_t getQueueSize() const;
    void clearQueue();

    void on(const char *state, StateCallbackFunction callback);
    void on(DeviceStateType state, StateCallbackFunction callback);
    void on(DeviceStateType state, VerificationCallbackFunction callback);
    void on(DeviceStateType state, PaymentCallbackFunction callback);
    void on(DeviceStateType state, DispenseCallbackFunction callback);
    void on(DeviceStateType state, UsageCallbackFunction callback);
    void on(DeviceStateType state, DeviceStateCallbackFunction callback);
    void on(DeviceStateType state, InitCallbackFunction callback);
    void on(DeviceStateType state, EntityCommandCallbackFunction callback);
    void on(DeviceStateType state, ActiveSessionCallbackFunction callback);
    void onEntity(const char *key, EntityCommandCallbackFunction callback);

    // Audio reaches a device over one persistent WebSocket, not a URL per clip: the
    // backend streams PCM down it and a newer clip cancels the one still playing.
    // openAudioStream() takes the endpoint and bearer token from the device's
    // provisioning bundle. Frames are drained from loop() into I2S (so the callback
    // may block on the hardware clock); control events also surface there. The
    // callback receives an I2S-ready interleaved stereo frame, not the mono wire frame.
    // onChunk / onEvent are optional: if a speaker was registered via
    // setAudioInterface(entity, chip), PCM is written there automatically when
    // onChunk is omitted.
    bool openAudioStream(AudioChunkCallbackFunction onChunk = nullptr,
                         AudioStreamEventCallbackFunction onEvent = nullptr);
    bool openAudioStream(const FirmnginAudioConfig &config,
                         AudioChunkCallbackFunction onChunk = nullptr,
                         AudioStreamEventCallbackFunction onEvent = nullptr);
    bool openAudioStream(const char *url, const char *token,
                         AudioChunkCallbackFunction onChunk = nullptr,
                         AudioStreamEventCallbackFunction onEvent = nullptr);
    void closeAudioStream();
    bool isAudioStreamConnected() const;

    // With a speaker registered via setAudioInterface(entity, chip), the stream is
    // automatic: begin() arms it and loop() connects once the clock is synced, so the
    // device just connects and stands by. enableAudioStream() re-arms after
    // closeAudioStream(), and the optional onEvent replaces the Serial error report.
    void enableAudioStream(AudioStreamEventCallbackFunction onEvent = nullptr);

    // Bind a server entity key to the audio stream: a clip whose control frame carries
    // that entity_key reaches `handler`, and playback state is reported back on the
    // key. ON_AUDIO() is the declarative form of this.
    Firmngin &bindAudioEntity(const char *key, AudioStreamEventCallbackFunction handler);

    // Register a named audio output bound to a server entity: clips whose control
    // frame carries that entity_key play here. Returns the I2S handle for fluent
    // config (pins/sampleRate/dma/mclk). Hardware survives closeAudioStream().
    // This is the only way to configure audio output — every speaker must be bound
    // to an entity, there is no unnamed default or unknown-key fallback.
    //   Entity speaker1("speaker1");
    //   fngin.setAudioInterface(speaker1, MAX98357A).pins(BCLK, LRC, DOUT);
    FirmnginI2s &setAudioInterface(const Entity &entity, FirmnginI2sChip chip);
    FirmnginI2s &setAudioInterface(const char *key, FirmnginI2sChip chip);

    // Bind an audio_input entity to an I2S microphone. Capture is explicit:
    // startAudioInput() streams one recording over the device's existing WSS,
    // and stopAudioInput() finalizes and saves it. Only one microphone may
    // record at a time. INMP441 defaults to its left slot; call .channel() when
    // the module's L/R pin is tied HIGH.
    FirmnginI2s &setAudioInput(const Entity &entity, FirmnginI2sChip chip = INMP441);
    FirmnginI2s &setAudioInput(const char *key, FirmnginI2sChip chip = INMP441);
    bool startAudioInput(const Entity &entity);
    bool startAudioInput(const char *key);
    void stopAudioInput();
    bool isAudioInputRecording() const;

    bool pushEntity(const char *key, const char *value);
    bool pushEntity(const char *key, const String &value);
    bool pushEntity(const char *key, int value);
    bool pushEntity(const char *key, unsigned long value);
    bool pushEntity(const char *key, float value);
    bool pushEntity(const char *key, double value);
    bool pushEntity(const char *key, float value, int decimals);
    bool pushEntity(const char *key, double value, int decimals);
    bool pushEntity(const char *key, bool value);
    bool pushEntity(const Entity &entity, const char *value);
    bool pushEntity(const Entity &entity, const String &value);
    bool pushEntity(const Entity &entity, int value);
    bool pushEntity(const Entity &entity, unsigned long value);
    bool pushEntity(const Entity &entity, float value);
    bool pushEntity(const Entity &entity, double value);
    bool pushEntity(const Entity &entity, float value, int decimals);
    bool pushEntity(const Entity &entity, double value, int decimals);
    bool pushEntity(const Entity &entity, bool value);
    bool updateEntities(const char *jsonPayload);
    EntityValue entity(const char *key);
    EntityValue entity(const Entity &entity);
    bool endSession();
    bool requestInit();
    // Ask for a QR for an amount: sets the amount to charge and sends the
    // request in one call. Digits only; the amount range belongs to the server,
    // so a range rejection arrives later as a failure code in onTerminalQR.
    // Returns nullptr once the
    // request is sent, otherwise a short literal reason to show the
    // operator ("amount is empty", "digits only", "amount too long",
    // "Failed to sent message").
    const char *setAmountTerminalQR(const String &amount);
    // Cancel the QR this device has open. It targets this device's own
    // transaction, so no request id is needed. The result arrives through the
    // existing onTerminalQR/onTerminalPaymentFailed callbacks on /c/{id}/qr
    // as st=canceled then st=idle (or a failure code).
    bool cancelTerminalQR();
    void onTerminalQR(TerminalQRCallbackFunction callback);
    void onTerminalPaymentFailed(TerminalQRCallbackFunction callback);
    LocationUpdate pushLocation();
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
    bool _identityLoaded = false;
    bool _runtimeUseFingerprint = false;
    bool _runtimeUseCA = false;
    uint8_t _runtimeFingerprint[20] = {0};
    String _runtimeDeviceId;
    String _runtimeDeviceKey;
    String _runtimeClientCert;
    String _runtimePrivateKey;
    String _runtimeCaCert;
    String _runtimeServiceCaCert;
    String _runtimeDecryptor;
    String _apiBaseUrl = FIRMNGIN_API_BASE_URL;
    // Audio gateway base. A provisioning record may replace the whole stream URL
    // through _runtimeAudioStreamUrl; this is what the device falls back to.
    String _audioStreamBaseUrl = FIRMNGIN_AUDIO_STREAM_BASE_URL;
    String _runtimeAudioStreamUrl = "";
    String _runtimeAudioStreamToken = "";
    // Named audio outputs (multi-speaker). One WebSocket still carries every clip and
    // clips play sequentially, so the active sink switches at audio.start(entity_key)
    // and one playback FIFO serves them all.
    struct SpeakerSink
    {
        String key;
        FirmnginI2s i2s;
    };
    std::vector<SpeakerSink> _speakers;
    struct MicrophoneSource
    {
        String key;
        FirmnginI2s i2s;
    };
    std::vector<MicrophoneSource> _microphones;
    String _activeSinkKey;
    FirmnginI2s *_activeSink();
    bool _writeActiveSink(const uint8_t *data, size_t len);
    bool _anySinkReady() const;
    bool _hasAudioInterfaceConfigured() const;
    bool _debug;
    unsigned long _lastMQTTAttempt;

#if defined(ESP32)
    WiFiClientSecure _wifiClient;
    WiFiClient _plainWifiClient;
#else
    WiFiClient _wifiClient;
#endif
#if defined(ESP32)
    esp_mqtt_client_handle_t _mqtt = nullptr;
    volatile bool _mqttUp = false;
    volatile bool _mqttHelloPending = false;
    String _mqttWillTopic;
    QueueHandle_t _mqttInbox = nullptr;
    SemaphoreHandle_t _mqttLock = nullptr;
    char *_rxTopic[2] = {};
    uint8_t *_rxData[2] = {};
    int _rxFilled[2] = {};
    int _rxTotal[2] = {};

    esp_mqtt_client_handle_t _userMqtt = nullptr;
    volatile bool _userMqttUp = false;
    bool _userMqttTls = true;
    String _userMqttHost;
    String _userMqttClientId;
    String _userMqttUser;
    String _userMqttPass;
    String _userMqttWillTopic;
    String _userMqttWillMsg;
    String _userMqttCa;
    uint16_t _userMqttPort = 0;
    MqttMessageHandler _userMqttCallback;
    struct UserMqttSub
    {
        String topic;
        int qos;
    };
    std::vector<UserMqttSub> _userMqttSubs;
#endif
    uint16_t _userMqttBufferSize = FIRMNGIN_MQTT_CLIENT_DEFAULT_BUFFER_SIZE;
    bool _userMqttInsecure = false;

    unsigned long _delayRetryMQTT = 5000;
    int maxRetryMQTT = 3;
    int defaultQos = 1;

    // Message stats & monitoring
    volatile uint32_t _msgDroppedCount = 0;
    volatile uint32_t _msgPlatformReconnects = 0;
    volatile uint32_t _msgUserReconnects = 0;
    unsigned long _msgLastReconnectMs = 0;
    unsigned long _msgReconnectBackoff = 5000;
    const unsigned long _msgReconnectBackoffMin = 5000;
    const unsigned long _msgReconnectBackoffMax = 60000;

    String _mqttServer;
    int _mqttPort;
    bool _mqttNoTls = (FIRMNGIN_NO_TLS != 0);
    bool _insecure = false;
    bool _securityReady = false;
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

    // Persistent offline queue state (LITTLEFS circular buffer)
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
    String _otaFirmwareVersion;

    // OTA boot rollback tracking (NVS via Preferences)
    bool _otaRollbackReady = false;
    bool _otaRollbackPending = false;
    int _otaRollbackBootCount = 0;
    String _otaLastOkVersion;
    bool _otaRollbackMarkedThisBoot = false;
    bool _otaRollbackReportPublished = false;
    unsigned long _otaBootStartMs = 0;
    String _otaBootReportStatus;
    String _otaBootReportMessage;
#if defined(ESP32)
    Preferences _otaPrefs;
#endif

    AudioChunkCallbackFunction _audioChunkCallback;
    AudioStreamEventCallbackFunction _audioEventCallback;
    bool _audioUsesBuiltinSink = false;
    // entity_key -> ON_AUDIO handler, so one device stream can serve several
    // audio_output entities.
    std::map<String, AudioStreamEventCallbackFunction> _audioEntityCallbacks;
    // I2S configured + not yet connected: loop() opens the stream once the clock is
    // synced. Cleared on connect and by closeAudioStream().
    bool _audioAutoConnect = false;
    unsigned long _audioAutoConnectAtMs = 0;
    // Set once an init frame has been parsed: without a provisioned URL the stream
    // waits for it instead of guessing the derived path.
    bool _deviceInitReceived = false;
#if defined(ESP32)
    esp_websocket_client_handle_t _audioWs = nullptr;
    // Mono wire frames land here from the WebSocket task and leave from loop().
    // Enqueue is non-blocking: a full ring drops one frame so the WS task can still
    // read audio.flush on stop (blocking here deadlocks the socket).
    StreamBufferHandle_t _audioRx = nullptr;
    QueueHandle_t _audioEvents = nullptr;
    // WEBSOCKET_EVENT_DATA fires per transport read, so one 640-byte wire frame can
    // arrive as several chunks (payload_offset resets per read). These reassemble it
    // before the frame-aligned FIFO.
    uint8_t _audioPcmAccum[AUDIO_FRAME_BYTES * 2];
    uint16_t _audioPcmAccumLen = 0;
    // Bumped on flush/disconnect so a blocked enqueue aborts cleanly.
    volatile uint32_t _audioFlushGen = 0;
    // Binary frames are enqueued only while a clip is running (between audio.start
    // and audio.end). Cleared by flush/stop/disconnect so in-flight frames left in
    // TCP buffers after a stop are dropped instead of replaying the stopped clip.
    volatile bool _audioStreaming = false;
    // True after audio.end until loop() finishes draining the ring at full level.
    bool _audioEndPending = false;
    // The client keeps the config strings by pointer, so they must outlive init().
    String _audioUrl;
    String _audioHeaders;
    String _audioToken;
    uint8_t _audioBufferFrames = FIRMNGIN_AUDIO_BUFFER_FRAMES;
    uint8_t _audioFramesPerLoop = FIRMNGIN_AUDIO_FRAMES_PER_LOOP;
    uint16_t _audioWsBufferBytes = FIRMNGIN_AUDIO_WS_BUFFER_BYTES;
    uint16_t _audioWsTaskStack = FIRMNGIN_AUDIO_WS_TASK_STACK;
    bool _audioReconnectPending = false;
    bool _audioKeepAlive = false;
    unsigned long _audioReconnectAtMs = 0;
    char _audioEndDetail[AUDIO_DETAIL_MAX] = {};
    TaskHandle_t _audioInputTaskHandle = nullptr;
    volatile bool _audioInputRecording = false;
    volatile bool _audioInputStopRequested = false;
    volatile bool _audioInputCancelRequested = false;
    MicrophoneSource *_activeMicrophone = nullptr;
    String _audioInputCaptureId;
#endif
    void _processAudio();
    void _tryAutoConnectAudio();
    void _applyAudioStreamUrl(const String &url);
    void _dispatchAudioEntity(const FirmnginAudioEventMessage &message);
    void _notifyAudioEvent(FirmnginAudioEvent event, const char *detail, const char *entityKey = nullptr);
    String _resolveAudioStreamUrl() const;
#if defined(ESP32)
    void _releaseAudioQueues();
    void _stopAudioWs();
    void _abortAudioClip();
    bool _enqueueAudioFrame(const uint8_t *frame, uint32_t flushGen);
    bool _buildAudioWsHeaders();
    bool _startAudioWsClient();
    bool _waitForAudioClock();
    bool _sendAudioInputControl(const char *type, const char *entityKey, const char *captureId);
    void _finishAudioInput(bool cancelled);
    void _runAudioInput();
    static void _audioInputTask(void *context);
    static void _audioWsEventHandler(void *handlerArgs, esp_event_base_t base, int32_t eventId, void *eventData);
#endif

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
#endif

    bool otaHTTPGet(const char *path, const char *queryParams, String &responseBody);

#if defined(ESP32)
    const char *_caCert = nullptr;
    const char *_clientCert = nullptr;
    const char *_privateKey = nullptr;
    const uint8_t *_fingerprint = nullptr;
#endif

    std::map<String, StateCallbackFunction> _callbacks;
    VerificationCallbackFunction _verificationCallback;
    PaymentCallbackFunction _paymentsCallback;
    DispenseCallbackFunction _dispenseCallback;
    std::vector<TerminalQRCallbackFunction> _terminalQRCallbacks;
    std::vector<TerminalQRCallbackFunction> _terminalPaymentFailedCallbacks;
    uint32_t _terminalRequestSeq = 0;
    UsageCallbackFunction _usagesCallback;
    DeviceStateCallbackFunction _deviceStateCallback;
    InitCallbackFunction _initCallback;
    EntityCommandCallbackFunction _entityCallback;
    std::vector<VerificationCallbackFunction> _verificationCallbacks;
    std::vector<PaymentCallbackFunction> _paymentCallbacks;
    std::vector<DispenseCallbackFunction> _dispenseCallbacks;
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
    bool _networkReady() const;
    bool _mqttIsConnected() const;
    bool _mqttIsStarted() const;
    bool _mqttPublish(const char *topic, const uint8_t *data, size_t len, bool retained);
    void _ensureMqttInbox();
    void _drainMqttInbox();
    void _mqttAfterConnect();
    void _mqttShutdown();
#if defined(ESP32)
    friend void firmnginMqttEvent(void *arg, esp_event_base_t base, int32_t event_id, void *event_data);
    void _onMqttEvent(esp_mqtt_event_handle_t event);
    void _subscribePlatform();
    void _userMqttResubscribe();
    void _mqttPost(uint8_t which, const char *topic, const uint8_t *data, unsigned len);
    void _mqttRxReset(uint8_t which);
#endif
    bool connectServer();
    void mqttCallback(char *path, byte *payload, unsigned int length);
    String getPathPayment(String deviceId);
    String getPathDispense(String deviceId);
    String getPathDeviceStatus(String deviceId);
    String getPathPendingPayment(String deviceId);
    String getPathPostpaidReady(String deviceId);
    String getPathMetadataOnPending(String deviceId);
    String getPathMetadataPostpaidReady(String deviceId);
    String getPathMetadataOnActiveService(String deviceId);
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
    uint64_t getTimestampMillis(); // Returns Unix epoch milliseconds, or 0 until system time is valid
    bool publishPayload(const char *path, const char *payload, bool retained = false);
    bool publishOTAStatus(const char *status, const char *message);
    const char *_activeServiceCACert() const;
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
    void _otaRollbackSetup();
    void _otaRollbackLoop();
    void _persistOtaRollbackState();
    void runActiveSessionHandlers();
    void setMerchantStatus(const String &status);
    void setCurrentOrder(const String &orderId);
    void clearCurrentSession();
};

inline BatchState::~BatchState()
{
    if (_instance != nullptr)
        _instance->updateEntities(_builder.build());
}

#endif // FIRMNGINKIT_H
