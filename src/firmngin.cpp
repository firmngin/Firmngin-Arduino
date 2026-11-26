#include "firmngin.h"
#include "firmngin_identity.h"
#include "hmac.h"
#include "http_response.h"
#include "identity_meta.h"
#include <stdarg.h>
#include <math.h>
#include <memory>
#include <new>
#include <algorithm>
#if defined(ESP_PLATFORM) && !defined(ARDUINO)
#include <esp_sntp.h>
#endif

#if defined(ESP_PLATFORM)
#include <esp_system.h>
#include <esp_random.h>
#endif

#if defined(ESP_PLATFORM)
#include <esp_ota_ops.h>
#endif

const char *NTP_SERVER = "pool.ntp.org";
int GMT_OFFSET_SEC = 7 * 3600;
int DAYLIGHT_OFFSET_SEC = 0;

const char *MQTT_SERVER_ADDR = FIRMNGIN_BROKER_ADDR;
const int MQTT_SERVER_PORT = FIRMNGIN_BROKER_PORT;

static bool parseFingerprintHex(const String &hexValue, uint8_t out[20])
{
    String hex = hexValue;
    fng_replace_all(hex, ":", "");
    fng_replace_all(hex, " ", "");
    fng_toUpperCase(hex);
    if (hex.length() != 40)
        return false;
    for (int i = 0; i < 20; i++)
    {
        char c1 = hex[i * 2];
        char c2 = hex[i * 2 + 1];
        auto nibble = [](char c) -> int
        {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'A' && c <= 'F')
                return c - 'A' + 10;
            return -1;
        };
        int hi = nibble(c1);
        int lo = nibble(c2);
        if (hi < 0 || lo < 0)
            return false;
        out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
}

static bool validateDeviceCredentials(const char *deviceId, const char *deviceKey)
{
    bool valid = true;
    if (deviceId == nullptr || deviceId[0] == '\0' || strcmp(deviceId, "YOUR_DEVICE_ID") == 0)
    {
        Console.println("ERROR: DEVICE_ID is missing or still uses the template placeholder");
        valid = false;
    }
    if (deviceKey == nullptr || deviceKey[0] == '\0' || strcmp(deviceKey, "YOUR_DEVICE_SECRET_KEY") == 0)
    {
        Console.println("ERROR: DEVICE_KEY is missing or still uses the template placeholder");
        valid = false;
    }
    return valid;
}

static bool decodeE2EEKey(const char *hex, uint8_t *output, uint8_t &outputLen)
{
    outputLen = 0;
    if (hex == nullptr)
        return false;
    const size_t hexLen = strlen(hex);
    if (hexLen != 32 && hexLen != 64)
        return false;
    const size_t keyLen = hexLen / 2;
    for (size_t i = 0; i < keyLen; i++)
    {
        const char c1 = hex[i * 2];
        const char c2 = hex[i * 2 + 1];
        const bool c1Valid = (c1 >= '0' && c1 <= '9') || (c1 >= 'a' && c1 <= 'f') || (c1 >= 'A' && c1 <= 'F');
        const bool c2Valid = (c2 >= '0' && c2 <= '9') || (c2 >= 'a' && c2 <= 'f') || (c2 >= 'A' && c2 <= 'F');
        if (!c1Valid || !c2Valid)
            return false;
        const uint8_t d1 = (c1 >= '0' && c1 <= '9') ? c1 - '0' : ((c1 | 0x20) - 'a' + 10);
        const uint8_t d2 = (c2 >= '0' && c2 <= '9') ? c2 - '0' : ((c2 | 0x20) - 'a' + 10);
        output[i] = (d1 << 4) | d2;
    }
    outputLen = static_cast<uint8_t>(keyLen);
    return true;
}

static bool isConfiguredPEM(const char *pem, const char *beginMarker)
{
    return pem != nullptr && pem[0] != '\0' && strstr(pem, beginMarker) != nullptr && strstr(pem, "[Your") == nullptr;
}

#if defined(ESP_PLATFORM)
Firmngin::Firmngin(const char *deviceId, const char *deviceKey)
    : _deviceId(deviceId),
      _deviceKey(deviceKey),
      _debug(false),
      _lastMQTTAttempt(0),
      _mqttServer(MQTT_SERVER_ADDR),
      _mqttPort(MQTT_SERVER_PORT),
      _e2eeEnabled(false)
{
    if (!PLATFORM_SUPPORTED)
    {
        Console.println("ERROR: Platform not supported");
    }
}

Firmngin::Firmngin(const char *deviceId, const char *deviceKey, const char *caCert, const char *clientCert, const char *privateKey)
    : _deviceId(deviceId),
      _deviceKey(deviceKey),
      _debug(false),
      _lastMQTTAttempt(0),
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
        Console.println("ERROR: Platform not supported");
    }
}

Firmngin::Firmngin(const char *deviceId, const char *deviceKey, const uint8_t *fingerprint, const char *clientCert, const char *privateKey)
    : _deviceId(deviceId),
      _deviceKey(deviceKey),
      _debug(false),
      _lastMQTTAttempt(0),
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
        Console.println("ERROR: Platform not supported");
    }
}
#else
Firmngin::Firmngin(const char *deviceId, const char *deviceKey)
    : _deviceId(deviceId),
      _deviceKey(deviceKey),
      _debug(false),
      _lastMQTTAttempt(0),
      _mqttServer(MQTT_SERVER_ADDR),
      _mqttPort(MQTT_SERVER_PORT),
      _e2eeEnabled(false)
{
    if (!PLATFORM_SUPPORTED)
    {
        Console.println("ERROR: Platform not supported");
    }
}
#endif


Firmngin::~Firmngin()
{
    _otaCleanup();
    _mqttShutdown();
}

bool Firmngin::setIdentity(const FirmnginIdentityRecord &identity)
{
    if (_securityReady || _otaTransfer != nullptr || identity.tlsMode != "ca" ||
        !validateDeviceCredentials(identity.deviceId.c_str(), identity.deviceKey.c_str()) ||
        !validateIdentityRecord(identity, false))
        return false;
    _applyIdentity(identity);
    _runtimeIdentityConfigured = true;
    return true;
}

void Firmngin::_applyIdentity(const FirmnginIdentityRecord &identity)
{
    _runtimeDeviceId = identity.deviceId;
    _runtimeDeviceKey = identity.deviceKey;
    _deviceId = _runtimeDeviceId.c_str();
    _deviceKey = _runtimeDeviceKey.c_str();
    _runtimeClientCert = identity.clientCert;
    _runtimePrivateKey = identity.privateKey;
    _runtimeCaCert = identity.caCert;
    _runtimeServiceCaCert = identity.serviceCaCert;
    _runtimeDecryptor = identity.decryptor;
    _runtimeUseFingerprint = identity.tlsMode == "fingerprint";
    _runtimeUseCA = identity.tlsMode == "ca";
    if (identity.fingerprintHex.length() > 0)
        _runtimeUseFingerprint = parseFingerprintHex(identity.fingerprintHex, _runtimeFingerprint);
    if (identity.brokerAddr.length() > 0)
        _mqttServer = identity.brokerAddr;
    if (identity.brokerPort > 0)
        _mqttPort = identity.brokerPort;
    if (identity.apiBaseUrl.length() > 0)
        _apiBaseUrl = identity.apiBaseUrl;
    if (identity.otaBaseUrl.length() > 0)
        _otaBaseUrl = identity.otaBaseUrl;
    if (identity.audioStreamUrl.length() > 0)
        _runtimeAudioStreamUrl = identity.audioStreamUrl;
    _runtimeAudioStreamToken = identity.audioStreamToken;
}

String Firmngin::getPathPayment(String deviceId)
{
    return String("/c/") + deviceId + "/" + PATH_PAYMENT;
}

String Firmngin::getPathDispense(String deviceId)
{
    return String("/c/") + deviceId + "/" + PATH_DISPENSE;
}

String Firmngin::getPathDeviceStatus(String deviceId)
{
    return String("/c/") + deviceId + "/" + PATH_DEVICE_STATUS;
}

String Firmngin::getPathPendingPayment(String deviceId)
{
    return String("/c/") + deviceId + "/" + PATH_PENDING_PAYMENT;
}

String Firmngin::getPathPostpaidReady(String deviceId)
{
    return String("/c/") + deviceId + "/" + PATH_POSTPAID_READY;
}

String Firmngin::getPathMetadataOnPending(String deviceId)
{
    return String("/c/") + deviceId + "/" + PATH_METADATA_ON_PENDING;
}

String Firmngin::getPathMetadataPostpaidReady(String deviceId)
{
    return String("/c/") + deviceId + "/" + PATH_METADATA_POSTPAID_READY;
}

String Firmngin::getPathMetadataOnActiveService(String deviceId)
{
    return String("/c/") + deviceId + "/" + PATH_METADATA_ON_ACTIVE_SERVICE;
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
    Console.println();
    Console.println("   __ _                            _            _            ");
    Console.println("  / _(_)                          (_)          | |           ");
    Console.println(" | |_ _ _ __ _ __ ___  _ __   __ _ _ _ __    __| | _____   __");
    Console.println(" |  _| | '__| '_ ` _ \\| '_ \\ / _` | | '_ \\  / _` |/ _ \\ \\ / /");
    Console.println(" | | | | |  | | | | | | | | | (_| | | | | || (_| |  __/\\ V / ");
    Console.println(" |_| |_|_|  |_| |_| |_|_| |_|\\__, |_|_| |_(_)__,_|\\___| \\_/  ");
    Console.println("                              __/ |                          ");
    Console.println(" firmngin.dev AIoT Platform  |___/                           ");
    Console.print(" Version: ");
    Console.println(FIRMNGIN_VERSION);
    Console.println();
}

bool Firmngin::isProvisioned()
{
    return FirmnginIdentity::isProvisioned();
}

