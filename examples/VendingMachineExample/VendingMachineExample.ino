/*
 * Firmngin Vending Machine Example
 *
 * Serial-debug only: no entities, no GPIO.
 * Use with a device whose business mode is set to Vending.
 *
 * Flow:
 * 1. Connect WiFi + Firmngin
 * 2. ON_INIT prints service vs vending mode
 * 3. After guest pays, Firmngin delivers SKUs to dispense
 * 4. ON_DISPENSES prints each SKU to Serial (replace with motor/servo later)
 *
 * Setup:
 * 1. Create keys.h (DEVICE_ID / DEVICE_KEY + mTLS certs)
 * 2. Set WiFi credentials below
 * 3. Bind device to merchant, set Business Mode = Vending
 * 4. Set Device Stock SKUs to match catalog item SKUs
 */

#include <Arduino.h>
#include <firmngin.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

Firmngin fngin(DEVICE_ID, DEVICE_KEY);

ON_INIT(init)
{
  Serial.println("========== INIT ==========");
  Serial.print("Device Mode:      ");
  Serial.println(init.isVendingMode() ? "vending" : "service");
  Serial.print("Status:    ");
  Serial.println(init.merchantStatus());
  Serial.println("==========================");
}

ON_DISPENSES(d)
{
  Serial.println("========== DISPENSE ==========");
  if (!d.isValid())
  {
    Serial.println("Invalid dispense payload");
    Serial.println(d.metadata());
    Serial.println("===============================");
    return;
  }

  Serial.print("Items: ");
  Serial.println(d.itemCount());
  for (int i = 0; i < d.itemCount(); i++)
  {
    Serial.print("  [");
    Serial.print(i);
    Serial.print("] SKU=");
    Serial.println(d.skuAt(i));
  }
  Serial.println("===============================");
}

ON_PAYMENTS(p)
{
  Serial.println("---------- PAYMENT ----------");
  if (p.isPending())
  {
    Serial.println("Status: pending");
  }
  if (p.isSuccess())
  {
    Serial.println("Status: success (expect dispense next)");
  }
  if (p.isPrePaid())
  {
    Serial.println("Type: prepaid");
  }
  if (p.isPostPaid())
  {
    Serial.println("Type: postpaid (not used for vending)");
  }
  Serial.print("Item:  ");
  Serial.println(p.itemTitle());
  Serial.print("Order: ");
  Serial.println(p.orderId());
  Serial.print("Qty:   ");
  Serial.println(p.quantity());
  Serial.println("-----------------------------");
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi connected, IP=");
  Serial.println(WiFi.localIP());

  fngin.setDebug(true);
  fngin.setTimezone(7);
  fngin.begin();
}

void loop()
{
  fngin.loop();
}
