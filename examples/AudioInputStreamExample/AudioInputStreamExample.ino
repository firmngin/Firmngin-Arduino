/*******************************************************************************
 * Firmngin Audio Input Stream Example (INMP441 push-to-talk)
 *
 * Dashboard setup:
 * 1. Create an entity of type audio_input, Key = "microphone"
 * 2. Hold the button to record; release it to finalize a WAV in Firmngin/R2.
 *
 * INMP441 wiring:
 *   SCK/BCLK -> 26, WS/LRCL -> 25, SD -> 33
 *   L/R -> GND (left slot), VDD -> 3.3V, GND -> GND
 ******************************************************************************/

#include <firmngin.h>
#include <WiFi.h>

const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

#define MIC_BCLK 26
#define MIC_WS 25
#define MIC_SD 33
#define TALK_BUTTON 32

Firmngin fngin(DEVICE_ID, DEVICE_KEY);
Entity microphone("1");

ON_AUDIO(microphone, [](FirmnginAudioEvent event, const char *) {
    if (event == AUDIO_INPUT_ACCEPTED)
        Serial.println("[mic] recording accepted");
    else if (event == AUDIO_INPUT_SAVED)
        Serial.println("[mic] recording saved");
    else if (event == AUDIO_INPUT_ERROR)
        Serial.println("[mic] recording failed");
});

void setup()
{
    Serial.begin(115200);
    pinMode(TALK_BUTTON, INPUT_PULLUP);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
        delay(250);

    fngin.setTimezone(7);
    fngin.setAudioInput(microphone, INMP441)
        .pins(MIC_BCLK, MIC_WS, MIC_SD)
        .channel(FIRMNGIN_I2S_LEFT);
    fngin.begin();
}

void loop()
{
    fngin.loop();

    static bool wasPressed = false;
    const bool pressed = digitalRead(TALK_BUTTON) == LOW;
    if (pressed && !wasPressed)
        (void)fngin.startAudioInput(microphone);
    else if (!pressed && wasPressed)
        fngin.stopAudioInput();
    wasPressed = pressed;
}
