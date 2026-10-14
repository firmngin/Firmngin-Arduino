/*
 * MqttClientExample — Firmngin platform MQTT plus a second broker.
 *
 * Both connections use the ESP-MQTT client built into the ESP32 core.
 * The second broker reconnects on its own. Publish is QoS 1 with a persistent
 * session. Inbound messages are delivered from fngin.loop().
 */

#include <Arduino.h>
#include <firmngin.h>
#include <WiFi.h>

#define USER_MQTT_HOST "mqtt.example.com"
#define USER_MQTT_PORT 58884
#define USER_MQTT_CLIENT_ID "my-device-user-mqtt"
#define USER_MQTT_USER "mqtt_user"
#define USER_MQTT_PASS "mqtt_password"
#define USER_MQTT_SUB_TOPIC "devices/demo/command"
#define USER_MQTT_PUB_TOPIC "devices/demo/telemetry"

const char *WIFI_SSID = "YOUR_SSID";
const char *WIFI_PASS = "YOUR_PASSWORD";

Firmngin fngin(DEVICE_ID, DEVICE_KEY);

static unsigned long lastUserPublish = 0;

void onUserMqttMessage(const char *topic, const uint8_t *payload, unsigned int length)
{
    Serial.print("[user-mqtt] ");
    Serial.print(topic);
    Serial.print(" => ");
    for (unsigned int i = 0; i < length; i++)
        Serial.print((char)payload[i]);
    Serial.println();

    fngin.mqttClientPublish(USER_MQTT_PUB_TOPIC, "{\"ack\":true}");
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

    fngin.setDebug(true);
    fngin.setTimezone(7);
    fngin.on(DEVICE_STATUS, [](DeviceStates &ds) {
        Serial.print("[firmngin] status=");
        Serial.println(ds.state());
    });
    fngin.begin();

    fngin.mqttClientSetInsecure(true);
    fngin.mqttClientBegin(USER_MQTT_HOST, USER_MQTT_PORT,
                          USER_MQTT_CLIENT_ID, USER_MQTT_USER, USER_MQTT_PASS,
                          "devices/demo/lwt", "offline", onUserMqttMessage);
    fngin.mqttClientSubscribe(USER_MQTT_SUB_TOPIC, 1);
}

void loop()
{
    fngin.loop();

    if (!fngin.mqttClientConnected())
        return;

    unsigned long now = millis();
    if (now - lastUserPublish >= 15000)
    {
        lastUserPublish = now;
        String payload = "{\"temp\":25.5,\"ts\":" + String(now) + "}";
        fngin.mqttClientPublish(USER_MQTT_PUB_TOPIC, payload.c_str());
        fngin.pushEntity("temperature", "25.5");

        // Monitor message health
        uint32_t dropped = fngin.getDroppedMessageCount();
        uint32_t reconnects = fngin.getReconnectCount();
        if (dropped > 0 || reconnects > 1)
        {
            Serial.printf("[MONITOR] Dropped: %u, Reconnects: %u\n", dropped, reconnects);
        }
    }
}
