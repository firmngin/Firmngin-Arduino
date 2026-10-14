/*******************************************************************************
 * Firmngin Audio Stream Example
 *
 * Dashboard setup:
 * 1. Create an entity of type audio_output, Key = "speaker1"
 * 2. (Optional) more audio_output entities for additional speakers — see
 *    fngin.setAudioInterface(entity, chip) in the README
 *
 * Wiring (MAX98357A):
 *   BCLK -> 26, LRC -> 25, DIN -> 22, VIN -> 5V, GND -> GND
 *
 * website: https://firmngin.dev
 * author: (Arif) Firmngin.dev
 ******************************************************************************/

#include <firmngin.h>
#include <WiFi.h>

const char *ssid     = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

// Pin I2S (MAX98357A)
#define I2S_BCLK 26
#define I2S_LRC  25
#define I2S_DOUT 22

Firmngin fngin(DEVICE_ID, DEVICE_KEY);

// The audio_output entity from the dashboard. The Key must match the entity Key on
// the server: clips arrive tagged with that entity_key.
Entity speaker1("speaker1");

ON_AUDIO(speaker1, [](FirmnginAudioEvent event, const char *entityKey) {
    switch (event)
    {
    case AUDIO_READY:
        Serial.println("[audio] stream ready, standing by");
        break;
    case AUDIO_START:
        Serial.printf("[audio] playing %s\n", entityKey ? entityKey : "-");
        break;
    case AUDIO_END:
        Serial.printf("[audio] done %s\n", entityKey ? entityKey : "-");
        break;
    case AUDIO_FLUSH:
        Serial.println("[audio] clip replaced");
        break;
    case AUDIO_DISCONNECTED:
        Serial.println("[audio] disconnected, reconnecting...");
        break;
    case AUDIO_ERROR:
        Serial.printf("[audio] error: %s\n", entityKey ? entityKey : "unknown");
        break;
    default:
        break;
    }
});

void setup()
{
    Serial.begin(115200);

    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\nWiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());

    fngin.setDebug(true);
    fngin.setFirmwareInfo("1.0.0", "ESP32", "esp32:esp32:esp32");
    fngin.setTimezone(7);

    fngin.setAudioInterface(speaker1, MAX98357A).pins(I2S_BCLK, I2S_LRC, I2S_DOUT);

    fngin.begin();
}

void loop()
{
    fngin.loop();
}
