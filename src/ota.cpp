#include "firmngin.h"
#include "hmac.h"
#include "ota_progress.h"

#if defined(ESP32)
#include <Update.h>
#elif defined(ESP8266)
#include <Updater.h>
#endif

void Firmngin::setOTABaseURL(const char *baseURL)
{
    _otaBaseUrl = baseURL != nullptr ? baseURL : "";
}

void Firmngin::setEnableOTA(bool enabled)
{
    _otaEnabled = enabled;
}

void Firmngin::enableOTA(bool enabled)
{
    setEnableOTA(enabled);
}

bool Firmngin::otaHTTPGet(const char *path, const char *queryParams, String &responseBody)
{
    String fullPath = String(path);
    if (queryParams != nullptr && strlen(queryParams) > 0)
    {
        fullPath += "?";
        fullPath += queryParams;
    }

    String url = _otaBaseUrl + fullPath;

    unsigned long ts = time(nullptr);
    String message = String(_deviceId) + "." + String(ts) + ".GET." + fullPath;

#if defined(ESP8266) || defined(ESP32)
    HTTPClient http;
#if defined(ESP32)
    WiFiClientSecure otaClient;
    if (_insecure)
    {
        otaClient.setInsecure();
    }
    else
    {
#if defined(HAS_SERVICE_CA_CERT)
        otaClient.setCACert(SERVICE_CA_CERT);
#else
        publishOTAStatus("failed", "Service CA certificate is not configured");
        return false;
#endif
    }
    if (_clientCert != nullptr)
        otaClient.setCertificate(_clientCert);
    if (_privateKey != nullptr)
        otaClient.setPrivateKey(_privateKey);
#elif defined(ESP8266)
    BearSSL::WiFiClientSecure otaClient;
    otaClient.setBufferSizes(512, 512);
    if (_clientCertList != nullptr && _clientPrivKey != nullptr)
        otaClient.setClientRSACert(_clientCertList, _clientPrivKey);
#if defined(HAS_SERVICE_CA_CERT)
    BearSSL::X509List serviceCACert(SERVICE_CA_CERT);
    otaClient.setTrustAnchors(&serviceCACert);
#else
    publishOTAStatus("failed", "Service CA certificate is not configured");
    return false;
#endif
#endif
    if (!http.begin(otaClient, url))
    {
        publishOTAStatus("failed", "Manifest HTTP client initialization failed");
        return false;
    }
    http.addHeader("X-Device-ID", String(_deviceId));
    http.addHeader("X-Device-Timestamp", String(ts));
    http.addHeader("X-Device-Signature", firmngin_hmac_sha256_hex(_deviceKey, message.c_str()));
    http.addHeader("Accept", "application/json");

    int httpCode = http.GET();
    bool ok = (httpCode == HTTP_CODE_OK);
    if (ok)
        responseBody = http.getString();
    else
    {
        _Debug("OTA HTTP " + String(httpCode) + ": " + url);
        _Debug("OTA HTTP error: " + http.errorToString(httpCode));
    }

    http.end();
    return ok;
#else
    (void)responseBody;
    return false;
#endif
}

bool Firmngin::checkOTA()
{
    if (!_otaEnabled)
        return publishOTAStatus("disabled", "OTA is not enabled");

    if (_otaBaseUrl.length() == 0)
        return publishOTAStatus("skipped", "OTA base URL is not configured");

    publishOTAStatus("checking", "Requesting manifest");

    char query[256];
    int pos = 0;
    if (_firmwareTargetBoard.length() > 0)
    {
        pos += snprintf(query + pos, sizeof(query) - pos,
                        "target_board=%s&", _firmwareTargetBoard.c_str());
    }
    if (_firmwareTargetModel.length() > 0)
    {
        pos += snprintf(query + pos, sizeof(query) - pos,
                        "target_model=%s&", _firmwareTargetModel.c_str());
    }
    pos += snprintf(query + pos, sizeof(query) - pos,
                    "current_version=%s", _firmwareVersion.c_str());

    String body;
    if (!otaHTTPGet("/manifest", query, body))
    {
        publishOTAStatus("failed", "Manifest request failed");
        return false;
    }

    int idStart = body.indexOf("\"firmware_id\":\"");
    int verStart = body.indexOf("\"version\":\"");
    int shaStart = body.indexOf("\"sha256\":\"");

    if (idStart < 0)
    {
        publishOTAStatus("uptodate", "No update available");
        return true;
    }

    idStart += 15;
    _otaFirmwareID = body.substring(idStart, body.indexOf("\"", idStart));
    verStart += 11;
    String newVer = body.substring(verStart, body.indexOf("\"", verStart));
    shaStart += 10;
    _otaFirmwareSHA256 = body.substring(shaStart, body.indexOf("\"", shaStart));

    char msg[128];
    snprintf(msg, sizeof(msg), "Update available: %s", newVer.c_str());
    publishOTAStatus("available", msg);
    return true;
}

