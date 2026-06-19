/*
 * MqttOnlyExample — Standalone MQTT without the Firmngin class.
 *
 * The Firmngin Arduino library is built on top of PubSubClient (Nick O'Leary) as its
 * MQTT dependency. A vendored copy lives under src/PubSubClient/ in this repo.
 *
 * This sketch uses that PubSubClient directly — no DEVICE_ID, DEVICE_KEY, or
 * #include <firmngin.h>. For Firmngin platform + a second broker, see MqttClientExample.
 */

#include <Arduino.h>
#include <PubSubClient/PubSubClient.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <WiFiClientSecure.h>
#endif

#define MQTT_HOST "mqtt.example.com"
#define MQTT_PORT 58884
#define MQTT_CLIENT_ID "my-standalone-device"
#define MQTT_USER "mqtt_user"
#define MQTT_PASS "mqtt_password"
#define MQTT_SUB_TOPIC "devices/demo/command"
#define MQTT_PUB_TOPIC "devices/demo/telemetry"

const char *WIFI_SSID = "YOUR_SSID";
const char *WIFI_PASS = "YOUR_PASSWORD";

WiFiClientSecure mqttNet;
PubSubClient mqtt(mqttNet);

static unsigned long lastReconnect = 0;
static unsigned long lastPublish = 0;

void onMessage(char *topic, uint8_t *payload, unsigned int length)
{
    Serial.print("[mqtt] ");
    Serial.print(topic);
    Serial.print(" => ");
    for (unsigned int i = 0; i < length; i++)
        Serial.print((char)payload[i]);
    Serial.println();
}

bool mqttConnect()
{
    if (mqtt.connected())
        return true;

    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setCallback(onMessage);
    mqtt.setBufferSize(1024);
    mqtt.setKeepAlive(30);
    mqtt.setSocketTimeout(10);

    Serial.print("MQTT connecting ");
    Serial.println(MQTT_HOST);

    const char *willTopic = "devices/demo/lwt";
    const char *willMsg = "offline";
    bool ok = mqtt.connect(
        MQTT_CLIENT_ID,
        MQTT_USER,
        MQTT_PASS,
        willTopic, 1, true, willMsg);

    if (!ok)
    {
        Serial.print("MQTT failed, rc=");
        Serial.println(mqtt.state());
        return false;
    }

    mqtt.subscribe(MQTT_SUB_TOPIC, 1);
    Serial.println("MQTT connected");
    return true;
}

void setup()
{
    Serial.begin(115200);

    mqttNet.setInsecure();

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi OK");

    mqttConnect();
}

void loop()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        mqtt.disconnect();
        mqttNet.stop();
        delay(500);
        return;
    }

    if (!mqtt.connected())
    {
        unsigned long now = millis();
        if (now - lastReconnect >= 5000)
        {
            lastReconnect = now;
            mqttConnect();
        }
    }
    else
    {
        mqtt.loop();

        unsigned long now = millis();
        if (now - lastPublish >= 15000)
        {
            lastPublish = now;
            String payload = "{\"temp\":25.5,\"ts\":" + String(now) + "}";
            mqtt.publish(MQTT_PUB_TOPIC, payload.c_str());
        }
    }
}