void Firmngin::begin()
{
    _securityReady = false;
    _e2eeEnabled = false;
    _e2eeKeyLen = 0;
    memset(_e2eeKeyBytes, 0, sizeof(_e2eeKeyBytes));

    if (!PLATFORM_SUPPORTED)
        return;

    bool audioPins = false;
    for (auto &sink : _speakers)
        if (sink.i2s.pinsConfigured())
        {
            sink.i2s.begin();
            audioPins = true;
        }
    for (const auto &source : _microphones)
        if (source.i2s.pinsConfigured())
            audioPins = true;
    if (audioPins)
    {
#if defined(ESP_PLATFORM)
        // A registered speaker or microphone needs the device-wide stream:
        // connect and stand by even when this is an input-only device.
        _audioAutoConnect = true;
#endif
    }

    _otaRollbackSetup();

    FirmnginIdentityRecord flashIdentity;
    const bool firmwareIdentityConfigured = validateDeviceCredentials(_deviceId, _deviceKey);
#if KEYS_H_AVAILABLE
    const bool allowFlashIdentity = false;
#else
    const bool allowFlashIdentity = _identityLoaded || !firmwareIdentityConfigured;
#endif
    _identityLoaded = _runtimeIdentityConfigured ||
                      (allowFlashIdentity && FirmnginIdentity::load(flashIdentity));
    const char *activeDecryptor = nullptr;

    if (_identityLoaded)
    {
        if (!_runtimeIdentityConfigured)
            _applyIdentity(flashIdentity);
        activeDecryptor = _runtimeDecryptor.c_str();
    }
    else if (!firmwareIdentityConfigured)
    {
        Console.println("ERROR: device identity is not configured; connection blocked");
        return;
    }
    else
    {
#if KEYS_H_AVAILABLE
        activeDecryptor = DECRYPTOR;
#endif
    }

    if (!validateDeviceCredentials(_deviceId, _deviceKey))
    {
        Console.println("ERROR: invalid device credentials; connection blocked");
        return;
    }

    if (_runtimeIdentityConfigured)
    {
        _Debug("Identity loaded from runtime configuration");
    }
    else if (_identityLoaded)
    {
        _Debug("Identity loaded from /firmnginfs");
    }
    else
    {
#if KEYS_H_AVAILABLE
        _Debug("Identity loaded from keys.h");
#else
        _Debug("Identity loaded from firmware configuration");
#endif
    }

    _topicUpdateEntity = "/d/" + String(_deviceId) + "/ps";
    _topicUpdateEntities = "/d/" + String(_deviceId) + "/psb";
    _cloudLogTopic = "/d/" + String(_deviceId) + "/lg";
    cloudlog.begin(fng_now_ms);
    cloudlog.onSink(
        [](const char *line, unsigned long tsMs, void *user) -> bool {
            return static_cast<Firmngin *>(user)->_cloudLogSink(line, tsMs, user);
        },
        this);

    if (!_networkReady())
    {
        _Debug("network not connected, call begin() after WiFi or Ethernet has an IP");
        return;
    }

    syncTime();
    printBanner();

#if defined(ESP_PLATFORM)
    if (_mqttNoTls)
    {
        Console.println("WARNING: server dev mode — TLS disabled");
    }
    else
    {
#endif
#if defined(ESP_PLATFORM)
    const char *caCert = _identityLoaded ? _runtimeCaCert.c_str() : _caCert;
    const char *clientCert = _identityLoaded ? _runtimeClientCert.c_str() : _clientCert;
    const char *privateKey = _identityLoaded ? _runtimePrivateKey.c_str() : _privateKey;

#if KEYS_H_AVAILABLE
    if (!_identityLoaded)
    {
        if (caCert == nullptr)
            caCert = CA_CERT;
        if (clientCert == nullptr)
            clientCert = CLIENT_CERT;
        if (privateKey == nullptr)
            privateKey = PRIVATE_KEY;
    }
#else
    (void)caCert;
    (void)clientCert;
    (void)privateKey;
#endif

    if (!isConfiguredPEM(clientCert, "-----BEGIN CERTIFICATE-----") ||
        !isConfiguredPEM(privateKey, "PRIVATE KEY-----"))
    {
        Console.println("ERROR: mTLS credentials not configured");
        return;
    }

    _caCert = caCert;
    _clientCert = clientCert;
    _privateKey = privateKey;

    if (_insecure)
    {
        Console.println("Server validation: DISABLED (setInsecure)");
    }
    else if (caCert != nullptr)
    {
        if (!isConfiguredPEM(caCert, "-----BEGIN CERTIFICATE-----"))
        {
            Console.println("ERROR: CA certificate is malformed or still uses the template placeholder; connection blocked");
            return;
        }
    }
    else
    {
        Console.println("ERROR: CA certificate is not configured; connection blocked");
        return;
    }
#endif
#if defined(ESP_PLATFORM)
    }
#endif

    if (activeDecryptor == nullptr || activeDecryptor[0] == '\0' || strlen(activeDecryptor) == 0)
    {
        _e2eeKeyLen = 0;
    }
    else if (!decodeE2EEKey(activeDecryptor, _e2eeKeyBytes, _e2eeKeyLen))
    {
        _e2eeKeyLen = 0;
    }
    else
    {
        _e2eeEnabled = true;
    }

    _securityReady = true;

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
        for (const auto &entry : deferredAudioEntityRegistrations())
        {
            bindAudioEntity(entry.first.c_str(), entry.second);
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
        for (const auto &callback : deferredDispenseRegistrations())
        {
            _dispenseCallbacks.push_back(callback);
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

const char *Firmngin::_activeServiceCACert() const
{
    if (_identityLoaded)
    {
        const char *runtimeCA = _runtimeServiceCaCert.c_str();
        return isConfiguredPEM(runtimeCA, "-----BEGIN CERTIFICATE-----") ? runtimeCA : nullptr;
    }
#if defined(HAS_SERVICE_CA_CERT)
    return isConfiguredPEM(SERVICE_CA_CERT, "-----BEGIN CERTIFICATE-----") ? SERVICE_CA_CERT : nullptr;
#else
    return nullptr;
#endif
}

void Firmngin::setBrokerServer(const char *server, int port, bool noTls)
{
    _mqttServer = server;
    _mqttPort = port;
    _mqttNoTls = noTls;
}

void Firmngin::setInsecure(bool insecure)
{
    _insecure = insecure;
    if (insecure)
        Console.println("WARNING: TLS certificate verification explicitly disabled");
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

void Firmngin::setDaylightOffsetSec(int daylightOffsetSec)
{
    DAYLIGHT_OFFSET_SEC = daylightOffsetSec;
}

void Firmngin::syncTime()
{
#if defined(ARDUINO)
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
#elif defined(ESP_PLATFORM)
    esp_sntp_stop();
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, NTP_SERVER);
    esp_sntp_init();
    // POSIX TZ offsets have the opposite sign to the configured UTC offset.
    const long offset = -(GMT_OFFSET_SEC + DAYLIGHT_OFFSET_SEC);
    const long seconds = labs(offset);
    char timezone[48];
    snprintf(timezone, sizeof(timezone), "UTC%s%ld:%02ld:%02ld",
             offset < 0 ? "-" : "+", seconds / 3600, seconds / 60 % 60, seconds % 60);
    setenv("TZ", timezone, 1);
    tzset();
#endif
    _ntpSyncStartMs = fng_now_ms();
    _ntpSynced = false;
}

uint64_t Firmngin::getTimestampMillis()
{
    const time_t now = time(nullptr);
    if (now < 1609459200) // 2021-01-01T00:00:00Z; system clock is not synchronized yet.
        return 0;

    return static_cast<uint64_t>(now) * UINT64_C(1000);
}

void Firmngin::on(const char *state, StateCallbackFunction callback)
{
    _callbacks[String(state)] = callback;
}

// State name mapping for paths
static const char *STATE_NAMES[] = {
    "pm",             // PAYMENT
    "ds",             // DEVICE_STATUS
    "pp",             // PENDING_PAYMENT
    "pr",             // POSTPAID_READY
    "mop",            // METADATA_ON_PENDING
    "mpp",            // METADATA_POSTPAID_READY
    "moa",            // METADATA_ON_ACTIVE_SERVICE
    "moe",            // METADATA_ON_EXPIRED
    "mos",            // METADATA_ON_SUCCESS
    "init",           // INIT
    "dpin",           // DISPLAY_PIN_NUMBER
    "vr",             // VERIFICATION_RESULT
    "ur",             // USAGE_RESPONSE
    "le",             // LIMIT_EXCEEDED
    "nl",             // NEAR_LIMIT
    "verif",          // VERIFICATIONS
    "pay",            // PAYMENTS
    "dp",             // DISPENSES
    "usg",            // USAGES
    "rs",             // ENTITIES
    "active_session", // ACTIVE_SESSION
};

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

void Firmngin::on(DeviceStateType state, DispenseCallbackFunction callback)
{
    (void)state;
    _dispenseCallback = callback;
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
TerminalQR::TerminalQR(const String &payload)
{
    firmngin_json::Parser p(payload.c_str(), payload.length());
    // Allocate once for the entire event. Raw QR content must never be truncated
    // to the short string buffers used by payment titles.
    std::unique_ptr<char[]> buf(new (std::nothrow) char[payload.length() + 1]);
    if (!buf)
        return;
    auto read = [&](const char *key) -> String {
        if (p.getString(key, buf.get(), payload.length() + 1) > 0)
            return String(buf.get());
        return String();
    };
    orderId = read("od");
    orderCode = read("oc");
    status = read("st");
    amount = read("amt");
    qrString = read("qr");
    expiresAt = read("exp");
}

void Firmngin::onTerminalPaymentFailed(TerminalQRCallbackFunction callback)
{
    if (callback)
        _terminalPaymentFailedCallbacks.push_back(callback);
}

void Firmngin::onTerminalQR(TerminalQRCallbackFunction callback)
{
    if (callback)
        _terminalQRCallbacks.push_back(callback);
}

Payments::Payments(const String &jsonPayload)
{
    _valid = false;
    _itemTitle = "";
    _price = "";
    _orderId = "";
    _quantity = 1;
    _isPending = false;
    _isSuccess = false;
    _isPostPaid = false;
    _isPrePaid = false;
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

Dispenses::Dispenses(const String &jsonPayload)
{
    _valid = false;
    _count = 0;
    _rawPayload = jsonPayload;

    const char *p = jsonPayload.c_str();
    const char *end = p + jsonPayload.length();
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
        p++;
    if (p >= end || *p != '[')
        return;
    p++;

    char buf[128];
    while (p < end && _count < FIRMNGIN_DISPENSE_MAX_ITEMS)
    {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ','))
            p++;
        if (p >= end || *p == ']')
            break;
        size_t n = firmngin_json::_extractString(p, end, buf, sizeof(buf));
        if (n == 0)
            break;
        _skus[_count++] = String(buf);
        if (*p == '"')
        {
            p++;
            while (p < end && *p != '"')
            {
                if (*p == '\\' && p + 1 < end)
                    p += 2;
                else
                    p++;
            }
            if (p < end && *p == '"')
                p++;
        }
    }

    _valid = _count > 0;
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

// A newer clip cancels the one still playing, so the device plays what arrives
// instead of fetching a URL per clip.
// Request path of a stream url.
static String audioStreamPath(const String &url)
{
    const int schemeEnd = fng_indexOf(url, "://");
    if (schemeEnd < 0)
        return "";
    const int pathStart = fng_indexOf(url, '/', schemeEnd + 3);
    if (pathStart < 0)
        return "";
    return fng_substring(url, pathStart);
}

String Firmngin::_resolveAudioStreamUrl() const
{
    String base = _runtimeAudioStreamUrl;
    if (base.length() > 0 && fng_isWsScheme(base))
        return base;
    if (base.length() == 0)
        base = _audioStreamBaseUrl;

    const bool secure = !fng_startsWith(base, "http://");
    const char *schemeEnd = strstr(base.c_str(), "://");
    if (schemeEnd == nullptr)
        return "";

    char streamId[33];
    if (!firmngin_audio_stream_id(_deviceId, _deviceKey, streamId, sizeof(streamId)))
        return "";

    String hostAndBase = fng_substring(base, static_cast<int>(schemeEnd - base.c_str()) + 3);
    while (fng_endsWith(hostAndBase, "/"))
        hostAndBase = fng_substring(hostAndBase, 0, static_cast<int>(hostAndBase.length()) - 1);
    if (hostAndBase.length() == 0)
        return "";
    return fng_streamScheme(secure) + hostAndBase + "/v1/e/" + streamId;
}

bool Firmngin::openAudioStream(AudioChunkCallbackFunction onChunk, AudioStreamEventCallbackFunction onEvent)
{
    return openAudioStream(FirmnginAudioConfig(), onChunk, onEvent);
}

void Firmngin::enableAudioStream(AudioStreamEventCallbackFunction onEvent)
{
    _audioEventCallback = onEvent;
#if defined(ESP_PLATFORM)
    _audioAutoConnect = true;
    _audioAutoConnectAtMs = 0;
    _tryAutoConnectAudio();
#else
    _notifyAudioEvent(AUDIO_ERROR, "audio streaming requires ESP32");
#endif
}

FirmnginI2s &Firmngin::setAudioInterface(const Entity &entity, FirmnginI2sChip chip)
{
    return setAudioInterface(entity.key().c_str(), chip);
}

FirmnginI2s &Firmngin::setAudioInterface(const char *key, FirmnginI2sChip chip)
{
    if (key != nullptr && key[0] != '\0' && chip != INMP441 && chip != I2S_RX)
    {
        for (auto &sink : _speakers)
            if (sink.key == key)
                return sink.i2s.setChip(chip);
        _speakers.push_back(SpeakerSink{String(key), FirmnginI2s(chip)});
        return _speakers.back().i2s;
    }
    // Every output must be bound to an entity. An empty key gets a never-begun sink
    // whose write() no-ops, so a bug announces itself as silence, not a crash.
    static FirmnginI2s unbound;
    return unbound;
}

FirmnginI2s &Firmngin::setAudioInput(const Entity &entity, FirmnginI2sChip chip)
{
    return setAudioInput(entity.key().c_str(), chip);
}

FirmnginI2s &Firmngin::setAudioInput(const char *key, FirmnginI2sChip chip)
{
    if (key != nullptr && key[0] != '\0' && chip == INMP441)
    {
        for (auto &source : _microphones)
            if (source.key == key)
                return source.i2s.setChip(chip);
#if defined(ESP_PLATFORM)
        // _activeMicrophone points into this vector while the capture task runs.
        // Refuse a new binding until it finishes so vector growth cannot invalidate
        // that pointer underneath the task.
        if (_audioInputRecording)
        {
            static FirmnginI2s recordingLocked(INMP441);
            return recordingLocked;
        }
#endif
        _microphones.push_back(MicrophoneSource{String(key), FirmnginI2s(chip)});
#if defined(ESP_PLATFORM)
        _audioAutoConnect = true;
        _audioAutoConnectAtMs = 0;
#endif
        return _microphones.back().i2s;
    }
    static FirmnginI2s unbound(INMP441);
    return unbound;
}

bool Firmngin::startAudioInput(const Entity &entity)
{
    return startAudioInput(entity.key().c_str());
}

bool Firmngin::startAudioInput(const char *key)
{
#if !defined(ESP_PLATFORM)
    (void)key;
    _notifyAudioEvent(AUDIO_INPUT_ERROR, "audio input requires ESP32");
    return false;
#else
    if (key == nullptr || key[0] == '\0' || _audioInputRecording ||
        _audioWs == nullptr || !esp_websocket_client_is_connected(_audioWs))
        return false;

    MicrophoneSource *source = nullptr;
    for (auto &candidate : _microphones)
        if (candidate.key == key)
        {
            source = &candidate;
            break;
        }
    if (source == nullptr || !source->i2s.pinsConfigured())
        return false;

    // MVP is deliberately half-duplex: stop playback before taking ownership of
    // I2S for the microphone. This also keeps Arduino-ESP32 core 2.x safe, where
    // every FirmnginI2s instance uses I2S_NUM_0.
    _abortAudioClip();
    for (auto &sink : _speakers)
    {
        if (sink.i2s.ready())
            (void)sink.i2s.flush();
        sink.i2s.end();
    }

    source->i2s.begin();
    if (!source->i2s.ready())
    {
        for (auto &sink : _speakers)
            if (sink.i2s.pinsConfigured())
                sink.i2s.begin();
        _notifyAudioEvent(AUDIO_INPUT_ERROR, "audio input I2S could not start", key);
        return false;
    }

    char captureId[32];
    snprintf(captureId, sizeof(captureId), "%lu-%08lx",
             static_cast<unsigned long>(time(nullptr)),
             static_cast<unsigned long>(esp_random()));
    if (!_sendAudioInputControl("audio.input.start", key, captureId))
    {
        source->i2s.end();
        for (auto &sink : _speakers)
            if (sink.i2s.pinsConfigured())
                sink.i2s.begin();
        _notifyAudioEvent(AUDIO_INPUT_ERROR, "audio input start could not be sent", key);
        return false;
    }

    _activeMicrophone = source;
    _audioInputCaptureId = captureId;
    _audioInputStopRequested = false;
    _audioInputCancelRequested = false;
    _audioInputRecording = true;
    if (xTaskCreate(_audioInputTask, "firmngin-mic", FIRMNGIN_AUDIO_INPUT_TASK_STACK,
                    this, 2, &_audioInputTaskHandle) != pdPASS)
    {
        _finishAudioInput(true);
        _notifyAudioEvent(AUDIO_INPUT_ERROR, "audio input task could not start", key);
        return false;
    }
    return true;
#endif
}

void Firmngin::stopAudioInput()
{
#if defined(ESP_PLATFORM)
    _audioInputStopRequested = true;
#endif
}

bool Firmngin::isAudioInputRecording() const
{
#if defined(ESP_PLATFORM)
    return _audioInputRecording;
#else
    return false;
#endif
}

#if defined(ESP_PLATFORM)
bool Firmngin::_sendAudioInputControl(const char *type, const char *entityKey, const char *captureId)
{
    const esp_websocket_client_handle_t ws = _audioWs;
    if (ws == nullptr || !esp_websocket_client_is_connected(ws))
        return false;
    char payload[256];
    firmngin_json::Builder builder(payload, sizeof(payload));
    builder.reset();
    if (!builder.add("type", type) || !builder.add("entity_key", entityKey) ||
        !builder.add("capture_id", captureId))
        return false;
    if (strcmp(type, "audio.input.start") == 0)
    {
        if (!builder.add("codec", "pcm_s16le") ||
            !builder.add("sample_rate", static_cast<uint64_t>(16000)) ||
            !builder.add("channels", static_cast<uint64_t>(1)) ||
            !builder.add("frame_ms", static_cast<uint64_t>(20)) ||
            !builder.add("frame_bytes", static_cast<uint64_t>(AUDIO_FRAME_BYTES)))
            return false;
    }
    const char *json = builder.build();
    const int len = static_cast<int>(strlen(json));
    return esp_websocket_client_send_text(ws, json, len, pdMS_TO_TICKS(1000)) == len;
}

void Firmngin::_finishAudioInput(bool cancelled)
{
    if (!_audioInputRecording)
        return;
    const String key = _activeMicrophone != nullptr ? _activeMicrophone->key : String();
    if (_audioWs != nullptr && esp_websocket_client_is_connected(_audioWs))
        (void)_sendAudioInputControl(cancelled ? "audio.input.cancel" : "audio.input.end",
                                     key.c_str(), _audioInputCaptureId.c_str());
    if (_activeMicrophone != nullptr)
        _activeMicrophone->i2s.end();
    _audioInputRecording = false;
    _audioInputStopRequested = false;
    _audioInputCancelRequested = false;
    _activeMicrophone = nullptr;
    _audioInputCaptureId = "";
    for (auto &sink : _speakers)
        if (sink.i2s.pinsConfigured())
            sink.i2s.begin();
}

void Firmngin::_audioInputTask(void *context)
{
    Firmngin *self = static_cast<Firmngin *>(context);
    if (self != nullptr)
        self->_runAudioInput();
    if (self != nullptr)
        self->_audioInputTaskHandle = nullptr;
    vTaskDelete(nullptr);
}

void Firmngin::_runAudioInput()
{
    uint8_t raw[AUDIO_SAMPLES_PER_FRAME * 2 * sizeof(int32_t)];
    uint8_t pcm[AUDIO_FRAME_BYTES];
    const unsigned long startedAt = fng_now_ms();
    bool cancelled = false;

    while (!_audioInputStopRequested && fng_now_ms() - startedAt < FIRMNGIN_AUDIO_INPUT_MAX_MS)
    {
        MicrophoneSource *source = _activeMicrophone;
        if (source == nullptr || source->i2s.read(raw, sizeof(raw)) != sizeof(raw))
        {
            cancelled = true;
            break;
        }
        if (firmngin_audio_inmp441_to_mono_s16le(
                raw, sizeof(raw), source->i2s.selectedChannel() == FIRMNGIN_I2S_RIGHT,
                pcm, sizeof(pcm)) != sizeof(pcm))
        {
            cancelled = true;
            break;
        }
        const esp_websocket_client_handle_t ws = _audioWs;
        if (ws == nullptr || !esp_websocket_client_is_connected(ws) ||
            esp_websocket_client_send_bin(ws, reinterpret_cast<const char *>(pcm),
                                          sizeof(pcm), pdMS_TO_TICKS(1000)) != sizeof(pcm))
        {
            cancelled = true;
            break;
        }
    }

    cancelled = cancelled || _audioInputCancelRequested;
    const String entityKey = _activeMicrophone != nullptr ? _activeMicrophone->key : String();
    _finishAudioInput(cancelled);
    if (cancelled)
        _notifyAudioEvent(AUDIO_INPUT_ERROR, "audio input capture interrupted", entityKey.c_str());
}
#endif

FirmnginI2s *Firmngin::_activeSink()
{
    // Routing is strict: a missing or unknown entity_key must never make audio leak
    // to another speaker on a multi-output device.
    if (_activeSinkKey.length() == 0)
        return nullptr;
    for (auto &sink : _speakers)
        if (sink.key == _activeSinkKey)
            return &sink.i2s;
    return nullptr;
}

bool Firmngin::_writeActiveSink(const uint8_t *data, size_t len)
{
    FirmnginI2s *sink = _activeSink();
    return sink != nullptr && sink->write(data, len);
}

bool Firmngin::_anySinkReady() const
{
    for (const auto &sink : _speakers)
        if (sink.i2s.ready())
            return true;
    return false;
}

bool Firmngin::_hasAudioInterfaceConfigured() const
{
    for (const auto &sink : _speakers)
        if (sink.i2s.pinsConfigured())
            return true;
    for (const auto &source : _microphones)
        if (source.i2s.pinsConfigured())
            return true;
    return false;
}

void Firmngin::_applyAudioStreamUrl(const String &url)
{
    if (url.length() == 0 || url == _runtimeAudioStreamUrl)
        return;
    _runtimeAudioStreamUrl = url;
#if defined(ESP_PLATFORM)
    if (!_audioKeepAlive)
        return;
    // The endpoint moved under a live stream: re-sign against the new URL on the next
    // loop pass instead of waiting for the socket to drop.
    _audioUrl = url;
    _audioReconnectPending = true;
    _audioReconnectAtMs = fng_now_ms();
#endif
}

void Firmngin::_tryAutoConnectAudio()
{
#if defined(ESP_PLATFORM)
    if (!_audioAutoConnect || _audioKeepAlive || !_hasAudioInterfaceConfigured())
        return;
    // Signing needs a real clock; NTP sync started by begin() flips this.
    if (static_cast<unsigned long>(time(nullptr)) < AUDIO_MIN_VALID_EPOCH)
        return;
    // The endpoint rides in on init's audio_output entity. Without a provisioned URL,
    // wait for that frame rather than guessing the legacy derived path.
    if (_runtimeAudioStreamUrl.length() == 0 && !_deviceInitReceived)
        return;
    if (static_cast<long>(fng_now_ms() - _audioAutoConnectAtMs) < 0)
        return;
    // openAudioStream() closes first, which clears the arm. Re-arm on failure so a
    // bad endpoint retries on a timer instead of once per loop(); on success the
    // stream's own keep-alive reconnect takes over.
    if (!openAudioStream(FirmnginAudioConfig(), nullptr, _audioEventCallback))
    {
        _audioAutoConnect = true;
        _audioAutoConnectAtMs = fng_now_ms() + 5000;
    }
#endif
}

Firmngin &Firmngin::bindAudioEntity(const char *key, AudioStreamEventCallbackFunction handler)
{
    if (key != nullptr && key[0] != '\0')
        _audioEntityCallbacks[String(key)] = handler;
#if defined(ESP_PLATFORM)
    if (!_audioKeepAlive)
    {
        _audioAutoConnect = true;
        _audioAutoConnectAtMs = 0;
    }
#endif
    return *this;
}

void Firmngin::_dispatchAudioEntity(const FirmnginAudioEventMessage &message)
{
    if (_audioEntityCallbacks.empty())
        return;
    const char *key = message.entity_key[0] != '\0' ? message.entity_key : nullptr;
    if (key == nullptr)
    {
        // Device-wide event (ready/error/disconnect): every bound handler hears it.
        for (auto &entry : _audioEntityCallbacks)
            if (entry.second)
                entry.second(message.event, nullptr);
        return;
    }
    auto it = _audioEntityCallbacks.find(String(key));
    if (it == _audioEntityCallbacks.end() || !it->second)
        return;
    it->second(message.event, key);
}

bool Firmngin::openAudioStream(const char *url, const char *token, AudioChunkCallbackFunction onChunk,
                               AudioStreamEventCallbackFunction onEvent)
{
    FirmnginAudioConfig config;
    config.url = url;
    config.token = token;
    return openAudioStream(config, onChunk, onEvent);
}

#if defined(ESP_PLATFORM)
void Firmngin::_releaseAudioQueues()
{
    if (_audioRx != nullptr)
    {
        vStreamBufferDelete(_audioRx);
        _audioRx = nullptr;
    }
    if (_audioEvents != nullptr)
    {
        vQueueDelete(_audioEvents);
        _audioEvents = nullptr;
    }
    _audioPcmAccumLen = 0;
    _audioStreaming = false;
    _audioEndPending = false;
    _audioEndDetail[0] = '\0';
}

void Firmngin::_abortAudioClip()
{
    _audioFlushGen = _audioFlushGen + 1;
    _audioPcmAccumLen = 0;
    _audioStreaming = false;
    _audioEndPending = false;
    _audioEndDetail[0] = '\0';
    if (_audioRx != nullptr)
        xStreamBufferReset(_audioRx);
}

bool Firmngin::_enqueueAudioFrame(const uint8_t *frame, uint32_t flushGen)
{
    if (_audioRx == nullptr || frame == nullptr)
        return false;
    if (flushGen != _audioFlushGen)
        return false;
    // Non-blocking: the websocket task must keep reading. Blocking here while the
    // ring is full prevents audio.flush (manual stop) from being handled, the
    // gateway Write stalls, and the peer resets the TCP connection.
    return xStreamBufferSend(_audioRx, frame, AUDIO_FRAME_BYTES, 0) == AUDIO_FRAME_BYTES;
}

void Firmngin::_stopAudioWs()
{
    // Detach the handle first. A capture task that is between frames will now skip
    // its terminal control instead of racing a client that is being destroyed. A
    // send already in progress is bounded by its 1 s timeout and is covered by the
    // wait below.
    esp_websocket_client_handle_t ws = _audioWs;
    _audioWs = nullptr;
    _audioInputCancelRequested = true;
    _audioInputStopRequested = true;
    const unsigned long inputStopDeadline = fng_now_ms() + 1250;
    while (_audioInputTaskHandle != nullptr && static_cast<long>(fng_now_ms() - inputStopDeadline) < 0)
        fng_delay_ms(1);
    if (ws != nullptr)
    {
        esp_websocket_client_stop(ws);
        esp_websocket_client_destroy(ws);
    }
}

bool Firmngin::_waitForAudioClock()
{
    if (static_cast<unsigned long>(time(nullptr)) >= AUDIO_MIN_VALID_EPOCH)
        return true;
    const unsigned long waitStart = fng_now_ms();
    while (static_cast<unsigned long>(time(nullptr)) < AUDIO_MIN_VALID_EPOCH &&
           fng_now_ms() - waitStart < FIRMNGIN_NTP_SYNC_TIMEOUT_MS)
        fng_delay_ms(50);
    return static_cast<unsigned long>(time(nullptr)) >= AUDIO_MIN_VALID_EPOCH;
}

bool Firmngin::_buildAudioWsHeaders()
{
    _audioHeaders = "";
    if (_audioToken.length() > 0)
        _audioHeaders += String("Authorization: Bearer ") + _audioToken + "\r\n";

    if (_deviceId == nullptr || _deviceKey == nullptr ||
        strlen(_deviceId) == 0 || strlen(_deviceKey) == 0)
        return true;

    if (!_waitForAudioClock())
    {
        _notifyAudioEvent(AUDIO_ERROR, "device clock is not set; the stream connection cannot be signed");
        return false;
    }

    const unsigned long now = static_cast<unsigned long>(time(nullptr));
    const String path = audioStreamPath(_audioUrl);
    char message[AUDIO_AUTH_MESSAGE_MAX];
    if (path.length() == 0 ||
        !firmngin_audio_ws_auth_message(_deviceId, String(now).c_str(), path.c_str(), message, sizeof(message)))
    {
        _notifyAudioEvent(AUDIO_ERROR, "audio stream url has no usable path");
        return false;
    }
    _audioHeaders += String("X-Device-ID: ") + _deviceId + "\r\n";
    _audioHeaders += String("X-Device-Timestamp: ") + String(now) + "\r\n";
    _audioHeaders += String("X-Device-Signature: ") + firmngin_hmac_sha256_hex(_deviceKey, message) + "\r\n";
    return true;
}

bool Firmngin::_startAudioWsClient()
{
    if (!_buildAudioWsHeaders())
        return false;

    const bool isSecure = fng_isSecureWsScheme(_audioUrl);
    const char *activeServiceCA = _activeServiceCACert();
    if (isSecure && !_insecure && activeServiceCA == nullptr)
    {
        _notifyAudioEvent(AUDIO_ERROR, "service CA certificate not configured");
        return false;
    }

    esp_websocket_client_config_t wsConfig = {};
    wsConfig.uri = _audioUrl.c_str();
    wsConfig.headers = _audioHeaders.length() > 0 ? _audioHeaders.c_str() : nullptr;
    wsConfig.user_context = this;
    wsConfig.buffer_size = _audioWsBufferBytes;
    wsConfig.task_stack = _audioWsTaskStack;
    // Native auto-reconnect would reuse a frozen HMAC timestamp and fail after the
    // platform's skew window. Reconnect is owned by loop() with a freshly signed header.
    wsConfig.disable_auto_reconnect = true;
    if (isSecure)
    {
        wsConfig.cert_pem = _insecure ? nullptr : activeServiceCA;
        wsConfig.skip_cert_common_name_check = _insecure;
        if (_clientCert != nullptr)
            wsConfig.client_cert = _clientCert;
        if (_privateKey != nullptr)
            wsConfig.client_key = _privateKey;
    }

    _audioWs = esp_websocket_client_init(&wsConfig);
    if (_audioWs == nullptr)
    {
        _notifyAudioEvent(AUDIO_ERROR, "audio stream client init failed");
        return false;
    }
    esp_websocket_register_events(_audioWs, WEBSOCKET_EVENT_ANY, _audioWsEventHandler, this);
    if (esp_websocket_client_start(_audioWs) != ESP_OK)
    {
        _stopAudioWs();
        _notifyAudioEvent(AUDIO_ERROR, "audio stream connect failed");
        return false;
    }
    return true;
}
#endif

bool Firmngin::openAudioStream(const FirmnginAudioConfig &config, AudioChunkCallbackFunction onChunk,
                               AudioStreamEventCallbackFunction onEvent)
{
    // Copy strings before closeAudioStream(): callers may point config.url/token at a
    // String owned by this instance, while the native client keeps its own pointers.
    String resolvedURL = config.url != nullptr && config.url[0] != '\0'
                             ? String(config.url)
                             : _resolveAudioStreamUrl();
    String resolvedToken = config.token != nullptr
                               ? String(config.token)
                               : _runtimeAudioStreamToken;

    closeAudioStream();
    _audioChunkCallback = onChunk;
    _audioEventCallback = onEvent;
    _audioUsesBuiltinSink = false;

    // These checks answer before the socket exists, so they are reported straight
    // back: the caller is in setup()/loop(), never the socket task.
    const bool isSecure = fng_isSecureWsScheme(resolvedURL);
    if (resolvedURL.length() == 0)
    {
        _notifyAudioEvent(AUDIO_ERROR, "audio stream url is not configured");
        return false;
    }
    if (!isSecure && !fng_isPlainWsScheme(resolvedURL))
    {
        _notifyAudioEvent(AUDIO_ERROR, "audio stream url must use a websocket scheme");
        return false;
    }
    // Late hardware registration: begin() already ran, so start configured I2S now.
    for (auto &sink : _speakers)
        if (sink.i2s.pinsConfigured() && !sink.i2s.ready())
            sink.i2s.begin();
    if (!_audioChunkCallback && !_microphones.empty())
    {
        // An input-only device needs the same persistent WSS but has no playback
        // sink. Server output frames are ignored until a speaker is registered.
    }
    else if (!_audioChunkCallback)
    {
        if (!_anySinkReady())
        {
            _notifyAudioEvent(AUDIO_ERROR,
                              "audio chunk callback is required unless a speaker is ready");
            return false;
        }
        _audioChunkCallback = [this](const uint8_t *data, size_t len) {
            return _writeActiveSink(data, len);
        };
        _audioUsesBuiltinSink = true;
    }
    if (config.bufferFrames == 0 || config.framesPerLoop == 0 ||
        config.websocketBufferBytes < AUDIO_FRAME_BYTES || config.websocketTaskStack < 2048)
    {
        _notifyAudioEvent(AUDIO_ERROR, "audio stream config is invalid");
        return false;
    }
#if !defined(ESP_PLATFORM)
    _audioChunkCallback = nullptr;
    _audioEventCallback = nullptr;
    _notifyAudioEvent(AUDIO_ERROR, "audio streaming requires ESP32");
    return false;
#else
    _audioBufferFrames = config.bufferFrames;
    _audioFramesPerLoop = config.framesPerLoop;
    _audioWsBufferBytes = config.websocketBufferBytes;
    _audioWsTaskStack = config.websocketTaskStack;
    _audioUrl = resolvedURL;
    _audioToken = resolvedToken;
    _audioKeepAlive = true;
    _audioReconnectPending = false;
    _audioEndPending = false;

    _audioRx = xStreamBufferCreate(AUDIO_FRAME_BYTES * _audioBufferFrames, 1);
    _audioEvents = xQueueCreate(FIRMNGIN_AUDIO_EVENT_QUEUE_LEN, sizeof(FirmnginAudioEventMessage));
    if (_audioRx == nullptr || _audioEvents == nullptr)
    {
        _releaseAudioQueues();
        _audioKeepAlive = false;
        _notifyAudioEvent(AUDIO_ERROR, "audio stream buffers could not be allocated");
        return false;
    }

    if (!_startAudioWsClient())
    {
        _releaseAudioQueues();
        _audioKeepAlive = false;
        return false;
    }
    return true;
#endif
}

void Firmngin::closeAudioStream()
{
#if defined(ESP_PLATFORM)
    stopAudioInput();
    _audioKeepAlive = false;
    _audioReconnectPending = false;
    _audioAutoConnect = false;
    _audioAutoConnectAtMs = 0;
    _stopAudioWs();
    _releaseAudioQueues();
    _audioUrl = "";
    _audioHeaders = "";
    _audioToken = "";
#endif
    _audioChunkCallback = nullptr;
    _audioEventCallback = nullptr;
    _audioUsesBuiltinSink = false;
    _activeSinkKey = "";
}

bool Firmngin::isAudioStreamConnected() const
{
#if defined(ESP_PLATFORM)
    return _audioWs != nullptr && esp_websocket_client_is_connected(_audioWs);
#else
    return false;
#endif
}

EntityValue ActiveSession::entity(const char *key) const
{
    if (_instance == nullptr)
        return EntityValue();
    return _instance->entity(key);
}

EntityValue ActiveSession::entity(const Entity &entity) const
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
// The init payload announces every GPIO entity as {p: <pin>, v: <value>, t: <type>}.
// The wire key is the pin number (Entity(int)), so remember those values here:
// `entity(pin)` then reports the server's state right after a reconnect instead of
// staying empty until the first command or the first report from the sketch.
static void collectInitEntityValues(const String &entitiesJson, std::map<String, String> &out)
{
    const char *p = entitiesJson.c_str();
    const char *end = p + entitiesJson.length();
    while (p < end)
    {
        const char *objStart = strchr(p, '{');
        if (objStart == nullptr || objStart >= end)
            break;
        const char *objEnd = strchr(objStart, '}');
        if (objEnd == nullptr || objEnd >= end)
            break;
        firmngin_json::Parser entry(objStart, static_cast<size_t>(objEnd - objStart) + 1);
        if (entry.has("p") && entry.has("v"))
        {
            const int pin = entry.getInt("p", -1);
            char value[64] = {0};
            entry.getString("v", value, sizeof(value));
            if (pin >= 0)
                out[String(pin)] = String(value);
        }
        p = objEnd + 1;
    }
}

static String findInitAudioStreamUrl(const String &entitiesJson)
{
    // Every audio entity carries the device-wide stream URL in its value instead
    // of a pin. Accept input (t=5) as well as output (t=6), so a microphone-only
    // device can open the same persistent WSS. Entity values are scalars, so each
    // entry is a flat object.
    const char *p = entitiesJson.c_str();
    const char *end = p + entitiesJson.length();
    while (p < end)
    {
        const char *objStart = strchr(p, '{');
        if (objStart == nullptr || objStart >= end)
            break;
        const char *objEnd = strchr(objStart, '}');
        if (objEnd == nullptr || objEnd >= end)
            break;
        firmngin_json::Parser entry(objStart, static_cast<size_t>(objEnd - objStart) + 1);
        const int type = entry.getInt("t", 0);
        if (type == 5 || type == 6)
        {
            // current_value remains the device-wide WebSocket endpoint. The latest
            // recording is stored separately in audio_clip_url, so only accept an
            // actual ws(s) URL here.
            char url[FIRMNGIN_AUDIO_STREAM_URL_MAX];
            if (entry.getString("v", url, sizeof(url)) > 0 &&
                fng_isWsSchemeC(url))
                return String(url);
        }
        p = objEnd + 1;
    }
    return String();
}

Inits::Inits(const String &jsonPayload)
{
    _valid = false;
    _entitiesJson = "";
    _merchantStatus = "";
    _activeOrderId = "";
    _audioStreamUrl = "";
    _verificationFlag = 0;
    _modeFlag = 1;
    _kioskFlag = 0;
    _deviceLocalEndFlag = 0;
    _rawPayload = jsonPayload;

    firmngin_json::Parser p(jsonPayload.c_str(), jsonPayload.length());
    char buf[128];

    size_t rawLen = 0;
    const char *raw = p.getRaw("e", rawLen);
    if (raw != nullptr && rawLen > 0)
    {
        _entitiesJson = fng_substring(String(raw), 0, static_cast<int>(rawLen));
        _audioStreamUrl = findInitAudioStreamUrl(_entitiesJson);
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
    _modeFlag = p.getInt("md", 1);
    if (_modeFlag != 1 && _modeFlag != 2 && _modeFlag != 3)
        _modeFlag = 1;
    _kioskFlag = p.getInt("ki", 0);
    _deviceLocalEndFlag = p.getInt("dl", 0);
    if (_modeFlag != 2)
    {
        _kioskFlag = 0;
        _deviceLocalEndFlag = 0;
    }

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
    _gatewayTick();
#if defined(FIRMNGIN_FACTORY_SERIAL) && (FIRMNGIN_FACTORY_SERIAL != 0)
    if (!_securityReady)
    {
        FirmnginIdentity::pollSerialProvision();
        return;
    }
#endif

    if (!PLATFORM_SUPPORTED || !_securityReady || !_networkReady())
        return;

    _otaRollbackLoop();

    if (_ntpSyncStartMs > 0 && !_ntpSynced)
    {
        time_t now = time(nullptr);
        if (now >= 1609459200)
        {
            _ntpSynced = true;
        }
    }

    static unsigned long lastReconnectAttempt = 0;
    static unsigned long backoffDelay = 2000;

    _drainMqttInbox();
    if (!_mqttIsConnected())
    {
        if (!_mqttIsStarted())
        {
            unsigned long now = fng_now_ms();
            if (now - lastReconnectAttempt > backoffDelay)
            {
                lastReconnectAttempt = now;
                backoffDelay = std::min(backoffDelay * 2, 15000UL);
                if (connectServer())
                    backoffDelay = 2000UL;
            }
        }
    }
    else if (_queueEnabled && _queueFileReady && _queueCount > 0)
    {
        _drainQueue();
    }

    _processAudio();
    _processOTA();
    runActiveSessionHandlers();
    _cloudLogDrain();
}

void Firmngin::_otaRollbackSetup()
{
    _otaBootStartMs = fng_now_ms();
#if defined(ESP_PLATFORM)
    _otaRollbackReady = _otaPrefs.begin("fngin_ota", false);
    if (!_otaRollbackReady)
    {
        _Debug("OTA rollback: failed to open persistent state");
        return;
    }
    _otaLastOkVersion = _otaPrefs.getString("last_ok", "");
    _otaRollbackPending = _otaPrefs.getBool("pending", false);
    _otaRollbackBootCount = _otaPrefs.getUChar("boot_cnt", 0);
    _otaPendingPartition = _otaPrefs.getUInt("target_part", 0);
    _otaBootFirmwareID = _otaPrefs.getString("firmware_id", "");
    // A report survives reset until the QoS 1 outbox has drained. Never load a
    // completion while the candidate is still pending confirmation.
    _otaBootReportStatus = _otaPrefs.getString("boot_report", "");
    _otaBootReportMessage = _otaPrefs.getString("boot_message", "");
    if (_otaRollbackPending && _otaBootReportStatus == "completed")
    {
        _otaBootReportStatus = "";
        _otaBootReportMessage = "";
    }
    if (!_otaRollbackPending && _otaLastOkVersion.length() == 0 && _firmwareVersion.length() > 0)
        _otaLastOkVersion = _firmwareVersion;
#endif

    OtaRollbackState st;
    st.lastOkVersion = _otaLastOkVersion.c_str();
    st.pending = _otaRollbackPending;
    st.bootCount = _otaRollbackBootCount;

    OtaRollbackResult res = firmngin_ota_rollback_decide(_firmwareVersion.c_str(), st, 0, _otaRunningCandidatePartition());
    _otaRollbackBootCount = res.nextBootCount;
    if (res.clearPending)
        _otaRollbackPending = false;
    if (res.setLastOk)
        _otaLastOkVersion = _firmwareVersion;
    switch (res.action)
    {
    case OTA_RB_REPORT_ROLLED_BACK:
        _otaBootReportStatus = "rolled_back";
        _otaBootReportMessage = "Reverted to " + _firmwareVersion;
        break;
    case OTA_RB_REPORT_BOOT_FAILED:
        _otaBootReportStatus = "boot_failed";
        _otaBootReportMessage = "Firmware failed to boot " + String(OTA_ROLLBACK_MAX_BOOTS) + " times";
        break;
    default:
        break;
    }
    // Save a terminal report before clearing pending so a reset cannot lose it.
    if (_otaBootReportStatus.length() > 0 && !_persistOtaBootReport())
        _Debug("OTA rollback: failed to persist boot report");
    if (!_persistOtaRollbackState())
        _Debug("OTA rollback: failed to persist boot state");
}

int Firmngin::_otaRunningCandidatePartition() const
{
#if defined(ESP_PLATFORM)
    if (_otaPendingPartition != 0)
    {
        const esp_partition_t *running = esp_ota_get_running_partition();
        if (running != nullptr)
            return running->address == _otaPendingPartition ? 1 : 0;
    }
#endif
    return -1;
}

bool Firmngin::_persistOtaBootReport()
{
#if defined(ESP_PLATFORM)
    if (!_otaRollbackReady || _otaBootReportStatus.length() == 0)
        return false;
    // Write the status last; it is the commit marker for a complete report.
    if (_otaPrefs.putString("boot_message", _otaBootReportMessage) == 0)
        return false;
    return _otaPrefs.putString("boot_report", _otaBootReportStatus) > 0;
#else
    return true;
#endif
}

bool Firmngin::_clearOtaBootReport()
{
#if defined(ESP_PLATFORM)
    if (!_otaRollbackReady || !firmngin_clear_ota_boot_report(_otaPrefs))
        return false;
#endif
    _otaBootReportStatus = "";
    _otaBootReportMessage = "";
    _otaRollbackReportPublished = false;
    _otaBootReportRetryMs = 0;
    return true;
}

void Firmngin::_otaRollbackLoop()
{
#if defined(ESP_PLATFORM)
    if (!_otaRollbackReady)
        return;

    if (!_mqttIsConnected())
        _otaRollbackReportPublished = false;

    // Keep the report durable until MQTT QoS 1 delivery has drained the outbox.
    // A reset or reconnect before that point sends it again; the BE is idempotent.
    if (_otaBootReportStatus.length() > 0 && _mqttIsConnected())
    {
        if (!_otaRollbackReportPublished &&
            (_otaBootReportRetryMs == 0 || fng_now_ms() - _otaBootReportRetryMs >= 5000UL))
        {
            _otaBootReportRetryMs = fng_now_ms();
            const String activeFirmwareID = _otaFirmwareID;
            _otaFirmwareID = _otaBootFirmwareID;
            _otaRollbackReportPublished = publishOTAStatus(_otaBootReportStatus.c_str(), _otaBootReportMessage.c_str());
            _otaFirmwareID = activeFirmwareID;
        }
        else if (_otaRollbackReportPublished && _mqtt != nullptr &&
                 esp_mqtt_client_get_outbox_size(_mqtt) == 0)
        {
            _clearOtaBootReport();
        }
    }

    // Mark the running firmware valid once it has been stable for the grace period.
    if (_otaRollbackPending && !_otaRollbackMarkAttemptedThisBoot &&
        fng_now_ms() - _otaBootStartMs >= OTA_ROLLBACK_BOOT_OK_MS)
    {
        OtaRollbackState st;
        st.lastOkVersion = _otaLastOkVersion.c_str();
        st.pending = _otaRollbackPending;
        st.bootCount = _otaRollbackBootCount;

        OtaRollbackResult res = firmngin_ota_rollback_decide(_firmwareVersion.c_str(), st, fng_now_ms() - _otaBootStartMs, _otaRunningCandidatePartition());
        if (res.action == OTA_RB_MARK_VALID)
        {
            _otaRollbackMarkAttemptedThisBoot = true;
#if defined(ESP_PLATFORM)
            if (esp_ota_mark_app_valid_cancel_rollback() != ESP_OK)
            {
                _otaBootReportStatus = "failed";
                _otaBootReportMessage = "Firmware boot confirmation failed";
                _otaRollbackReportPublished = false;
                _otaBootReportRetryMs = 0;
                _persistOtaBootReport();
                _Debug("OTA rollback: failed to mark running firmware valid");
                return;
            }
#endif
            _otaBootReportStatus = "completed";
            _otaBootReportMessage = "Firmware boot confirmed";
            _otaRollbackReportPublished = false;
            _otaBootReportRetryMs = 0;
            if (!_persistOtaBootReport())
            {
                _otaBootReportStatus = "failed";
                _otaBootReportMessage = "Boot confirmation report could not be saved";
                _persistOtaBootReport();
                _Debug("OTA rollback: failed to persist boot confirmation report");
                return;
            }
            _otaRollbackPending = false;
            _otaLastOkVersion = _firmwareVersion;
            _otaRollbackBootCount = 0;
            if (!_persistOtaRollbackState())
            {
                _otaBootReportStatus = "failed";
                _otaBootReportMessage = "Rollback state could not be saved";
                _persistOtaBootReport();
                _Debug("OTA rollback: failed to persist confirmed firmware state");
                return;
            }
            _Debug("OTA rollback: running firmware marked valid");
        }
    }
#endif
}

bool Firmngin::_persistOtaRollbackState()
{
#if defined(ESP_PLATFORM)
    if (!_otaRollbackReady)
    {
        _otaRollbackReady = _otaPrefs.begin("fngin_ota", false);
        if (!_otaRollbackReady)
            return false;
    }
    const size_t versionBytes = _otaPrefs.putString("last_ok", _otaLastOkVersion);
    const size_t bootCountBytes = _otaPrefs.putUChar("boot_cnt", (uint8_t)_otaRollbackBootCount);
    if (versionBytes == 0 || bootCountBytes == 0)
        return false;
    if (_otaRollbackPending &&
        (_otaPrefs.putUInt("target_part", _otaPendingPartition) == 0 ||
         (_otaBootFirmwareID.length() > 0 && _otaPrefs.putString("firmware_id", _otaBootFirmwareID) == 0)))
        return false;
    // Commit pending only after its partition and artifact identity are durable.
    return _otaPrefs.putBool("pending", _otaRollbackPending) > 0;
#else
    return true;
#endif
}

void Firmngin::_Debug(String message, bool newLine)
{
    if (_debug)
    {
        if (newLine)
            Console.println(message);
        else
            Console.print(message);
    }
}

void Firmngin::printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buf[FNG_CLOUDLOG_LINE_MAX];
    const int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n <= 0)
        return;
    // n is the untruncated length; clamp to what actually fit in buf.
    size_t len = n < (int)sizeof(buf) ? static_cast<size_t>(n) : sizeof(buf) - 1;
    // printf lines end with Serial's newline; a trailing \n must close the
    // CloudLog line, otherwise the ring never receives a completed line and
    // loop() has nothing to publish.
    bool endsNewline = len > 0 && buf[len - 1] == '\n';
    if (endsNewline)
        --len;
    if (len > 0 && buf[len - 1] == '\r')
        --len;
    buf[len] = '\0';
    cloudlog.print(buf);
    if (endsNewline)
        cloudlog.println();
}

// Sink the CloudLog ring drains into. Rate limits the publish to once per
// second and refuses (order-preserving retry next drain) while the platform
// MQTT session is still handshaking. Lines go out E2EE-when-enabled, plaintext
// otherwise, same as every other publish; the offline persistent queue is
// deliberately bypassed — a dropped shell log is not a lost transaction.
bool Firmngin::_cloudLogSink(const char *line, unsigned long tsMs, void *user)
{
#if defined(ESP_PLATFORM)
    (void)user;
    if (!_mqttUp || !_mqtt || _mqttHelloPending)
        return false;
    unsigned long now = fng_now_ms();
    if (now - _cloudLogLastAttemptMs < 1000UL)
        return false;

    // One line per publish: {"t":<boot ms>,"l":"<json-escaped line>"} on
    // /d/{deviceId}/lg. ts is boot-relative (fng_now_ms) — the dashboard
    // correlates it with session start instead of trusting device clocks.
    char tsBuf[24];
    snprintf(tsBuf, sizeof(tsBuf), "%lu", static_cast<unsigned long>(tsMs));

    char buf[FNG_CLOUDLOG_LINE_MAX * 2 + 64];
    firmngin_json::Builder b(buf, sizeof(buf));
    b.reset();
    if (!b.addRaw("t", tsBuf))
        return false;
    char escaped[FNG_CLOUDLOG_LINE_MAX * 2];
    firmngin_json::_jsonEscape(line, escaped, sizeof(escaped));
    char quoted[FNG_CLOUDLOG_LINE_MAX * 2 + 2];
    const size_t escLen = strlen(escaped);
    quoted[0] = '"';
    memcpy(quoted + 1, escaped, escLen);
    quoted[escLen + 1] = '"';
    quoted[escLen + 2] = '\0';
    if (!b.addRaw("l", quoted))
        return false;
    if (!publishPayload(_cloudLogTopic.c_str(), b.build()))
        return false;
    _cloudLogLastAttemptMs = now;
    return true;
#else
    (void)line; (void)tsMs; (void)user; return false;
#endif
}

void Firmngin::_cloudLogDrain()
{
    _cloudLogSent = cloudlog.drain();
}

void Firmngin::setDebug(bool debug)
{
    _debug = debug;
}

void Firmngin::linkUp(bool up)
{
    _linkForced = true;
    _linkForcedState = up;
}

bool Firmngin::isPlatformSupported()
{
    return PLATFORM_SUPPORTED;
}


static void fillE2EENonce(uint8_t *iv, size_t len)
{
#if defined(ESP_PLATFORM)
    esp_fill_random(iv, len);
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

#if defined(ESP_PLATFORM)
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

#else
    (void)plaintext;
    (void)plainLen;
    (void)key;
    (void)keyLen;
    (void)packet;
    return false;
#endif
}

static bool decryptE2EE(const uint8_t *packet, size_t packetLen, const uint8_t *key, size_t keyLen, uint8_t *plaintext, size_t plainCapacity, size_t *plainLen)
{
    *plainLen = 0;

    // Minimum: 12 bytes nonce + 16 bytes tag
    if (packetLen < 28)
        return false;

    const uint8_t *iv = packet;
    size_t cipherLen = packetLen - 12 - 16;
    if (cipherLen > plainCapacity)
        return false;
    const uint8_t *ciphertext = packet + 12;
    const uint8_t *tag = packet + 12 + cipherLen;

#if defined(ESP_PLATFORM)
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

#else
    (void)iv;
    (void)ciphertext;
    (void)tag;
    return false;
#endif
}

void Firmngin::mqttCallback(char *path, uint8_t *payload, unsigned int length)
{
    String payloadStr;

    if (!_e2eeEnabled || _e2eeKeyLen == 0)
    {
        payloadStr.reserve(length);
        for (size_t i = 0; i < length; i++)
        {
            payloadStr += (char)payload[i];
        }
    }
    else
    {
        // Match the server transport bound without putting a large QR buffer on
        // the callback stack. decryptE2EE also receives the actual capacity.
        const String gatewayRoutes = String("/d/") + _deviceId + "/gw/routes";
        const unsigned int limit = _gatewayEnabled && strcmp(path, gatewayRoutes.c_str()) == 0 ? 65536U : 4096U;
        if (length < 28 || length > limit)
            return;
        const size_t plainCapacity = length - 28;
        std::unique_ptr<uint8_t[]> plainBuf(new (std::nothrow) uint8_t[plainCapacity ? plainCapacity : 1]);
        if (!plainBuf)
            return;
        size_t plainLen = 0;

        if (decryptE2EE(payload, length, _e2eeKeyBytes, _e2eeKeyLen, plainBuf.get(), plainCapacity, &plainLen))
        {
            payloadStr.reserve(plainLen);
            for (size_t i = 0; i < plainLen; i++)
            {
                payloadStr += (char)plainBuf[i];
            }
        }
        else
        {
            Console.println("E2EE: decryption failed — verify DECRYPTOR key in keys.h matches the server key");
            return;
        }
    }
    String pathStr = String(path);
    if (_gatewayHandle(pathStr, payloadStr)) return;
    String stateType = "";

    int lastSlash = fng_lastIndexOf(pathStr, '/');
    if (lastSlash >= 0)
    {
        stateType = fng_substring(pathStr, lastSlash + 1);
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

    if ((stateType == "pp" || stateType == "pm" || stateType == "pr") && (_paymentsCallback || !_paymentCallbacks.empty()))
    {
        Payments p(payloadStr);
        if (p.isValid())
        {
            p.setPending(stateType == "pp");
            p.setSuccess(stateType == "pm");
            p.setPaymentType(stateType == "pr");
            if (stateType == "pm" || stateType == "pr")
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
    else if (stateType == "pm" || stateType == "pr")
    {
        Payments p(payloadStr);
        if (p.isValid())
        {
            setCurrentOrder(p.orderId());
        }
    }

    if (stateType == "qr")
    {
        TerminalQR qr(payloadStr);
        if (qr.isValid())
        {
            for (const auto &callback : _terminalQRCallbacks)
                if (callback)
                    callback(qr);
            if (qr.isFailed())
                for (const auto &callback : _terminalPaymentFailedCallbacks)
                    if (callback)
                        callback(qr);
        }
    }

    if (stateType == "dp" && (_dispenseCallback || !_dispenseCallbacks.empty()))
    {
        Dispenses d(payloadStr);
        if (d.isValid())
        {
            if (_dispenseCallback)
            {
                _dispenseCallback(d);
            }
            for (const auto &callback : _dispenseCallbacks)
            {
                if (callback)
                {
                    callback(d);
                }
            }
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

    if (stateType == "init")
    {
        Inits i(payloadStr);
        if (i.isValid())
        {
            _deviceInitReceived = true;
            _applyAudioStreamUrl(i.audioStreamUrl());
            setMerchantStatus(i.merchantStatus());
            if (i.isOnActiveService() && i.activeOrderId().length() > 0)
            {
                setCurrentOrder(i.activeOrderId());
            }
            // Remember what the server announced before the sketch applies it: that
            // is the reconnect baseline `entity()` and reportAllEntities() use.
            collectInitEntityValues(i.entities(), _localEntityValues);
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

    // Auto-reply: echo back ping payload as pong
    if (stateType == "pi")
    {
        String pongPath = "/d/" + String(_deviceId) + "/" + PATH_PONG;
        _mqttPublish(pongPath.c_str(), reinterpret_cast<const uint8_t *>(payloadStr.c_str()), payloadStr.length(), false);
    }

    if (fng_indexOf(pathStr, "/ot/trg") >= 0)
    {
        if (!firmngin_ota_can_start(_otaAsyncState))
        {
            _Debug("OTA trigger ignored, download already in progress");
            return;
        }

        char firmwareID[48] = {0};
        char firmwareSHA256[65] = {0};
        firmngin_json::Parser trigger(payloadStr.c_str(), payloadStr.length());
        const size_t firmwareIDLength = trigger.getString("firmware_id", firmwareID, sizeof(firmwareID));
        const size_t firmwareSHALength = trigger.getString("sha256", firmwareSHA256, sizeof(firmwareSHA256));
        bool validFirmwareID = firmwareIDLength == 36;
        for (size_t i = 0; validFirmwareID && i < firmwareIDLength; ++i)
        {
            const char c = firmwareID[i];
            validFirmwareID = (c >= '0' && c <= '9') ||
                              (c >= 'a' && c <= 'f') ||
                              (c >= 'A' && c <= 'F') || c == '-';
        }
        bool validFirmwareSHA = firmwareSHALength == 64;
        for (size_t i = 0; validFirmwareSHA && i < firmwareSHALength; ++i)
        {
            const char c = firmwareSHA256[i];
            validFirmwareSHA = (c >= '0' && c <= '9') ||
                               (c >= 'a' && c <= 'f') ||
                               (c >= 'A' && c <= 'F');
        }
        if (!validFirmwareID || !validFirmwareSHA)
        {
            publishOTAStatus("failed", "Invalid OTA trigger payload");
            return;
        }

        _Debug("OTA signal received, performing OTA update...", true);
        _otaFirmwareID = String(firmwareID);
        _otaFirmwareSHA256 = String(firmwareSHA256);
        publishOTAStatus("triggered", "Remote OTA triggered by dashboard");
        performOTA();
    }

    if (fng_indexOf(pathStr, "/rs/") >= 0)
    {
        int rsIndex = fng_indexOf(pathStr, "/rs/");
        String entityKey = fng_substring(pathStr, rsIndex + 4); // after "/rs/"
        EntityCommand cmd(entityKey, payloadStr);
        if (_entityCallbacks.count(entityKey) > 0 || _localEntityValues.count(entityKey) > 0)
        {
            _localEntityValues[entityKey] = payloadStr;
        }

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

bool Firmngin::_gatewayPublish(const char *suffix, const String &payload)
{
#if defined(ESP_PLATFORM)
    if (!_gatewayEnabled || !_mqttUp || !_mqtt || _mqttHelloPending || !suffix || payload.length() == 0 || payload.length() + 28 > FIRMNGIN_E2EE_BUFFER_SIZE) return false;
    String path = String("/d/") + _deviceId + "/gw/" + suffix;
    String wire = payload;
    if (strcmp(suffix, "routes/request") == 0) {
        time_t now = time(nullptr);
        if (now < 1700000000) return false;
        struct tm stamp = {};
        gmtime_r(&now, &stamp);
        char observed[32];
        strftime(observed, sizeof(observed), "%Y-%m-%dT%H:%M:%SZ", &stamp);
        char request[192];
        snprintf(request, sizeof(request), "{\"v\":1,\"message_id\":\"%08lx-%08lx-%08lx\",\"observed_at\":\"%s\"}", static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(++_gatewaySequence), observed);
        wire = request;
    }
    if (wire.length() + 28 > FIRMNGIN_E2EE_BUFFER_SIZE) return false;
    if (!_e2eeEnabled || _e2eeKeyLen == 0) return false;
    uint8_t encrypted[FIRMNGIN_E2EE_BUFFER_SIZE];
    size_t length = 0;
    if (!encryptE2EE(reinterpret_cast<const uint8_t *>(wire.c_str()), wire.length(), _e2eeKeyBytes, _e2eeKeyLen, encrypted, &length)) return false;
    // QoS 0, non-retained, direct send: bypass persistent queue and MQTT outbox.
    return esp_mqtt_client_publish(_mqtt, path.c_str(), reinterpret_cast<const char *>(encrypted), static_cast<int>(length), 0, 0) >= 0;
#else
    (void)suffix; (void)payload; return false;
#endif
}

bool Firmngin::publishPayload(const char *path, const char *payload, bool retained)
{
    if (!_e2eeEnabled || _e2eeKeyLen == 0)
    {
        // E2EE disabled — send plaintext directly
        if (!_mqttIsConnected())
        {
            if (_queueEnabled)
                return _enqueueToQueue(path, (const uint8_t *)payload, strlen(payload), retained);
            return false;
        }
        bool published = _mqttPublish(path, reinterpret_cast<const uint8_t *>(payload), strlen(payload), retained);
        if (!published && _queueEnabled)
            return _enqueueToQueue(path, (const uint8_t *)payload, strlen(payload), retained);
        return published;
    }

    size_t payloadLen = strlen(payload);
    if (payloadLen + 28 > FIRMNGIN_E2EE_BUFFER_SIZE)
        return false;

    uint8_t encrypted[FIRMNGIN_E2EE_BUFFER_SIZE];
    size_t encryptedLen = 0;
    if (!encryptE2EE((const uint8_t *)payload, payloadLen, _e2eeKeyBytes, _e2eeKeyLen, encrypted, &encryptedLen))
        return false;

    if (!_mqttIsConnected())
    {
        if (_queueEnabled)
            return _enqueueToQueue(path, encrypted, encryptedLen, retained);
        return false;
    }

    bool published = _mqttPublish(path, encrypted, encryptedLen, retained);
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
    b.add("t", getTimestampMillis());

    return publishPayload(_topicUpdateEntity.c_str(), b.build());
}

bool Firmngin::pushEntity(const char *key, const String &value)
{
    return pushEntity(key, value.c_str());
}

bool Firmngin::pushEntity(const char *key, int value)
{
    String s = String(value);
    return pushEntity(key, s.c_str());
}

bool Firmngin::pushEntity(const char *key, unsigned long value)
{
    String s = String(value);
    return pushEntity(key, s.c_str());
}

bool Firmngin::pushEntity(const char *key, float value)
{
    String s = String(value);
    return pushEntity(key, s.c_str());
}

bool Firmngin::pushEntity(const char *key, double value)
{
    String s = String(value);
    return pushEntity(key, s.c_str());
}

bool Firmngin::pushEntity(const char *key, float value, int decimals)
{
    if (decimals < 0)
        decimals = 0;
    if (decimals > 6)
        decimals = 6;
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", decimals, value);
    return pushEntity(key, buf);
}

bool Firmngin::pushEntity(const char *key, double value, int decimals)
{
    if (decimals < 0)
        decimals = 0;
    if (decimals > 6)
        decimals = 6;
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", decimals, value);
    return pushEntity(key, buf);
}

bool Firmngin::pushEntity(const char *key, bool value)
{
    return pushEntity(key, value ? "1" : "0");
}

bool Firmngin::pushEntity(const Entity &entity, const char *value)
{
    return pushEntity(entity.key().c_str(), value);
}

bool Firmngin::pushEntity(const Entity &entity, const String &value)
{
    return pushEntity(entity.key().c_str(), value);
}

bool Firmngin::pushEntity(const Entity &entity, int value)
{
    String s = String(value);
    return pushEntity(entity.key().c_str(), s.c_str());
}

bool Firmngin::pushEntity(const Entity &entity, unsigned long value)
{
    String s = String(value);
    return pushEntity(entity.key().c_str(), s.c_str());
}

bool Firmngin::pushEntity(const Entity &entity, float value)
{
    String s = String(value);
    return pushEntity(entity.key().c_str(), s.c_str());
}

bool Firmngin::pushEntity(const Entity &entity, double value)
{
    String s = String(value);
    return pushEntity(entity.key().c_str(), s.c_str());
}

bool Firmngin::pushEntity(const Entity &entity, float value, int decimals)
{
    return pushEntity(entity.key().c_str(), value, decimals);
}

bool Firmngin::pushEntity(const Entity &entity, double value, int decimals)
{
    return pushEntity(entity.key().c_str(), value, decimals);
}

bool Firmngin::pushEntity(const Entity &entity, bool value)
{
    return pushEntity(entity.key().c_str(), value ? "1" : "0");
}

bool Firmngin::updateEntities(const char *jsonPayload)
{
    if (_topicUpdateEntities.length() == 0)
        return false;

    // Wrap array payload with timestamp: {"t":<ms>,"items":[...]}
    char buf[FIRMNGIN_BATCH_BUFFER_SIZE + 32];
    firmngin_json::Builder b(buf, sizeof(buf));
    b.reset();
    if (!b.add("t", getTimestampMillis()) || !b.addRaw("items", jsonPayload))
        return false;
    return publishPayload(_topicUpdateEntities.c_str(), b.build());
}

bool Firmngin::reportAllEntities()
{
    if (_topicUpdateEntities.length() == 0 || _localEntityValues.empty())
        return false;

    // After a reboot the server and the apps still hold the pre-crash value, and
    // init only tells the device what it believes. Reporting everything this
    // sketch actually applied is what makes the two agree again.
    char buffer[FIRMNGIN_BATCH_BUFFER_SIZE];
    firmngin_json::ArrayBuilder b(buffer, sizeof(buffer));
    b.reset();
    for (const auto &entry : _localEntityValues)
    {
        if (!b.add(entry.first.c_str(), entry.second.c_str()))
            return false; // never publish a truncated array
    }
    return updateEntities(b.build());
}

EntityValue Firmngin::entity(const char *key)
{
    String entityKey(key);
    if (_localEntityValues.count(entityKey) == 0)
        return EntityValue();
    return EntityValue(_localEntityValues[entityKey]);
}

EntityValue Firmngin::entity(const Entity &entity)
{
    return this->entity(entity.key().c_str());
}

bool Firmngin::endSession()
{
    if (_currentOrderId.length() == 0 || _merchantStatus != "on_active_service")
        return false;

    unsigned long now = fng_now_ms();
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
    b.add("t", getTimestampMillis());

    bool sent = publishPayload(getPathSessionEnd(_deviceId).c_str(), b.build());
    if (sent)
    {
        _sessionEndRequested = true;
    }
    return sent;
}

// Returns nullptr once the request is sent, otherwise the reason it was not:
// a short literal the sketch can print as-is.
const char *Firmngin::setAmountTerminalQR(const String &amount)
{
    if (amount.length() == 0)
        return "amount is empty";
    for (size_t i = 0; i < amount.length(); ++i)
    {
        if (amount[i] < '0' || amount[i] > '9')
            return "digits only";
    }
    _terminalRequestSeq++;
    String requestId = String("dev-") + String(getTimestampMillis()) + "-" + String(_terminalRequestSeq);
    char buf[384];
    firmngin_json::Builder b(buf, sizeof(buf));
    b.reset();
    if (!b.add("rid", requestId.c_str()) || !b.add("amt", amount.c_str()) || b.full())
        return "amount too long";
    String topic = String("/d/") + _deviceId + "/qr";
    return publishPayload(topic.c_str(), b.build()) ? nullptr : "Failed to sent message";
}

bool Firmngin::cancelTerminalQR()
{
    String topic = String("/d/") + _deviceId + "/a/qr-cancel";
    return publishPayload(topic.c_str(), "");
}

bool Firmngin::requestInit()
{
    char buf[64];
    firmngin_json::Builder b(buf, sizeof(buf));
    b.reset();
    b.add("t", getTimestampMillis());
    return publishPayload(getPathRequestInit(_deviceId).c_str(), b.build());
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
    if (_mqttIsConnected())
    {
        syncFirmwareInfo();
    }
}

void Firmngin::setFirmwareInfo(const char *version)
{
    if (version != nullptr)
        _firmwareVersion = version;

    // Auto-sync to server if already connected.
    if (_mqttIsConnected())
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
    if (!_mqttIsConnected())
    {
        if (_debug)
        {
            Console.println("[firmngin] syncFirmwareInfo: skipped, not connected");
        }
        return false;
    }

    if (_firmwareVersion == _lastSyncedFirmwareVersion)
    {
        if (_debug)
        {
            Console.println("[firmngin] syncFirmwareInfo: skipped, same version");
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
        Console.print("Syncing Firmware Info: V");
        Console.print(_firmwareVersion);
        Console.print(", Target Board: ");
        Console.print(_firmwareTargetBoard);
        Console.print(", Model: ");
        Console.println(_firmwareTargetModel);
    }

    return ok;
}

#if defined(ESP_PLATFORM)


#endif


// ==================== AUDIO STREAM ====================
#if defined(ESP_PLATFORM)
void Firmngin::_audioWsEventHandler(void *handlerArgs, esp_event_base_t, int32_t eventId, void *eventData)
{
    Firmngin *self = static_cast<Firmngin *>(handlerArgs);
    if (self == nullptr)
        return;

    switch (eventId)
    {
    case WEBSOCKET_EVENT_DATA:
    {
        esp_websocket_event_data_t *data = static_cast<esp_websocket_event_data_t *>(eventData);
        if (data == nullptr || data->data_len <= 0)
            break;
        if (data->op_code == 0x01)
        {
            if (data->payload_offset != 0)
                break;
            char entityKey[AUDIO_DETAIL_MAX];
            FirmnginAudioEvent event = firmngin_audio_control_event(
                data->data_ptr, static_cast<size_t>(data->data_len), entityKey, sizeof(entityKey));
            if (event == AUDIO_NONE)
                break;
            if (event == AUDIO_START)
            {
                // Binary frames are only legal inside a started clip. Gating on this
                // flag is what makes a stop a stop: frames still sitting in TCP and
                // socket buffers after audio.flush must not re-enter the ring and
                // resurrect the clip the user just stopped.
                self->_audioStreaming = true;
                self->_audioEndPending = false;
                self->_audioEndDetail[0] = '\0';
            }
            if (event == AUDIO_END)
            {
                self->_audioStreaming = false;
                self->_audioPcmAccumLen = 0;
                self->_audioEndPending = true;
                if (entityKey[0] != '\0')
                {
                    strncpy(self->_audioEndDetail, entityKey, sizeof(self->_audioEndDetail) - 1);
                    self->_audioEndDetail[sizeof(self->_audioEndDetail) - 1] = '\0';
                }
            }
            if (event == AUDIO_FLUSH)
                self->_abortAudioClip();
            if (event == AUDIO_INPUT_ERROR)
            {
                self->_audioInputCancelRequested = true;
                self->_audioInputStopRequested = true;
            }
            self->_notifyAudioEvent(event, nullptr, entityKey[0] != '\0' ? entityKey : nullptr);
            break;
        }
        if (data->op_code == 0x02 && self->_audioRx != nullptr && self->_audioStreaming)
        {
            if (data->payload_len != AUDIO_FRAME_BYTES)
            {
                self->_audioPcmAccumLen = 0;
                self->_notifyAudioEvent(AUDIO_ERROR, "audio PCM frame must be 640 bytes");
                break;
            }
            if (self->_audioPcmAccumLen + data->data_len > sizeof(self->_audioPcmAccum))
            {
                self->_audioPcmAccumLen = 0;
                self->_notifyAudioEvent(AUDIO_ERROR, "audio PCM reassembly overflow");
                break;
            }
            memcpy(self->_audioPcmAccum + self->_audioPcmAccumLen, data->data_ptr, data->data_len);
            self->_audioPcmAccumLen += data->data_len;

            const uint32_t flushGen = self->_audioFlushGen;
            while (self->_audioPcmAccumLen >= AUDIO_FRAME_BYTES)
            {
                if (flushGen != self->_audioFlushGen)
                {
                    self->_audioPcmAccumLen = 0;
                    break;
                }
                if (!self->_enqueueAudioFrame(self->_audioPcmAccum, flushGen))
                {
                    // Ring full: drop this frame and return to the event loop so
                    // audio.flush/stop can be read. Do not raise AUDIO_ERROR — with
                    // a paced gateway this is rare; treating it as an error made
                    // every eager flood look like a failed clip.
                    self->_audioPcmAccumLen -= AUDIO_FRAME_BYTES;
                    if (self->_audioPcmAccumLen > 0)
                        memmove(self->_audioPcmAccum,
                                self->_audioPcmAccum + AUDIO_FRAME_BYTES,
                                self->_audioPcmAccumLen);
                    break;
                }
                self->_audioPcmAccumLen -= AUDIO_FRAME_BYTES;
                if (self->_audioPcmAccumLen > 0)
                    memmove(self->_audioPcmAccum,
                            self->_audioPcmAccum + AUDIO_FRAME_BYTES,
                            self->_audioPcmAccumLen);
            }
        }
        break;
    }
    case WEBSOCKET_EVENT_DISCONNECTED:
        self->_abortAudioClip();
        self->_audioInputCancelRequested = true;
        self->_audioInputStopRequested = true;
        if (self->_audioKeepAlive)
        {
            self->_audioReconnectPending = true;
            self->_audioReconnectAtMs = fng_now_ms() + 2000;
        }
        self->_notifyAudioEvent(AUDIO_DISCONNECTED, "audio stream disconnected");
        break;
    case WEBSOCKET_EVENT_ERROR:
        self->_audioInputCancelRequested = true;
        self->_audioInputStopRequested = true;
        if (self->_audioKeepAlive)
        {
            self->_audioReconnectPending = true;
            self->_audioReconnectAtMs = fng_now_ms() + 2000;
        }
        self->_notifyAudioEvent(AUDIO_ERROR, "audio stream error");
        break;
    default:
        break;
    }
}
#endif

void Firmngin::_notifyAudioEvent(FirmnginAudioEvent event, const char *detail, const char *entityKey)
{
#if defined(ESP_PLATFORM)
    if (_audioEvents != nullptr)
    {
        FirmnginAudioEventMessage message = {};
        message.event = event;
        if (detail != nullptr)
            strncpy(message.detail, detail, sizeof(message.detail) - 1);
        if (entityKey != nullptr)
            strncpy(message.entity_key, entityKey, sizeof(message.entity_key) - 1);
        if (xQueueSend(_audioEvents, &message, 0) == pdTRUE)
            return;
    }
#endif
    FirmnginAudioEventMessage direct = {};
    direct.event = event;
    if (detail != nullptr)
        strncpy(direct.detail, detail, sizeof(direct.detail) - 1);
    if (entityKey != nullptr)
        strncpy(direct.entity_key, entityKey, sizeof(direct.entity_key) - 1);
    _dispatchAudioEntity(direct);
    if (_audioEventCallback)
    {
        _audioEventCallback(event, detail);
        return;
    }
    if (_audioEntityCallbacks.empty() && event == AUDIO_ERROR)
        Console.printf("[firmngin] audio error: %s\n", detail != nullptr ? detail : "unknown");
}

void Firmngin::_processAudio()
{
#if !defined(ESP_PLATFORM)
    return;
#else
    _tryAutoConnectAudio();

    if (_audioReconnectPending && _audioKeepAlive && _hasAudioInterfaceConfigured() &&
        static_cast<long>(fng_now_ms() - _audioReconnectAtMs) >= 0)
    {
        _audioReconnectPending = false;
        _stopAudioWs();
        if (!_startAudioWsClient())
        {
            _audioReconnectPending = true;
            _audioReconnectAtMs = fng_now_ms() + 5000;
        }
    }

    if (_audioEvents != nullptr)
    {
        FirmnginAudioEventMessage message;
        while (xQueueReceive(_audioEvents, &message, 0) == pdTRUE)
        {
            if ((message.event == AUDIO_FLUSH || message.event == AUDIO_DISCONNECTED) &&
                _audioUsesBuiltinSink)
            {
                FirmnginI2s *sink = _activeSink();
                // A flush before the first clip has no selected speaker yet. That is
                // a valid no-op; only a selected sink that fails to flush is an error.
                if (sink != nullptr && !sink->flush())
                    _notifyAudioEvent(AUDIO_ERROR, "audio I2S flush failed");
            }

            if (message.event == AUDIO_START)
            {
                _activeSinkKey = message.entity_key;
                if (_audioChunkCallback == nullptr ||
                    (_audioUsesBuiltinSink && _activeSink() == nullptr))
                {
                    // Stop accepting this clip immediately. Binary frames already
                    // queued behind audio.start are discarded by _abortAudioClip().
                    _abortAudioClip();
                    _activeSinkKey = "";
                    _notifyAudioEvent(AUDIO_ERROR, "audio output entity is not registered",
                                      message.entity_key[0] != '\0' ? message.entity_key : nullptr);
                    continue;
                }
            }
            else if (message.event == AUDIO_READY)
                _activeSinkKey = "";
            else if (message.event == AUDIO_FLUSH ||
                     message.event == AUDIO_DISCONNECTED)
                _activeSinkKey = "";

            _dispatchAudioEntity(message);

            // Defer user AUDIO_END until the ring has drained at full level.
            if (message.event == AUDIO_END)
            {
                const char *endTag = message.detail[0] != '\0' ? message.detail : message.entity_key;
                if (endTag[0] != '\0')
                {
                    strncpy(_audioEndDetail, endTag, sizeof(_audioEndDetail) - 1);
                    _audioEndDetail[sizeof(_audioEndDetail) - 1] = '\0';
                }
                _audioEndPending = true;
                continue;
            }
            if (message.event == AUDIO_FLUSH || message.event == AUDIO_START ||
                message.event == AUDIO_DISCONNECTED)
            {
                _audioEndPending = false;
                _audioEndDetail[0] = '\0';
            }

            const char *eventDetail = message.detail[0] != '\0'
                                          ? message.detail
                                          : (message.entity_key[0] != '\0' ? message.entity_key : nullptr);
            if (_audioEventCallback)
                _audioEventCallback(message.event, eventDetail);
            else if (_audioEntityCallbacks.empty() && message.event == AUDIO_ERROR)
                Console.printf("[firmngin] audio error: %s\n",
                              eventDetail != nullptr ? eventDetail : "unknown");
        }
    }

    if (_audioChunkCallback == nullptr || _audioRx == nullptr)
        return;

    static uint8_t mono[AUDIO_FRAME_BYTES];
    static uint8_t stereo[AUDIO_PLAYBACK_FRAME_BYTES];
    const size_t queuedFrames =
        xStreamBufferBytesAvailable(_audioRx) / AUDIO_FRAME_BYTES;
    uint8_t maxPasses = _audioFramesPerLoop;
    // Drain faster when ending or the ring is filling so loop() jitter does not
    // underrun I2S or leave a long tail after audio.end.
    if (_audioEndPending || queuedFrames > (_audioBufferFrames / 2))
        maxPasses = _audioBufferFrames;

    for (uint8_t pass = 0; pass < maxPasses; pass++)
    {
        if (xStreamBufferBytesAvailable(_audioRx) < AUDIO_FRAME_BYTES)
            break;
        const bool finalFrame = _audioEndPending &&
                                xStreamBufferBytesAvailable(_audioRx) == AUDIO_FRAME_BYTES;
        if (xStreamBufferReceive(_audioRx, mono, sizeof(mono), 0) != sizeof(mono))
        {
            xStreamBufferReset(_audioRx);
            return;
        }
        if (finalFrame)
        {
            uint32_t fadePos = 0;
            firmngin_audio_fade_out_s16le(mono, sizeof(mono), &fadePos,
                                          AUDIO_SAMPLES_PER_FRAME);
        }
        if (firmngin_audio_mono_s16le_to_stereo(mono, sizeof(mono), stereo, sizeof(stereo)) !=
            sizeof(stereo))
        {
            xStreamBufferReset(_audioRx);
            _notifyAudioEvent(AUDIO_ERROR, "audio PCM conversion failed");
            return;
        }
        if (!_audioChunkCallback(stereo, sizeof(stereo)))
        {
            xStreamBufferReset(_audioRx);
            return;
        }
    }

    if (_audioEndPending && xStreamBufferBytesAvailable(_audioRx) < AUDIO_FRAME_BYTES)
    {
        _audioEndPending = false;
        static uint8_t silence[AUDIO_PLAYBACK_FRAME_BYTES];
        memset(silence, 0, sizeof(silence));
        for (uint8_t i = 0; i < FIRMNGIN_AUDIO_TAIL_SILENCE_FRAMES; i++)
            (void)_audioChunkCallback(silence, sizeof(silence));
        if (_audioEventCallback)
            _audioEventCallback(AUDIO_END,
                                _audioEndDetail[0] != '\0' ? _audioEndDetail : nullptr);
        _audioEndDetail[0] = '\0';
    }
#endif
}

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
        Console.print("[CAMERA] Uploading ");
        Console.print(len);
        Console.print(" bytes as ");
        Console.println(contentType);
    }

#if defined(ESP_PLATFORM)
    String url = _apiBaseUrl + "/device-camera/upload";

    unsigned long ts = time(nullptr);
    String path = "/device-camera/upload";
    String message = String(_deviceId) + "." + String(ts) + ".POST." + path;
    String signature = firmngin_hmac_sha256_hex(_deviceKey, message.c_str());
    const char *activeServiceCA = _activeServiceCACert();

    const char *securityError = firmngin_http_security_error(
        _insecure, activeServiceCA != nullptr, fng_http_insecure_supported());
    if (securityError != nullptr)
    {
        if (onError)
            onError(0, securityError);
        return false;
    }

    esp_http_client_config_t config = fng_http_config(url.c_str(), activeServiceCA, _clientCert, _privateKey, _insecure);
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr)
    {
        if (onError)
            onError(0, "HTTP client init failed");
        return false;
    }

    // Multipart form: a fixed boundary is safe here because entity-key values and
    // JPEG bytes never contain it.
    String boundary = "----FirmnginBoundary" + String(fng_now_ms());
    char contentTypeHeaderBuf[96];
    snprintf(contentTypeHeaderBuf, sizeof(contentTypeHeaderBuf), "multipart/form-data; boundary=%s", boundary.c_str());

    String part1 = "";
    part1 += "--" + boundary + "\r\n";
    part1 += "Content-Disposition: form-data; name=\"entity_key\"\r\n\r\n";
    part1 += String(entityKey) + "\r\n";
    part1 += "--" + boundary + "\r\n";
    part1 += "Content-Disposition: form-data; name=\"image\"; filename=\"capture.jpg\"\r\n";
    part1 += "Content-Type: " + String(contentType) + "\r\n\r\n";
    String part3 = "\r\n--" + boundary + "--\r\n";

    size_t totalLen = part1.length() + len + part3.length();

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "X-Device-ID", _deviceId);
    char tsBuf[24];
    snprintf(tsBuf, sizeof(tsBuf), "%lu", ts);
    esp_http_client_set_header(client, "X-Device-Timestamp", tsBuf);
    esp_http_client_set_header(client, "X-Device-Signature", signature.c_str());
    esp_http_client_set_header(client, "Content-Type", contentTypeHeaderBuf);

    bool sentOk = false;
    int statusCode = 0;
    if (esp_http_client_open(client, static_cast<int>(totalLen)) == ESP_OK)
    {
        int writeErr = 0;
        // Fixed-content-length upload: write part1, raw image, then footer.
        if (esp_http_client_write(client, part1.c_str(), static_cast<int>(part1.length())) != static_cast<int>(part1.length()))
            writeErr = 1;
        if (writeErr == 0 && esp_http_client_write(client, reinterpret_cast<const char *>(data), static_cast<int>(len)) != static_cast<int>(len))
            writeErr = 2;
        if (writeErr == 0 && esp_http_client_write(client, part3.c_str(), static_cast<int>(part3.length())) != static_cast<int>(part3.length()))
            writeErr = 3;
        if (writeErr == 0)
        {
            sentOk = esp_http_client_fetch_headers(client) >= 0;
            if (sentOk)
            {
                statusCode = esp_http_client_get_status_code(client);
                sentOk = statusCode == 200;
            }
        }
    }

    String response;
    FirmnginHttpBodyResult bodyResult = FirmnginHttpBodyResult::Incomplete;
    if (sentOk)
    {
        bodyResult = firmngin_read_http_body(
            [client](char *data, size_t capacity) {
                return esp_http_client_read(client, data, static_cast<int>(capacity));
            },
            [client]() { return esp_http_client_is_complete_data_received(client); },
            response, 4096);
        sentOk = bodyResult == FirmnginHttpBodyResult::Complete;
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (!sentOk)
    {
        if (onError)
            onError(statusCode, bodyResult == FirmnginHttpBodyResult::TooLarge
                           ? "upload response exceeds 4096 bytes"
                           : "upload request or response incomplete");
        return false;
    }

    if (_debug)
    {
        Console.print("[CAMERA] HTTP 200, response: ");
        Console.println(response);
    }
    if (onSuccess)
        onSuccess(response.c_str());
    return true;

#else
    if (onError)
        onError(0, "platform not supported");
    return false;
#endif
}

LocationUpdate Firmngin::pushLocation()
{
    return LocationUpdate(this);
}

BatchState Firmngin::pushBatchEntities()
{
    return BatchState(this);
}

LocationUpdate::LocationUpdate(Firmngin *instance)
    : _instance(instance),
      _builder(_buffer, sizeof(_buffer)),
      _lat(0.0),
      _lon(0.0),
      _accuracy(0.0f),
      _alt(0.0f),
      _speed(0.0f),
      _hasLat(false),
      _hasLon(false),
      _hasAccuracy(false),
      _hasAlt(false),
      _hasSpeed(false),
      _sent(false)
{
    _builder.reset();
}

LocationUpdate::~LocationUpdate()
{
    if (!_sent)
        _dispatch();
}

LocationUpdate::LocationUpdate(LocationUpdate &&other) noexcept
    : _instance(other._instance),
      _builder(_buffer, sizeof(_buffer)),
      _lat(other._lat),
      _lon(other._lon),
      _accuracy(other._accuracy),
      _alt(other._alt),
      _speed(other._speed),
      _hasLat(other._hasLat),
      _hasLon(other._hasLon),
      _hasAccuracy(other._hasAccuracy),
      _hasAlt(other._hasAlt),
      _hasSpeed(other._hasSpeed),
      _sent(false)
{
    _builder.reset();
    other._sent = true;
}

LocationUpdate &LocationUpdate::operator=(LocationUpdate &&other) noexcept
{
    if (this != &other)
    {
        _instance = other._instance;
        _lat = other._lat;
        _lon = other._lon;
        _accuracy = other._accuracy;
        _alt = other._alt;
        _speed = other._speed;
        _hasLat = other._hasLat;
        _hasLon = other._hasLon;
        _hasAccuracy = other._hasAccuracy;
        _hasAlt = other._hasAlt;
        _hasSpeed = other._hasSpeed;
        _sent = false;
        other._sent = true;
    }
    return *this;
}

LocationUpdate &LocationUpdate::lat(double value)
{
    _lat = value;
    _hasLat = true;
    return *this;
}

LocationUpdate &LocationUpdate::lon(double value)
{
    _lon = value;
    _hasLon = true;
    return *this;
}

LocationUpdate &LocationUpdate::accuracy(float value)
{
    if (!isnan(value) && value >= 0.0f)
    {
        _accuracy = value;
        _hasAccuracy = true;
    }
    return *this;
}

LocationUpdate &LocationUpdate::alt(float value)
{
    if (!isnan(value))
    {
        _alt = value;
        _hasAlt = true;
    }
    return *this;
}

LocationUpdate &LocationUpdate::speed(float value)
{
    if (!isnan(value) && value >= 0.0f)
    {
        _speed = value;
        _hasSpeed = true;
    }
    return *this;
}

bool LocationUpdate::_dispatch()
{
    if (_sent || _instance == nullptr || !_hasLat || !_hasLon)
        return false;

    if (_lat < -90.0 || _lat > 90.0 || _lon < -180.0 || _lon > 180.0)
        return false;

    _builder.reset();

    char valueBuf[24];
    snprintf(valueBuf, sizeof(valueBuf), "%.6f", _lat);
    if (!_builder.add(FIRMNGIN_ENTITY_KEY_LAT, valueBuf))
        return false;

    snprintf(valueBuf, sizeof(valueBuf), "%.6f", _lon);
    if (!_builder.add(FIRMNGIN_ENTITY_KEY_LON, valueBuf))
        return false;

    if (_hasAccuracy)
    {
        snprintf(valueBuf, sizeof(valueBuf), "%.2f", _accuracy);
        if (!_builder.add(FIRMNGIN_ENTITY_KEY_ACCURACY, valueBuf))
            return false;
    }

    if (_hasAlt)
    {
        snprintf(valueBuf, sizeof(valueBuf), "%.2f", _alt);
        if (!_builder.add(FIRMNGIN_ENTITY_KEY_ALT, valueBuf))
            return false;
    }

    if (_hasSpeed)
    {
        snprintf(valueBuf, sizeof(valueBuf), "%.2f", _speed);
        if (!_builder.add(FIRMNGIN_ENTITY_KEY_SPEED, valueBuf))
            return false;
    }

    _sent = true;
    return _instance->updateEntities(_builder.build());
}