bool Firmngin::performOTA(const char *manifestUrl)
{
    (void)manifestUrl;

    if (_otaAsyncState != OTA_ASYNC_IDLE)
        return publishOTAStatus("skipped", "OTA already in progress");

    if (!_otaEnabled)
    {
        _otaAsyncState = OTA_ASYNC_IDLE;
        _otaFirmwareID = "";
        return publishOTAStatus("disabled", "OTA is not enabled");
    }

    if (_otaFirmwareID.length() == 0)
    {
        if (!checkOTA())
        {
            _otaAsyncState = OTA_ASYNC_IDLE;
            _otaFirmwareID = "";
            return false;
        }
    }

    if (_otaFirmwareID.length() == 0)
    {
        _otaAsyncState = OTA_ASYNC_IDLE;
        _otaFirmwareID = "";
        return publishOTAStatus("skipped", "No firmware available");
    }

    publishOTAStatus("downloading", "Starting firmware download");

    String downloadPath = String("/firmware/") + _otaFirmwareID + "/download";

#if defined(ESP8266) || defined(ESP32)
    String url = _otaBaseUrl + downloadPath;
#if defined(ESP32)
    if (_insecure)
    {
        _otaWifiClient.setInsecure();
    }
    else
    {
#if defined(HAS_SERVICE_CA_CERT)
        _otaWifiClient.setCACert(SERVICE_CA_CERT);
#else
        _otaAsyncState = OTA_ASYNC_IDLE;
        publishOTAStatus("failed", "Service CA certificate is not configured");
        _otaFirmwareID = "";
        return false;
#endif
    }
    if (_clientCert != nullptr)
        _otaWifiClient.setCertificate(_clientCert);
    if (_privateKey != nullptr)
        _otaWifiClient.setPrivateKey(_privateKey);
#elif defined(ESP8266)
    _otaWifiClient.setBufferSizes(512, 512);
    if (_clientCertList != nullptr && _clientPrivKey != nullptr)
        _otaWifiClient.setClientRSACert(_clientCertList, _clientPrivKey);
#if defined(HAS_SERVICE_CA_CERT)
    if (_otaTrustAnchors != nullptr)
    {
        delete _otaTrustAnchors;
    }
    _otaTrustAnchors = new BearSSL::X509List(SERVICE_CA_CERT);
    _otaWifiClient.setTrustAnchors(_otaTrustAnchors);
#else
    _otaAsyncState = OTA_ASYNC_IDLE;
    publishOTAStatus("failed", "Service CA certificate is not configured");
    _otaFirmwareID = "";
    return false;
#endif
#endif
    if (!_otaHttp.begin(_otaWifiClient, url))
    {
        _otaAsyncState = OTA_ASYNC_IDLE;
        publishOTAStatus("failed", "Firmware HTTP client initialization failed");
        _otaFirmwareID = "";
        return false;
    }

    unsigned long ts = time(nullptr);
    String message = String(_deviceId) + "." + String(ts) + ".GET." + downloadPath;
    _otaHttp.addHeader("X-Device-ID", String(_deviceId));
    _otaHttp.addHeader("X-Device-Timestamp", String(ts));
    _otaHttp.addHeader("X-Device-Signature", firmngin_hmac_sha256_hex(_deviceKey, message.c_str()));

    int httpCode = _otaHttp.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        _Debug("OTA download HTTP " + String(httpCode));
        _Debug("OTA download error: " + _otaHttp.errorToString(httpCode));
        _otaHttp.end();
        _otaAsyncState = OTA_ASYNC_IDLE;
        publishOTAStatus("failed", "Firmware download failed");
        _otaFirmwareID = "";
        return false;
    }

    _otaContentLength = _otaHttp.getSize();
    if (_debug)
    {
        Serial.print("OTA download started (async). Size: ");
        Serial.print(_otaContentLength >= 0 ? String(_otaContentLength / 1024) : String("unknown"));
        if (_otaContentLength >= 0)
            Serial.println(" KB");
        else
            Serial.println();
    }

    _otaBuffer = (uint8_t *)malloc(FIRMWARE_BUFFER_SIZE);
    if (_otaBuffer == nullptr)
    {
        _otaHttp.end();
        _otaAsyncState = OTA_ASYNC_IDLE;
        publishOTAStatus("failed", "Firmware buffer allocation failed");
        _otaFirmwareID = "";
        return false;
    }
    _otaDownloaded = 0;
    _otaLastProgressBucket = -1;
    _otaLastPublishedProgressBucket = -1;
    _otaLastDebugAt = 0;

