/*
 * Firmngin Generic Camera Example
 *
 * This example shows how to upload images from ANY camera source
 * to Firmngin backend for object detection automation.
 *
 * Use this when you have:
 *   - External camera module (OV2640, OV5640, etc.)
 *   - Camera connected via SPI/UART
 *   - Image data from other source (SD card, network, etc.)
 *   - Pre-captured image buffer
 *
 * The key function is fngin.uploadImage() which accepts:
 *   - uint8_t *data: Pointer to image data (JPEG/PNG)
 *   - size_t len: Length of image data in bytes
 *   - const char *contentType: MIME type ("image/jpeg", "image/png")
 *   - UploadCallback onSuccess: Success callback
 *   - ErrorCallback onError: Error callback
 *
 * Setup:
 *   1. Copy keys.h from Firmngin dashboard to this sketch folder
 *   2. Update WiFi credentials
 *   3. Update DEVICE_ID and DEVICE_KEY
 *   4. Upload and run
 */

#include <Arduino.h>
#include <firmngin.h>

// WiFi credentials
const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

#define DEVICE_ID "YOUR_DEVICE_ID"
#define DEVICE_KEY "YOUR_DEVICE_SECRET_KEY"

Firmngin fngin(DEVICE_ID, DEVICE_KEY);

// ==================== EXAMPLE: Upload from SD Card ====================

#if defined(ESP32)
#include "SD.h"
#include "SPI.h"

#define SD_CS_PIN 5

bool uploadFromSDCard(const char *filename) {
    File file = SD.open(filename, FILE_READ);
    if (!file) {
        Serial.printf("[SD] Failed to open: %s\n", filename);
        return false;
    }

    size_t fileSize = file.size();
    Serial.printf("[SD] File size: %d bytes\n", fileSize);

    // Read file into buffer
    uint8_t *buffer = (uint8_t *)malloc(fileSize);
    if (!buffer) {
        Serial.println("[SD] Buffer allocation failed");
        file.close();
        return false;
    }

    file.read(buffer, fileSize);
    file.close();

    // Upload to backend
    bool result = fngin.uploadImage("sd_card_image",
        buffer,
        fileSize,
        "image/jpeg",
        [](const char *response) {
            Serial.printf("[UPLOAD] Success: %s\n", response);
        },
        [](int code, const char *message) {
            Serial.printf("[UPLOAD] Error %d: %s\n", code, message);
        }
    );

    free(buffer);
    return result;
}
#endif

// ==================== EXAMPLE: Upload from Memory Buffer ====================

// Sample JPEG image (smallest possible: 1x1 pixel)
// In real use, this would come from your camera
const uint8_t sample_jpeg[] = {
    0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01,
    0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0xFF, 0xD9
};
const size_t sample_jpeg_len = sizeof(sample_jpeg);

void uploadSampleImage() {
    Serial.println("[EXAMPLE] Uploading sample image...");
    
    bool sent = fngin.uploadImage("sample_image",
        (uint8_t *)sample_jpeg,
        sample_jpeg_len,
        "image/jpeg",
        [](const char *response) {
            Serial.printf("[UPLOAD] Success: %s\n", response);
        },
        [](int code, const char *message) {
            Serial.printf("[UPLOAD] Error %d: %s\n", code, message);
        }
    );

    if (sent) {
        Serial.println("[UPLOAD] Request sent successfully");
    } else {
        Serial.println("[UPLOAD] Failed to send request");
    }
}

// ==================== EXAMPLE: Custom Camera Class Interface ====================

/*
 * Implement this interface for your camera module:
 *
 * class MyCamera {
 * public:
 *     bool begin();                    // Initialize camera
 *     bool capture();                  // Capture image to internal buffer
 *     uint8_t* getData();              // Get image data pointer
 *     size_t getSize();                // Get image data size
 *     const char* getContentType();    // Get MIME type ("image/jpeg")
 *     void release();                  // Release buffer
 * };
 *
 * Then use:
 *
 * MyCamera camera;
 *
 * void captureAndUpload() {
 *     if (camera.capture()) {
 *         fngin.uploadImage(
 *             camera.getData(),
 *             camera.getSize(),
 *             camera.getContentType(),
 *             onSuccess,
 *             onError
 *         );
 *         camera.release();
 *     }
 * }
 */

// ==================== ENTITY COMMANDS ====================

Entity uploadTrigger("upload_trigger");

ON_ENTITY(uploadTrigger, [](EntityCommand &cmd) {
    Serial.printf("[ENTITY] Upload triggered: %s\n", cmd.value().c_str());
    
    // Example: upload from SD card
    #if defined(ESP32)
    if (cmd.value() == "sd") {
        uploadFromSDCard("/image.jpg");
    }
    #endif
    
    // Example: upload sample image
    if (cmd.value() == "sample") {
        uploadSampleImage();
    }
});

// ==================== SETUP ====================

void setup() {
    Serial.begin(115200);
    Serial.println("\n=============================");
    Serial.println("  Firmngin Generic Camera");
    Serial.println("=============================\n");

    // Connect to WiFi
    WiFi.begin(ssid, password);
    Serial.print("[WiFi] Connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n[WiFi] Connected!");

    // Initialize SD card (ESP32 only)
    #if defined(ESP32)
    if (SD.begin(SD_CS_PIN)) {
        Serial.println("[SD] Card initialized");
    } else {
        Serial.println("[SD] Card init failed");
    }
    #endif

    // Configure Firmngin
    fngin.setDebug(true);
    fngin.setFirmwareInfo("1.0.0", "ESP32", "esp32:esp32:generic");
    fngin.setTimezone(7);

    // Initialize connection
    fngin.begin();

    Serial.println("[SETUP] Ready!\n");
}

// ==================== LOOP ====================

void loop() {
    fngin.loop();

    // Upload every 60 seconds
    static unsigned long lastUpload = 0;
    if (millis() - lastUpload > 60000) {
        lastUpload = millis();
        uploadSampleImage();
    }
}
