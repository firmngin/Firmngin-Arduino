#include "firmngin.h"
#include "hmac.h"
#include "http_response.h"
#include "ota_manifest.h"
#include "ota_transfer.h"
#include <new>

void Firmngin::setOTABaseURL(const char *baseURL)
{
    _otaBaseUrl = baseURL != nullptr ? baseURL : "";
}

void Firmngin::setOTAFirmwarePublicKey(const char *publicKeyPem)
{
    _otaFirmwarePublicKey = publicKeyPem != nullptr ? publicKeyPem : "";
}

void Firmngin::setEnableOTA(bool enabled) { _otaEnabled = enabled; }
void Firmngin::enableOTA(bool enabled) { setEnableOTA(enabled); }

bool Firmngin::otaHTTPGet(const char *path, const char *queryParams, String &responseBody)
{
    String fullPath = path;
    if (queryParams != nullptr && queryParams[0] != '\0')
        fullPath += String("?") + queryParams;
    const String url = _otaBaseUrl + fullPath;
    if (!fng_startsWith(url, "https://"))
    {
        publishOTAStatus("failed", "OTA requires HTTPS");
        return false;
    }
    const char *ca = _activeServiceCACert();
    const char *securityError = firmngin_http_security_error(_insecure, ca != nullptr,
                                                            fng_http_insecure_supported());
    if (securityError != nullptr)
    {
        publishOTAStatus("failed", securityError);
        return false;
    }
    auto config = fng_http_config(url.c_str(), ca, _clientCert, _privateKey, _insecure);
    config.disable_auto_redirect = true;
    auto client = esp_http_client_init(&config);
    if (!client)
    {
        publishOTAStatus("failed", "Manifest HTTP client initialization failed");
        return false;
    }
    const unsigned long timestamp = time(nullptr);
    char ts[24];
    snprintf(ts, sizeof(ts), "%lu", timestamp);
    const String message = String(_deviceId) + "." + ts + ".GET." + fullPath;
    const String signature = firmngin_hmac_sha256_hex(_deviceKey, message.c_str());
    const bool opened =
        esp_http_client_set_header(client, "X-Device-ID", _deviceId) == ESP_OK &&
        esp_http_client_set_header(client, "X-Device-Timestamp", ts) == ESP_OK &&
        esp_http_client_set_header(client, "X-Device-Signature", signature.c_str()) == ESP_OK &&
        esp_http_client_set_header(client, "Accept", "application/json") == ESP_OK &&
        esp_http_client_open(client, 0) == ESP_OK &&
        esp_http_client_fetch_headers(client) >= 0 &&
        esp_http_client_get_status_code(client) == 200;
    bool ok = false;
    if (opened)
    {
        ok = firmngin_read_http_body(
            [client](char *data, size_t capacity) { return esp_http_client_read(client, data, capacity); },
            [client]() { return esp_http_client_is_complete_data_received(client); },
            responseBody, 2048) == FirmnginHttpBodyResult::Complete;
    }
    esp_http_client_cleanup(client);
    if (!ok)
        publishOTAStatus("failed", "Manifest request failed or response incomplete/too large");
    return ok;
}

bool Firmngin::checkOTA()
{
    // Explicit metadata checks are synchronous; performOTA never calls this from
    // the loop. Its worker fetches a manifest when no cached artifact is present.
    if (_otaTransfer != nullptr || !_otaEnabled)
        return false;
    if (!firmngin_ota_public_key_valid(_otaFirmwarePublicKey))
    {
        publishOTAStatus("failed", "OTA publisher verification key is missing or invalid");
        return false;
    }
    String path;
    if (!firmngin_ota_manifest_path(_firmwareTargetBoard, _firmwareTargetModel, _firmwareVersion, path))
    {
        publishOTAStatus("failed", "OTA manifest query is too long");
        return false;
    }
    publishOTAStatus("checking", "Requesting manifest");
    String body;
    if (!otaHTTPGet(path.c_str(), nullptr, body)) return false;
    FirmnginOtaManifest manifest;
    const auto result = firmngin_parse_ota_manifest(body, manifest);
    _otaFirmwareID = manifest.id;
    _otaFirmwareVersion = manifest.version;
    _otaFirmwareSHA256 = manifest.sha256;
    if (result == FirmnginOtaManifestResult::Invalid)
    {
        publishOTAStatus("failed", "Manifest fields are missing or invalid");
        return false;
    }
    if (result == FirmnginOtaManifestResult::UpToDate)
        publishOTAStatus("uptodate", "No update available");
    else
    {
        const String message = String("Update available: ") + manifest.version;
        publishOTAStatus("available", message.c_str());
    }
    return true;
}