#if defined(ESP32)
    mbedtls_md_init(&_otaSha256Ctx);
    mbedtls_md_setup(&_otaSha256Ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
    mbedtls_md_starts(&_otaSha256Ctx);
#elif defined(ESP8266)
    br_sha256_init(&_otaSha256Ctx);
#endif

    if (_otaContentLength > 0 && !Update.begin(_otaContentLength))
    {
        if (_debug)
            Serial.println("OTA Update.begin() failed: " + String(Update.getError()));
        _otaCleanup();
        publishOTAStatus("failed", "Update.begin() failed");
        _otaFirmwareID = "";
        return false;
    }

    _otaAsyncState = OTA_ASYNC_DOWNLOADING;
    return true;
#else
    _otaAsyncState = OTA_ASYNC_IDLE;
    _otaFirmwareID = "";
    return publishOTAStatus("skipped", "Platform not supported for HTTP OTA");
#endif
}

void Firmngin::_processOTA()
{
    if (_otaAsyncState == OTA_ASYNC_IDLE || _otaAsyncState == OTA_ASYNC_FAILED)
        return;

    if (_otaAsyncState == OTA_ASYNC_DOWNLOADING)
    {
        WiFiClient *stream = _otaHttp.getStreamPtr();
        if (stream == nullptr)
        {
            _otaCleanup();
            publishOTAStatus("failed", "HTTP stream lost");
            _otaFirmwareID = "";
            return;
        }

        size_t available = stream->available();
        if (available == 0)
        {
            if (!_otaHttp.connected())
                _otaAsyncState = OTA_ASYNC_VERIFYING;
            return;
        }

        int toRead = min((int)available, FIRMWARE_BUFFER_SIZE);
        int readLen = stream->readBytes(_otaBuffer, toRead);
        if (readLen <= 0)
        {
            if (!_otaHttp.connected())
                _otaAsyncState = OTA_ASYNC_VERIFYING;
            return;
        }

        _otaDownloaded += readLen;

        yield();

#if defined(ESP32)
        mbedtls_md_update(&_otaSha256Ctx, _otaBuffer, readLen);
#elif defined(ESP8266)
        br_sha256_update(&_otaSha256Ctx, _otaBuffer, readLen);
#endif

        if (_otaContentLength > 0)
        {
            size_t written = Update.write(_otaBuffer, readLen);
            if (written != (size_t)readLen)
            {
                if (_debug)
                    Serial.println("OTA Update.write() failed: " + String(Update.getError()));
#if defined(ESP32)
                Update.abort();
#endif
                _otaCleanup();
                publishOTAStatus("failed", "Firmware write failed");
                _otaFirmwareID = "";
                return;
            }

            int progress = (_otaDownloaded * 100) / _otaContentLength;
            int progressBucket = progress / 10;

            if (progressBucket != _otaLastPublishedProgressBucket || progress == 100)
            {
                _otaLastPublishedProgressBucket = progressBucket;
                char progressValue[8];
                snprintf(progressValue, sizeof(progressValue), "%d", progress);
                char progressPayload[160];
                firmngin_json::ArrayBuilder b(progressPayload, sizeof(progressPayload));
                b.reset();
                b.add("ots", "downloading");
                b.add("otp", progressValue);
                if (_otaFirmwareID.length() > 0)
                    b.add("ofd", _otaFirmwareID.c_str());
                updateEntities(b.build());
            }

            if (_debug)
            {
                if (progressBucket != _otaLastProgressBucket || progress == 100)
                {
                    _otaLastProgressBucket = progressBucket;
                    Serial.print("OTA download progress: ");
                    Serial.print(progress);
                    Serial.print("% (");
                    Serial.print(_otaDownloaded / 1024);
                    Serial.print("/");
                    Serial.print(_otaContentLength / 1024);
                    Serial.println(" KB)");
                }
            }
        }
        else if (_debug && millis() - _otaLastDebugAt > 1000)
        {
            _otaLastDebugAt = millis();
            Serial.print("OTA download received: ");
            Serial.print(_otaDownloaded / 1024);
            Serial.println(" KB");
        }
        else if (millis() - _otaLastDebugAt > 1000)
        {
            _otaLastDebugAt = millis();
            int estimated = _otaDownloaded / 32;
            if (estimated < 0)
                estimated = 0;
            if (estimated > 99)
                estimated = 99;
            char progressValue[8];
            snprintf(progressValue, sizeof(progressValue), "%d", estimated);
            char progressPayload[160];
            firmngin_json::ArrayBuilder b(progressPayload, sizeof(progressPayload));
            b.reset();
            b.add("ots", "downloading");
            b.add("otp", progressValue);
            if (_otaFirmwareID.length() > 0)
                b.add("ofd", _otaFirmwareID.c_str());
            updateEntities(b.build());

            if (_debug)
            {
                Serial.print("OTA download received: ");
                Serial.print(_otaDownloaded / 1024);
                Serial.println(" KB");
            }
        }

        if (_otaContentLength > 0 && _otaDownloaded >= _otaContentLength)
        {
            _otaAsyncState = OTA_ASYNC_VERIFYING;
        }
        else
        {
            if (_otaHttp.connected() || _otaContentLength < 0 || _otaDownloaded < _otaContentLength)
                return;

            _otaAsyncState = OTA_ASYNC_VERIFYING;
        }
    }

    if (_otaAsyncState == OTA_ASYNC_VERIFYING)
    {
        _otaHttp.end();

        if (_otaContentLength > 0 && _otaDownloaded != _otaContentLength)
        {
#if defined(ESP32)
            Update.abort();
#endif
            _otaCleanup();
            publishOTAStatus("failed", "Firmware download incomplete");
            _otaFirmwareID = "";
            return;
        }

#if defined(ESP32)
        uint8_t computedHash[32];
        mbedtls_md_finish(&_otaSha256Ctx, computedHash);
        mbedtls_md_free(&_otaSha256Ctx);
#elif defined(ESP8266)
        uint8_t computedHash[32];
        br_sha256_out(&_otaSha256Ctx, computedHash);
#endif

#if defined(ESP32) || defined(ESP8266)
        char computedHex[65];
        for (int i = 0; i < 32; i++)
            sprintf(computedHex + (i * 2), "%02x", computedHash[i]);
        computedHex[64] = '\0';

        if (_otaFirmwareSHA256.length() > 0 && _otaFirmwareSHA256 != computedHex)
        {
            if (_debug)
            {
                Serial.print("OTA SHA256 mismatch: expected ");
                Serial.print(_otaFirmwareSHA256);
                Serial.print(", got ");
                Serial.println(computedHex);
            }
#if defined(ESP32)
            Update.abort();
#endif
            _otaCleanup();
            publishOTAStatus("failed", "SHA256 mismatch");
            _otaFirmwareID = "";
            return;
        }

        if (_debug)
        {
            Serial.print("OTA SHA256 verified OK: ");
            Serial.println(computedHex);
        }
#endif
        publishOTAStatus("installing", "Installing firmware");
        _otaAsyncState = OTA_ASYNC_INSTALLING;
    }

    if (_otaAsyncState == OTA_ASYNC_INSTALLING)
    {
        if (_debug)
        {
            Serial.print("OTA download completed: ");
            Serial.print(_otaDownloaded / 1024);
            Serial.println(" KB");
        }

        if (!Update.end())
        {
            if (_debug)
                Serial.println("OTA Update.end() failed: " + String(Update.getError()));
            _otaCleanup();
            publishOTAStatus("failed", "Update.end() failed");
            _otaFirmwareID = "";
            return;
        }

        publishOTAStatus("installing", "Finalizing firmware install");
        publishOTAStatus("installed", "Firmware installed");
        publishOTAStatus("rebooting", "Rebooting device");
        _otaFirmwareID = "";

        free(_otaBuffer);
        _otaBuffer = nullptr;
        _otaAsyncState = OTA_ASYNC_IDLE;

        if (_debug)
            Serial.println("OTA installed successfully, rebooting...");
        delay(500);
        ESP.restart();
    }
}

