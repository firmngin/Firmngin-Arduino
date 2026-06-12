/*******************************************************************************
 * Firmngin Receive Entity Example
 *
 * Example using Entity class to receive commands from backend and control GPIO
 *
 * website: https://firmngin.dev
 * author: (Arif) Firmngin.dev
 ******************************************************************************/

#include "keys.h"
#include <firmngin.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

// WiFi credentials
const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

Firmngin fngin(DEVICE_ID, DEVICE_KEY);

// Entity objects as key references
Entity relay1("gpio_1");
Entity relay2("relay_2");
Entity led("pwm_1");

// Register callbacks for entity objects
ON_ENTITY(relay1, [](EntityCommand &cmd)
          { digitalWrite(2, cmd.value() == "1" ? HIGH : LOW); });

ON_ENTITY(relay2, [](EntityCommand &cmd)
          { digitalWrite(4, cmd.value() == "1" ? HIGH : LOW); });

ON_ENTITY(led, [](EntityCommand &cmd)
          { analogWrite(5, cmd.value().toInt()); });

// Custom callback for complex logic (using string key)
ON_ENTITY_S("status", [](EntityCommand &cmd)
            {
  Serial.print("Status entity received: ");
  Serial.println(cmd.value()); });

void setup()
{
  Serial.begin(115200);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  fngin.begin();
}

void loop()
{
  fngin.loop();
  delay(100);
}