bool Firmngin::performOTA(const char *manifestUrl)
{
    (void)manifestUrl;
    if (_otaTransfer != nullptr || !firmngin_ota_can_start(_otaAsyncState))
        return false;
    if (!_securityReady)
    {
        publishOTAStatus("failed", "Call begin() successfully before starting OTA");
        return false;
    }
    if (!_otaEnabled)
    {
        publishOTAStatus("disabled", "OTA is not enabled");
        return false;
    }
    const char *failure = nullptr;
    const char *ca = _activeServiceCACert();
    if (!fng_startsWith(_otaBaseUrl, "https://")) failure = "OTA requires HTTPS";
    else if (!firmngin_ota_public_key_valid(_otaFirmwarePublicKey))
        failure = "OTA publisher verification key is missing or invalid";
    else if (_otaRollbackPending) failure = "Previous OTA boot confirmation is still pending";
    else failure = firmngin_http_security_error(_insecure, ca != nullptr, fng_http_insecure_supported());
    if (failure != nullptr)
    {
        publishOTAStatus("failed", failure);
        return false;
    }
    if (!_clearOtaBootReport())
    {
        publishOTAStatus("failed", "Previous OTA boot report could not be cleared");
        return false;
    }
    FirmnginOtaRequest request;
    request.baseUrl = _otaBaseUrl;
    request.deviceId = _deviceId;
    request.deviceKey = _deviceKey;
    request.ca = ca != nullptr ? ca : "";
    request.clientCert = _clientCert != nullptr ? _clientCert : "";
    request.privateKey = _privateKey != nullptr ? _privateKey : "";
    request.publisherKey = _otaFirmwarePublicKey;
    request.board = _firmwareTargetBoard;
    request.model = _firmwareTargetModel;
    request.currentVersion = _firmwareVersion;
    request.firmware.id = _otaFirmwareID;
    request.firmware.version = _otaFirmwareVersion;
    request.firmware.sha256 = _otaFirmwareSHA256;
    request.insecure = _insecure;
    request.clientAuth = _clientCert != nullptr || _privateKey != nullptr;
    _otaTransfer = new (std::nothrow) FirmnginOtaTransfer(request);
    if (!_otaTransfer || !_otaTransfer->start())
    {
        _otaCleanup();
        publishOTAStatus("failed", "OTA worker allocation failed");
        return false;
    }
    _otaAsyncState = OTA_ASYNC_DOWNLOADING;
    return true; // Accepted: all network and flash I/O now belongs to the worker.
}