void Firmngin::_otaCleanup()
{
    _otaAsyncState = OTA_ASYNC_FAILED;
    _otaHttp.end();
    if (_otaBuffer != nullptr)
    {
        free(_otaBuffer);
        _otaBuffer = nullptr;
    }
#if defined(ESP32)
    mbedtls_md_free(&_otaSha256Ctx);
#endif
    _otaDownloaded = 0;
    _otaContentLength = 0;
}

void Firmngin::onOTAStatus(OTACallbackFunction callback)
{
    _otaCallback = callback;
}

int Firmngin::normalizeOTAProgress(const char *status, const char *message)
{
    return firmngin_normalize_ota_progress(status, message);
}

bool Firmngin::publishOTAStatus(const char *status, const char *message)
{
    if (_debug)
    {
        Serial.print("OTA status: ");
        Serial.print(status);
        Serial.print(" - ");
        Serial.println(message);
    }

    if (_otaCallback)
        _otaCallback(status, message);
    for (const auto &callback : _otaCallbacks)
    {
        if (callback)
        {
            callback(status, message);
        }
    }

    char buf[512];
    firmngin_json::ArrayBuilder b(buf, sizeof(buf));
    b.reset();
    b.add("ots", status);
    char progressValue[8];
    snprintf(progressValue, sizeof(progressValue), "%d", normalizeOTAProgress(status, message));
    b.add("otp", progressValue);
    if (_otaFirmwareID.length() > 0)
        b.add("ofd", _otaFirmwareID.c_str());

    return updateEntities(b.build());
}
