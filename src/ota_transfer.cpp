#include "ota_transfer.h"
#include "hmac.h"
#include "http_response.h"
#include <esp_http_client.h>
#include <mbedtls/base64.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <freertos/task.h>
#include <algorithm>
#include <time.h>
#include <vector>
#include <type_traits>
#include <cassert>

static_assert(std::is_trivially_copyable<FirmnginOtaEvent>::value,
              "FreeRTOS events must contain fixed-size copied data");

namespace
{
struct HttpClient
{
    esp_http_client_handle_t handle = nullptr;
    ~HttpClient() { if (handle != nullptr) esp_http_client_cleanup(handle); }
};
struct Sha256
{
    mbedtls_md_context_t context;
    Sha256() { mbedtls_md_init(&context); }
    ~Sha256() { mbedtls_md_free(&context); }
};
struct FlashWriter
{
    esp_ota_handle_t handle = 0;
    ~FlashWriter() { if (handle != 0) esp_ota_abort(handle); }
    bool finish()
    {
        const esp_err_t result = esp_ota_end(handle);
        handle = 0; // esp_ota_end consumes its handle, including on error.
        return result == ESP_OK;
    }
};
struct SignatureHeaders
{
    String signature, keyHash;
    bool invalid = false;
    static esp_err_t event(esp_http_client_event_t *event)
    {
        if (event->event_id != HTTP_EVENT_ON_HEADER || !event->user_data ||
            !event->header_key || !event->header_value)
            return ESP_OK;
        auto &headers = *static_cast<SignatureHeaders *>(event->user_data);
        if (otaHeaderMatches(event->header_key, "X-Firmware-Signature"))
        {
            if (strlen(event->header_value) > 4096) headers.invalid = true;
            else headers.signature = event->header_value;
        }
        else if (otaHeaderMatches(event->header_key, "X-Firmware-Key-Sha256"))
        {
            if (strlen(event->header_value) > 64) headers.invalid = true;
            else headers.keyHash = event->header_value;
        }
        return ESP_OK;
    }
};

bool parsePublicKey(const String &pem, mbedtls_pk_context &key)
{
    if (pem.length() == 0) return false;
    mbedtls_pk_init(&key);
    if (mbedtls_pk_parse_public_key(&key,
            reinterpret_cast<const unsigned char *>(pem.c_str()), pem.length() + 1) != 0 ||
        !mbedtls_pk_can_do(&key, MBEDTLS_PK_RSA) || mbedtls_pk_get_bitlen(&key) < 2048)
    {
        mbedtls_pk_free(&key);
        return false;
    }
    return true;
}

String publicKeyHash(const String &pem)
{
    mbedtls_pk_context key;
    if (!parsePublicKey(pem, key)) return String();
    std::vector<unsigned char> der(mbedtls_pk_get_len(&key) * 2 + 128);
    const int length = mbedtls_pk_write_pubkey_der(&key, der.data(), der.size());
    mbedtls_pk_free(&key);
    unsigned char digest[32];
    if (length <= 0 || mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
            der.data() + der.size() - length, length, digest) != 0)
        return String();
    char hex[65];
    for (size_t i = 0; i < sizeof(digest); ++i)
        snprintf(hex + 2 * i, 3, "%02x", digest[i]);
    return String(hex);
}

const char *verifySignature(const String &pem, const uint8_t digest[32], const String &encoded)
{
    if (encoded.length() == 0 || encoded.length() > 4096)
        return "OTA_SIGNATURE_INVALID: Firmware publisher signature is missing or invalid";
    mbedtls_pk_context key;
    if (!parsePublicKey(pem, key))
        return "OTA_PUBLISHER_KEY_INVALID: Device publisher verification key is invalid";
    const size_t expected = mbedtls_pk_get_len(&key);
    std::vector<unsigned char> signature(expected);
    size_t length = 0;
    const int decoded = mbedtls_base64_decode(signature.data(), signature.size(), &length,
        reinterpret_cast<const unsigned char *>(encoded.c_str()), encoded.length());
    const char *failure = nullptr;
    if (decoded == MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL || (decoded == 0 && length != expected))
        failure = "OTA_SIGNATURE_LENGTH: Signature size does not match the device publisher key";
    else if (decoded != 0)
        failure = "OTA_SIGNATURE_ENCODING: Firmware publisher signature is not valid Base64";
    else if (mbedtls_pk_verify(&key, MBEDTLS_MD_SHA256, digest, 32,
                              signature.data(), length) != 0)
        failure = "OTA_SIGNATURE_INVALID: Firmware signature verification failed; check device publisher key";
    mbedtls_pk_free(&key);
    return failure;
}

