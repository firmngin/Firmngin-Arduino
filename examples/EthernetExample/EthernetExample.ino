/*
 * Firmngin Ethernet Example
 *
 * Brings up Ethernet through the ESP32 core (lwIP), then starts Firmngin.
 * MQTT uses that interface directly. There is no EthernetClient to inject.
 *
 * W5500 (SPI) needs Arduino-ESP32 3.x. A PHY on the internal MAC (LAN8720,
 * RTL8201, and similar) also works on core 2.0.x — use the begin() below it.
 * If the build exceeds the app partition, select a larger scheme (Huge APP).
 */

#include "keys.h"
#include <firmngin.h>
#include <ETH.h>
#if ESP_ARDUINO_VERSION_MAJOR >= 3
#include <SPI.h>
#endif

Firmngin fngin(DEVICE_ID, DEVICE_KEY);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
static const int ETH_SCK = 18;
static const int ETH_MISO = 19;
static const int ETH_MOSI = 23;
static const int ETH_CS = 5;
static const int ETH_IRQ = 4;
static const int ETH_RST = 14;
#endif

Entity relay1(1);
Entity relay2(2);

ON_ENTITY(relay1, [](EntityCommand &cmd) {
    digitalWrite(2, cmd.value() == "1" ? HIGH : LOW);
});

ON_ENTITY(relay2, [](EntityCommand &cmd) {
    digitalWrite(4, cmd.value() == "1" ? HIGH : LOW);
});

void setup()
{
    Serial.begin(115200);
    pinMode(2, OUTPUT);
    pinMode(4, OUTPUT);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
    SPI.begin(ETH_SCK, ETH_MISO, ETH_MOSI);
    ETH.begin(ETH_PHY_W5500, 1, ETH_CS, ETH_IRQ, ETH_RST, SPI);
#else
    // Core 2.0.x ETH API is the internal MAC. Pins: addr, power, mdc, mdio, phy, clock.
    ETH.begin(1, 16, 23, 18, ETH_PHY_LAN8720, ETH_CLOCK_GPIO0_IN);
#endif

    Serial.print("Waiting for Ethernet IP");
    while (ETH.localIP() == IPAddress(0, 0, 0, 0))
    {
        delay(250);
        Serial.print(".");
    }
    Serial.println();
    Serial.println(ETH.localIP());

    fngin.setDebug(true);
    fngin.begin();
}

void loop()
{
    fngin.loop();
}
