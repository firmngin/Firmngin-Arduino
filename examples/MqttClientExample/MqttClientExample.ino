/*
 * MqttClientExample — Firmngin platform MQTT plus a second broker via mqttClient().
 *
 * Firmngin is built on top of PubSubClient (vendored under src/PubSubClient/). Platform
 * traffic uses an internal PubSubClient; mqttClient() exposes a separate stack with the
 * full PubSubClient API for your own broker.
 *
 * Two independent connections:
 * - Firmngin: fngin.begin() / fngin.loop() (reconnect, states, entities, OTA, …)
 * - User:     fngin.mqttClient*() — you manage connect / subscribe / publish / loop
 *
 * MQTT-only without Firmngin credentials: see MqttOnlyExample.
 */

#include <Arduino.h>
#include <firmngin.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

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

static unsigned long lastUserMqttAttempt = 0;
static unsigned long lastUserPublish = 0;
static const unsigned long USER_MQTT_RETRY_MS = 5000;
static const unsigned long USER_PUBLISH_INTERVAL_MS = 15000;

void onUserMqttMessage(char *topic, uint8_t *payload, unsigned int length)
{
    Serial.print("[user-mqtt] ");
    Serial.print(topic);
    Serial.print(" => ");
    for (unsigned int i = 0; i < length; i++)
        Serial.print((char)payload[i]);
    Serial.println();

    String reply = "{\"ack\":true}";
    fngin.mqttClient().publish(USER_MQTT_PUB_TOPIC, reply.c_str());
}

bool connectUserMqtt()
{
    if (fngin.mqttClient().connected())
        return true;

    fngin.mqttClientSetBufferSize(1024);
    fngin.mqttClientSetInsecure(true);

    fngin.mqttClientPrepare();

    PubSubClient &mqtt = fngin.mqttClient();
    mqtt.setServer(USER_MQTT_HOST, USER_MQTT_PORT);
    mqtt.setCallback(onUserMqttMessage);

    Serial.print("[user-mqtt] connecting ");
    Serial.println(USER_MQTT_HOST);

    const char *willTopic = "devices/demo/lwt";
    const char *willMsg = "offline";
    bool ok = mqtt.connect(
        USER_MQTT_CLIENT_ID,
        USER_MQTT_USER,
        USER_MQTT_PASS,
        willTopic, 1, true, willMsg);

    if (!ok)
    {
        Serial.print("[user-mqtt] failed, rc=");
        Serial.println(mqtt.state());
        return false;
    }

    mqtt.subscribe(USER_MQTT_SUB_TOPIC, 1);

    Serial.println("[user-mqtt] connected & subscribed");
    return true;
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
    connectUserMqtt();
}

void loop()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        fngin.mqttClientDisconnect();
        delay(500);
        return;
    }

    fngin.loop();

    if (!fngin.mqttClient().connected())
    {
        unsigned long now = millis();
        if (now - lastUserMqttAttempt >= USER_MQTT_RETRY_MS)
        {
            lastUserMqttAttempt = now;
            connectUserMqtt();
        }
    }
    else
    {
        fngin.mqttClientLoop();

        unsigned long now = millis();
        if (now - lastUserPublish >= USER_PUBLISH_INTERVAL_MS)
        {
            lastUserPublish = now;
            String payload = "{\"temp\":25.5,\"ts\":" + String(now) + "}";
            fngin.mqttClient().publish(USER_MQTT_PUB_TOPIC, payload.c_str());
            fngin.pushEntity("temperature", "25.5");
        }
    }
}