void Firmngin::_processOTA()
{
    if (!_otaTransfer) return;
    FirmnginOtaEvent event;
    for (int i = 0; i < 8 && _otaTransfer->poll(event); ++i)
    {
        if (event.kind == FirmnginOtaEvent::Status)
        {
            _otaAsyncState = event.state;
            _otaFirmwareID = event.firmwareId;
            publishOTAStatus(event.status, event.message, event.firmwareId, event.progress);
            continue;
        }
        // A terminal event is emitted only after all transfer I/O has unwound.
        delete _otaTransfer;
        _otaTransfer = nullptr;
        if (event.kind == FirmnginOtaEvent::Failed || event.kind == FirmnginOtaEvent::UpToDate)
        {
            _otaAsyncState = event.kind == FirmnginOtaEvent::Failed ? OTA_ASYNC_FAILED : OTA_ASYNC_IDLE;
            _otaFirmwareID = "";
            publishOTAStatus(event.status, event.message, event.firmwareId);
            return; // A callback may start a new transfer; do not touch it here.
        }
        _otaAsyncState = OTA_ASYNC_INSTALLING;
        _otaFirmwareID = event.firmwareId;
        const esp_partition_t *candidatePartition = event.partition;
        if (!candidatePartition)
        {
            _otaCleanup();
            publishOTAStatus("failed", "OTA target partition is unavailable");
            _otaFirmwareID = "";
            return;
        }
        publishOTAStatus("installing", "Installing verified firmware");
        const bool previousRollbackPending = _otaRollbackPending;
        const int previousRollbackBootCount = _otaRollbackBootCount;
        const uint32_t previousPendingPartition = _otaPendingPartition;
        const String previousBootFirmwareID = _otaBootFirmwareID;
        _otaPendingPartition = candidatePartition->address;
        _otaBootFirmwareID = _otaFirmwareID;
        _otaRollbackPending = true;
        _otaRollbackBootCount = 0;
        // The worker never selects boot. Persist the rollback intent first.
        if (!_persistOtaRollbackState())
        {
            _otaRollbackPending = previousRollbackPending;
            _otaRollbackBootCount = previousRollbackBootCount;
            _otaPendingPartition = previousPendingPartition;
            _otaBootFirmwareID = previousBootFirmwareID;
            _otaCleanup();
            publishOTAStatus("failed", "Rollback state could not be saved");
            _otaFirmwareID = "";
            return;
        }
        if (esp_ota_set_boot_partition(candidatePartition) != ESP_OK)
        {
            _otaRollbackPending = previousRollbackPending;
            _otaRollbackBootCount = previousRollbackBootCount;
            _otaPendingPartition = previousPendingPartition;
            _otaBootFirmwareID = previousBootFirmwareID;
            if (!_persistOtaRollbackState())
                _Debug("OTA rollback: failed to restore state after boot partition error");
            _otaCleanup();
            publishOTAStatus("failed", "OTA target partition is unavailable");
            _otaFirmwareID = "";
            return;
        }
        publishOTAStatus("installed", "Firmware installed");
        publishOTAStatus("rebooting", "Rebooting device");
        _otaFirmwareID = "";
        fng_delay_ms(500);
        fng_restart();
        return;
    }
}

void Firmngin::_otaCleanup()
{
    delete _otaTransfer;
    _otaTransfer = nullptr;
    _otaAsyncState = OTA_ASYNC_FAILED;
}

void Firmngin::onOTAStatus(OTACallbackFunction callback) { _otaCallback = callback; }
int Firmngin::normalizeOTAProgress(const char *status, const char *message)
{
    return firmngin_normalize_ota_progress(status, message);
}

bool Firmngin::publishOTAStatus(const char *status, const char *message,
                                const char *artifactId, int progressOverride)
{
    // Snapshot before calling application code: callbacks may start another OTA.
    const String firmwareId = artifactId != nullptr ? artifactId : _otaFirmwareID;
    if (_debug) _Debug(String("OTA status: ") + status + " - " + message);
    if (_otaCallback) _otaCallback(status, message);
    for (const auto &callback : _otaCallbacks)
        if (callback) callback(status, message);
    char buffer[512], progress[8], detail[128];
    firmngin_json::ArrayBuilder builder(buffer, sizeof(buffer));
    builder.reset();
    builder.add("ots", status);
    const int percent = progressOverride >= 0 ? progressOverride :
        (status && strcmp(status, "downloading") == 0 ? 0 : normalizeOTAProgress(status, message));
    snprintf(progress, sizeof(progress), "%d", percent);
    builder.add("otp", progress);
    if (firmwareId.length()) builder.add("ofd", firmwareId.c_str());
    snprintf(detail, sizeof(detail), "%s", message != nullptr ? message : "");
    builder.add("ota_message", detail);
    return updateEntities(builder.build());
}