bool openRequest(HttpClient &http, const FirmnginOtaRequest &request,
                 const String &path, SignatureHeaders *headers, String &error)
{
    const String url = request.baseUrl + path;
    auto config = fng_http_config(url.c_str(), request.ca.length() ? request.ca.c_str() : nullptr,
        request.clientCert.length() ? request.clientCert.c_str() : nullptr,
        request.privateKey.length() ? request.privateKey.c_str() : nullptr, request.insecure);
    config.disable_auto_redirect = true; // Never forward device authentication to another host.
    if (headers != nullptr)
    {
        config.event_handler = SignatureHeaders::event;
        config.user_data = headers;
    }
    http.handle = esp_http_client_init(&config);
    if (http.handle == nullptr)
    {
        error = "HTTP client initialization failed";
        return false;
    }
    const unsigned long timestamp = time(nullptr);
    char ts[24];
    snprintf(ts, sizeof(ts), "%lu", timestamp);
    const String message = request.deviceId + "." + ts + ".GET." + path;
    const String signature = firmngin_hmac_sha256_hex(request.deviceKey.c_str(), message.c_str());
    if (esp_http_client_set_header(http.handle, "X-Device-ID", request.deviceId.c_str()) != ESP_OK ||
        esp_http_client_set_header(http.handle, "X-Device-Timestamp", ts) != ESP_OK ||
        esp_http_client_set_header(http.handle, "X-Device-Signature", signature.c_str()) != ESP_OK ||
        esp_http_client_open(http.handle, 0) != ESP_OK ||
        esp_http_client_fetch_headers(http.handle) < 0 ||
        esp_http_client_get_status_code(http.handle) != 200)
    {
        error = "HTTP request failed";
        return false;
    }
    return true;
}
}

bool firmngin_ota_public_key_valid(const String &pem)
{
    mbedtls_pk_context key;
    if (!parsePublicKey(pem, key)) return false;
    mbedtls_pk_free(&key);
    return true;
}

FirmnginOtaTransfer::FirmnginOtaTransfer(const FirmnginOtaRequest &request) : request_(request) {}

FirmnginOtaTransfer::~FirmnginOtaTransfer()
{
    cancelled_ = true;
    // shortcut: destruction waits for the current SDK I/O call; use an async
    // transport if applications require a bounded shutdown time.
    if (started_) xSemaphoreTake(done_, portMAX_DELAY);
    if (events_) vQueueDelete(events_);
    if (done_) vSemaphoreDelete(done_);
}

bool FirmnginOtaTransfer::start()
{
    owner_ = xTaskGetCurrentTaskHandle();
    events_ = xQueueCreate(16, sizeof(FirmnginOtaEvent));
    done_ = xSemaphoreCreateBinary();
    if (!events_ || !done_) return false;
    started_ = xTaskCreate(task, "firmngin-ota", 8192, this, tskIDLE_PRIORITY + 1, nullptr) == pdPASS;
    return started_;
}

bool FirmnginOtaTransfer::poll(FirmnginOtaEvent &event)
{
    assert(xTaskGetCurrentTaskHandle() == owner_);
    return xQueueReceive(events_, &event, 0) == pdTRUE;
}

