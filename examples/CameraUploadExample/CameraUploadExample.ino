/*
 * Firmngin Camera Upload Example (ESP32-CAM)
 *
 * This example shows how to capture images from ESP32-CAM (AI-Thinker)
 * and upload them to Firmngin backend for object detection automation.
 *
 * Hardware:
 *   - ESP32-CAM (AI-Thinker) module
 *   - FTDI adapter for programming (GPIO 0 must be grounded during upload)
 *
 * Wiring (FTDI for upload):
 *   ESP32-CAM      FTDI
 *   ---------      ----
 *   5V         →   5V
 *   GND        →   GND
 *   U0R (GPIO3) →  TX
 *   U0T (GPIO1) →  RX
 *
 *   ⚠️  During upload: Connect GPIO 0 to GND
 *   ⚠️  During run:   Disconnect GPIO 0 from GND
 *
 * Board Settings (Arduino IDE / PlatformIO):
 *   Board: AI Thinker ESP32-CAM
 *   Upload Speed: 115200
 *   Partition Scheme: Huge APP (3MB No OTA / 1MB SPIFFS)
 *
 * Setup:
 *   1. Copy keys.h from Firmngin dashboard to this sketch folder
 *   2. Update WiFi credentials below
 *   3. Update DEVICE_ID and DEVICE_KEY
 *   4. Upload sketch (connect GPIO 0 to GND)
 *   5. Disconnect GPIO 0 and press RESET
 *   6. Open Serial Monitor at 115200 baud
 *
 * Flow:
 *   1. Device connects to WiFi and Firmngin MQTT
 *   2. Device captures image every 30 seconds (configurable)
 *   3. Image is uploaded via HTTP POST to backend
 *   4. Backend runs object detection (if automation configured)
 *   5. Results are stored in device state for downstream nodes
 */

#include <Arduino.h>
#include <firmngin.h>

// ESP32-CAM specific includes
#if defined(ESP32)
#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#endif

// ==================== CONFIGURATION ====================

#define DEVICE_ID "YOUR_DEVICE_ID"
#define DEVICE_KEY "YOUR_DEVICE_SECRET_KEY"

// WiFi credentials
const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

// Image upload interval (milliseconds)
#define UPLOAD_INTERVAL_MS 30000  // 30 seconds

// Image quality (0-63, lower = higher quality)
#define CAMERA_QUALITY 10

// Image resolution
// FRAMESIZE_QQVGA  160x120
// FRAMESIZE_QVGA   320x240
// FRAMESIZE_VGA    640x480
// FRAMESIZE_SVGA   800x600
// FRAMESIZE_XGA    1024x768
// FRAMESIZE_UXGA   1600x1200 (ESP32-CAM only)
#define CAMERA_RESOLUTION FRAMESIZE_VGA

// ==================== AI-THINKER ESP32-CAM PINOUT ====================

#if defined(ESP32)
// AI-Thinker ESP32-CAM pin definitions
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Flash LED (built-in on AI-Thinker)
#define FLASH_LED_PIN      4
#endif

// ==================== GLOBAL OBJECTS ====================

Firmngin fngin(DEVICE_ID, DEVICE_KEY);

// Track upload status
bool imageReady = false;
camera_fb_t *lastFrame = NULL;
unsigned long lastUploadTime = 0;
unsigned long uploadCount = 0;
unsigned long errorCount = 0;

// ==================== CAMERA FUNCTIONS ====================

bool initCamera() {
#if defined(ESP32)
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = CAMERA_QUALITY;
    config.fb_count = 2;  // Double buffer for smooth capture

    // Use higher quality if PSRAM available
    if (psramFound()) {
        config.frame_size = CAMERA_RESOLUTION;
        config.jpeg_quality = CAMERA_QUALITY;
        config.fb_count = 2;
        Serial.println("[CAM] PSRAM found, using high quality");
    } else {
        config.frame_size = FRAMESIZE_SVGA;
        config.jpeg_quality = 15;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_DRAM;
        Serial.println("[CAM] No PSRAM, using lower quality");
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[CAM] Init failed: 0x%x\n", err);
        return false;
    }

    // Configure camera settings
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        s->set_brightness(s, 1);     // -2 to 2
        s->set_contrast(s, 1);       // -2 to 2
        s->set_saturation(s, 0);     // -2 to 2
        s->set_whitebal(s, 1);       // auto white balance
        s->set_awb_gain(s, 1);       // auto white balance gain
        s->set_wb_mode(s, 0);        // white balance mode
        s->set_exposure_ctrl(s, 1);  // auto exposure
        s->set_aec2(s, 1);           // auto exposure DSP
        s->set_gain_ctrl(s, 1);      // auto gain
        s->set_agc_gain(s, 0);       // gain ceiling
        s->set_gainceiling(s, (gainceiling_t)6);
        s->set_bpc(s, 1);            // bad pixel correction
        s->set_wpc(s, 1);            // white pixel correction
        s->set_lenc(s, 1);           // lens correction
    }

    Serial.println("[CAM] Camera initialized successfully");
    return true;

