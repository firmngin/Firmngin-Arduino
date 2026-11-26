#ifndef FIRMNGIN_OTA_TRANSFER_H
#define FIRMNGIN_OTA_TRANSFER_H

#include "ota_manifest.h"
#include "ota_progress.h"
#include <esp_ota_ops.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <atomic>

struct FirmnginOtaRequest
{
    String baseUrl, deviceId, deviceKey;
    String ca, clientCert, privateKey, publisherKey;
    String board, model, currentVersion;
    FirmnginOtaManifest firmware;
    bool insecure = false;
    bool clientAuth = false;
};

struct FirmnginOtaEvent
{
    enum Kind { Status, Ready, Failed, UpToDate } kind = Status;
    OTAAsyncState state = OTA_ASYNC_DOWNLOADING;
    char status[16] = {};
    char message[160] = {};
    char firmwareId[37] = {};
    int progress = -1;
    const esp_partition_t *partition = nullptr;
};

bool firmngin_ota_public_key_valid(const String &pem);

// One task owns all transfer resources. Only fixed-size event copies cross back
// to Firmngin::loop(); the worker never calls application code or selects boot.
struct FirmnginOtaTransfer
{
    explicit FirmnginOtaTransfer(const FirmnginOtaRequest &request);
    ~FirmnginOtaTransfer();
    FirmnginOtaTransfer(const FirmnginOtaTransfer &) = delete;
    FirmnginOtaTransfer &operator=(const FirmnginOtaTransfer &) = delete;
    bool start();
    bool poll(FirmnginOtaEvent &event);

private:
    FirmnginOtaRequest request_;
    QueueHandle_t events_ = nullptr;
    SemaphoreHandle_t done_ = nullptr;
    std::atomic<bool> cancelled_{false};
    bool started_ = false;
    TaskHandle_t owner_ = nullptr;
    String error_;
    bool noUpdate_ = false;
    const esp_partition_t *partition_ = nullptr;
    static void task(void *context);
    bool execute();
    bool send(FirmnginOtaEvent::Kind kind, const char *status, const char *message,
              OTAAsyncState state = OTA_ASYNC_DOWNLOADING, int progress = -1);
};

#endif