bool FirmnginOtaTransfer::send(FirmnginOtaEvent::Kind kind, const char *status,
                              const char *message, OTAAsyncState state, int progress)
{
    FirmnginOtaEvent event;
    event.kind = kind;
    event.state = state;
    event.progress = progress;
    event.partition = partition_;
    snprintf(event.status, sizeof(event.status), "%s", status);
    snprintf(event.message, sizeof(event.message), "%s", message);
    snprintf(event.firmwareId, sizeof(event.firmwareId), "%s", request_.firmware.id.c_str());
    while (!cancelled_)
        if (xQueueSend(events_, &event, pdMS_TO_TICKS(50)) == pdTRUE) return true;
    return false;
}

void FirmnginOtaTransfer::task(void *context)
{
    auto &transfer = *static_cast<FirmnginOtaTransfer *>(context);
    assert(xTaskGetCurrentTaskHandle() != transfer.owner_);
    const bool success = transfer.execute(); // All HTTP/hash/flash guards unwind before the terminal event.
    if (success)
        transfer.send(transfer.noUpdate_ ? FirmnginOtaEvent::UpToDate : FirmnginOtaEvent::Ready,
                      transfer.noUpdate_ ? "uptodate" : "installing",
                      transfer.noUpdate_ ? "No update available" : "Firmware verified",
                      OTA_ASYNC_INSTALLING);
    else
        transfer.send(FirmnginOtaEvent::Failed, "failed", transfer.error_.c_str(), OTA_ASYNC_FAILED);
    xSemaphoreGive(transfer.done_);
    vTaskDelete(nullptr); // Do not access transfer after giving done_: its owner may now delete it.
}

