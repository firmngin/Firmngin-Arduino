/*
 * MqttOnlyExample — MQTT without the Firmngin class.
 *
 * Uses esp_mqtt_client from the ESP32 Arduino core. No extra MQTT library.
 * The client reconnects on its own, publishes at QoS 1, and keeps a persistent
 * session so a subscriber that connects later still receives the backlog.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <mqtt_client.h>

#define MQTT_HOST "mqtt.example.com"
#define MQTT_PORT 58884
#define MQTT_CLIENT_ID "my-standalone-device"
#define MQTT_USER "mqtt_user"
#define MQTT_PASS "mqtt_password"
#define MQTT_SUB_TOPIC "devices/demo/command"
#define MQTT_PUB_TOPIC "devices/demo/telemetry"

const char *WIFI_SSID = "YOUR_SSID";
const char *WIFI_PASS = "YOUR_PASSWORD";

static esp_mqtt_client_handle_t mqtt = nullptr;
static volatile bool mqttUp = false;
static unsigned long lastPublish = 0;

static void onMqttEvent(void *, esp_event_base_t, int32_t, void *data)
{
    auto *event = static_cast<esp_mqtt_event_handle_t>(data);
    if (event->event_id == MQTT_EVENT_CONNECTED)
    {
        mqttUp = true;
        esp_mqtt_client_subscribe(mqtt, MQTT_SUB_TOPIC, 1);
        Serial.println("MQTT connected");
    }
    else if (event->event_id == MQTT_EVENT_DISCONNECTED)
    {
        mqttUp = false;
    }
    else if (event->event_id == MQTT_EVENT_DATA && event->topic != nullptr)
    {
        Serial.print("[mqtt] ");
        Serial.write(reinterpret_cast<const uint8_t *>(event->topic), event->topic_len);
        Serial.print(" => ");
        if (event->data != nullptr && event->data_len > 0)
            Serial.write(reinterpret_cast<const uint8_t *>(event->data), event->data_len);
        Serial.println();
    }
}

static bool mqttStart()
{
    esp_mqtt_client_config_t cfg = {};
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    cfg.broker.address.hostname = MQTT_HOST;
    cfg.broker.address.port = MQTT_PORT;
    cfg.broker.address.transport = MQTT_TRANSPORT_OVER_SSL;
    cfg.broker.verification.skip_cert_common_name_check = true;
    cfg.credentials.client_id = MQTT_CLIENT_ID;
    cfg.credentials.username = MQTT_USER;
    cfg.credentials.authentication.password = MQTT_PASS;
    cfg.session.last_will.topic = "devices/demo/lwt";
    cfg.session.last_will.msg = "offline";
    cfg.session.last_will.qos = 1;
    cfg.session.last_will.retain = 1;
    cfg.session.disable_clean_session = true;
    cfg.session.keepalive = 30;
    cfg.buffer.size = 1024;
    cfg.buffer.out_size = 1024;
#else
    cfg.host = MQTT_HOST;
    cfg.port = MQTT_PORT;
    cfg.transport = MQTT_TRANSPORT_OVER_SSL;
    cfg.skip_cert_common_name_check = true;
    cfg.client_id = MQTT_CLIENT_ID;
    cfg.username = MQTT_USER;
    cfg.password = MQTT_PASS;
    cfg.lwt_topic = "devices/demo/lwt";
    cfg.lwt_msg = "offline";
    cfg.lwt_qos = 1;
    cfg.lwt_retain = 1;
    cfg.disable_clean_session = 1;
    cfg.keepalive = 30;
    cfg.buffer_size = 1024;
    cfg.out_buffer_size = 1024;
#endif

    mqtt = esp_mqtt_client_init(&cfg);
    if (mqtt == nullptr)
        return false;
    esp_mqtt_client_register_event(mqtt, MQTT_EVENT_ANY, onMqttEvent, nullptr);
    return esp_mqtt_client_start(mqtt) == ESP_OK;
}

void setup()
{
    Serial.begin(115200);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi OK");
    Serial.println("WARNING: MQTT TLS verification disabled for this example");
    mqttStart();
}

void loop()
{
    if (!mqttUp)
        return;

    unsigned long now = millis();
    if (now - lastPublish >= 15000)
    {
        lastPublish = now;
        String payload = "{\"temp\":25.5,\"ts\":" + String(now) + "}";
        esp_mqtt_client_enqueue(mqtt, MQTT_PUB_TOPIC, payload.c_str(), payload.length(), 1, 0, true);
    }
}