#else
    Serial.println("[CAM] Camera not supported on this platform");
    return false;
#endif
}

bool captureImage() {
#if defined(ESP32)
    // Release previous frame if exists
    if (lastFrame) {
        esp_camera_fb_return(lastFrame);
        lastFrame = NULL;
    }

    // Capture new frame
    lastFrame = esp_camera_fb_get();
    if (!lastFrame) {
        Serial.println("[CAM] Capture failed");
        return false;
    }

    Serial.printf("[CAM] Captured: %d bytes, %dx%d\n", 
                  lastFrame->len, lastFrame->width, lastFrame->height);
    imageReady = true;
    return true;
#else
    return false;
#endif
}

// ==================== UPLOAD CALLBACKS ====================

void onUploadSuccess(const char *response) {
    uploadCount++;
    Serial.printf("[UPLOAD] Success (#%lu): %s\n", uploadCount, response);
    
    // Push upload status to entity
    fngin.pushEntity("camera_status", "online");
    fngin.pushEntity("last_capture_size", String(lastFrame ? lastFrame->len : 0).c_str());
}

void onUploadError(int code, const char *message) {
    errorCount++;
    Serial.printf("[UPLOAD] Error (#%lu): %d - %s\n", errorCount, code, message);
    
    // Push error status to entity
    fngin.pushEntity("camera_status", "error");
}

// ==================== AUTOMATION ENTITY ====================

Entity cameraTrigger("camera_capture");

// Trigger capture via entity command from automation
ON_ENTITY(cameraTrigger, [](EntityCommand &cmd) {
    Serial.println("[AUTO] Camera capture triggered by automation");
    
    // Flash LED briefly when capturing
    digitalWrite(FLASH_LED_PIN, HIGH);
    delay(100);
    
    if (captureImage()) {
        // Upload immediately when triggered by automation
        fngin.uploadImage("camera_image",
            lastFrame->buf,
            lastFrame->len,
            "image/jpeg",
            onUploadSuccess,
            onUploadError
        );
    }
    
    digitalWrite(FLASH_LED_PIN, LOW);
});

// ==================== SETUP ====================

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=============================");
    Serial.println("  Firmngin Camera Upload");
    Serial.println("=============================\n");

    // Disable flash LED initially
    pinMode(FLASH_LED_PIN, OUTPUT);
    digitalWrite(FLASH_LED_PIN, LOW);

    // ⚠️ Disable brownout detector (ESP32-CAM draws high current)
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    // Connect to WiFi
    WiFi.begin(ssid, password);
    Serial.print("[WiFi] Connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n[WiFi] Connected!");
    Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());

    // Initialize camera
    if (!initCamera()) {
        Serial.println("[FATAL] Camera init failed! Halting.");
        while (1) { delay(1000); }
    }

    // Setup Firmngin entity handler (BEFORE begin)
    // Camera trigger entity is already defined via ON_ENTITY macro above

    // Configure Firmngin
    fngin.setDebug(true);
    fngin.setFirmwareInfo("1.0.0", "ESP32-CAM", "esp32:esp32:esp32cam");
    fngin.setOTABaseURL("https://api.firmngin.dev/api/v1/ota");
    fngin.setTimezone(7);  // GMT+7 Indonesia

    // Initialize Firmngin MQTT connection
    fngin.begin();

    // Initial capture
    delay(2000);  // Wait for camera to stabilize
    captureImage();

    Serial.println("[SETUP] Ready! Waiting for MQTT connection...\n");
}

// ==================== LOOP ====================

void loop() {
    // Run Firmngin MQTT loop (handles incoming commands, OTA, etc.)
    fngin.loop();

    // Periodic image capture and upload
    unsigned long now = millis();
    if (now - lastUploadTime >= UPLOAD_INTERVAL_MS) {
        lastUploadTime = now;

        // Check if MQTT is connected
        if (!fngin.isConnected()) {
            Serial.println("[LOOP] MQTT not connected, skipping upload");
            return;
        }

        Serial.println("[LOOP] Capturing image...");

        // Capture new frame
        if (captureImage()) {
            // Flash LED during upload
            digitalWrite(FLASH_LED_PIN, HIGH);

            // Upload image to backend
            Serial.printf("[LOOP] Uploading %d bytes...\n", lastFrame->len);
            
            bool sent = fngin.uploadImage("camera_image",
                lastFrame->buf,
                lastFrame->len,
                "image/jpeg",
                onUploadSuccess,
                onUploadError
            );

            if (!sent) {
                Serial.println("[LOOP] Upload failed to start");
                errorCount++;
            }

            digitalWrite(FLASH_LED_PIN, LOW);
        } else {
            Serial.println("[LOOP] Capture failed");
        }

        // Push camera info to entities
        fngin.pushEntity("camera_status", "online");
        fngin.pushEntity("upload_count", String(uploadCount).c_str());
        fngin.pushEntity("error_count", String(errorCount).c_str());
    }
}