bool FirmnginOtaTransfer::execute()
{
    // Recheck the owned copies before any network I/O (Arduino String copies
    // can fail allocation without throwing).
    const char *securityError = firmngin_http_security_error(
        request_.insecure, request_.ca.length() > 0, fng_http_insecure_supported());
    if (securityError != nullptr || !fng_startsWith(request_.baseUrl, "https://") ||
        request_.deviceId.length() == 0 || request_.deviceKey.length() == 0 ||
        (request_.clientAuth && (request_.clientCert.length() == 0 || request_.privateKey.length() == 0)) ||
        !firmngin_ota_public_key_valid(request_.publisherKey))
    {
        error_ = securityError != nullptr ? securityError : "OTA credential snapshot is incomplete or invalid";
        return false;
    }
    if (request_.firmware.id.length() == 0)
    {
        send(FirmnginOtaEvent::Status, "checking", "Requesting manifest");
        String path;
        if (!firmngin_ota_manifest_path(request_.board, request_.model, request_.currentVersion, path))
        {
            error_ = "OTA manifest query is too long";
            return false;
        }
        HttpClient manifest;
        if (!openRequest(manifest, request_, path, nullptr, error_)) return false;
        String body;
        const auto result = firmngin_read_http_body(
            [this, &manifest](char *data, size_t size) {
                return cancelled_ ? -1 : esp_http_client_read(manifest.handle, data, size);
            },
            [&manifest]() { return esp_http_client_is_complete_data_received(manifest.handle); }, body, 2048);
        if (result != FirmnginHttpBodyResult::Complete)
        {
            error_ = "Manifest response is incomplete or too large";
            return false;
        }
        const auto parsed = firmngin_parse_ota_manifest(body, request_.firmware);
        if (parsed == FirmnginOtaManifestResult::UpToDate)
        {
            noUpdate_ = true;
            return true;
        }
        if (parsed != FirmnginOtaManifestResult::Available)
        {
            error_ = "Manifest fields are missing or invalid";
            return false;
        }
        const String message = String("Update available: ") + request_.firmware.version;
        send(FirmnginOtaEvent::Status, "available", message.c_str());
    }
    if (cancelled_) { error_ = "OTA cancelled"; return false; }
    if (!isValidFirmwareID(request_.firmware.id.c_str()) || !isValidOTAHash(request_.firmware.sha256))
    {
        error_ = "Invalid OTA firmware ID or SHA256";
        return false;
    }
    send(FirmnginOtaEvent::Status, "downloading", "Starting firmware download", OTA_ASYNC_DOWNLOADING, 0);
    SignatureHeaders headers;
    HttpClient http;
    if (!openRequest(http, request_, String("/firmware/") + request_.firmware.id + "/download", &headers, error_))
        return false;
    const int64_t size = esp_http_client_get_content_length(http.handle);
    const esp_partition_t *partition = esp_ota_get_next_update_partition(nullptr);
    if (!partition || size <= 0 || static_cast<uint64_t>(size) > partition->size)
    {
        error_ = "Firmware size or partition is invalid";
        return false;
    }
    if (headers.invalid || headers.signature.length() == 0)
    {
        error_ = "Firmware publisher signature is missing or invalid";
        return false;
    }
    if (headers.keyHash.length() && (!isValidOTAHash(headers.keyHash) ||
        !fng_equalsIgnoreCase(publicKeyHash(request_.publisherKey), headers.keyHash)))
    {
        error_ = "OTA_PUBLISHER_KEY_MISMATCH: Device publisher key differs from server";
        return false;
    }
    Sha256 hash;
    if (mbedtls_md_setup(&hash.context, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0) != 0 ||
        mbedtls_md_starts(&hash.context) != 0)
    {
        error_ = "SHA256 initialization failed";
        return false;
    }
    FlashWriter flash;
    if (esp_ota_begin(partition, OTA_WITH_SEQUENTIAL_WRITES, &flash.handle) != ESP_OK)
    {
        error_ = "OTA partition initialization failed";
        return false;
    }
    esp_http_client_set_timeout_ms(http.handle, 250);
    uint8_t buffer[256];
    int64_t received = 0;
    unsigned long lastByte = fng_now_ms();
    int lastBucket = -1;
    while (received < size && !cancelled_)
    {
        const int count = esp_http_client_read(http.handle, reinterpret_cast<char *>(buffer),
            static_cast<int>(std::min<int64_t>(sizeof(buffer), size - received)));
        if (count <= 0)
        {
            if (count < 0 && count != -ESP_ERR_HTTP_EAGAIN)
            {
                error_ = "Firmware read failed";
                return false;
            }
            if (esp_http_client_is_complete_data_received(http.handle) || otaStallExceeded(fng_now_ms(), lastByte, 15000UL))
            {
                error_ = "Firmware download incomplete or stalled";
                return false;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        if (count > size - received || esp_ota_write(flash.handle, buffer, count) != ESP_OK ||
            mbedtls_md_update(&hash.context, buffer, count) != 0)
        {
            error_ = "Firmware write or hash update failed";
            return false;
        }
        received += count;
        lastByte = fng_now_ms();
        const int percent = static_cast<int>(received * 100 / size);
        if (percent / 10 != lastBucket)
        {
            lastBucket = percent / 10;
            send(FirmnginOtaEvent::Status, "downloading", "Receiving firmware", OTA_ASYNC_DOWNLOADING, percent);
        }
    }
    if (cancelled_ || !esp_http_client_is_complete_data_received(http.handle))
    {
        error_ = "Firmware download incomplete or cancelled";
        return false;
    }
    send(FirmnginOtaEvent::Status, "downloading", "Verifying firmware", OTA_ASYNC_VERIFYING, 100);
    uint8_t digest[32];
    if (mbedtls_md_finish(&hash.context, digest) != 0)
    {
        error_ = "SHA256 finalization failed";
        return false;
    }
    char hex[65];
    for (size_t i = 0; i < sizeof(digest); ++i) snprintf(hex + 2 * i, 3, "%02x", digest[i]);
    if (!fng_equalsIgnoreCase(request_.firmware.sha256, hex))
    {
        error_ = "SHA256 mismatch";
        return false;
    }
    const char *failure = verifySignature(request_.publisherKey, digest, headers.signature);
    if (failure != nullptr)
    {
        error_ = failure;
        return false;
    }
    if (!flash.finish())
    {
        error_ = "Firmware finalization failed";
        return false;
    }
    partition_ = partition;
    return true;
}
