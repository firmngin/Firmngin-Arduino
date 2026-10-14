/*
 * Firmngin Terminal QR Example
 * Flow:
 * 1. Connect WiFi + Firmngin
 * 2. Type IDR amount (2000) then Enter to request a QR
 * 3. Type 'c' + Enter to cancel the QR this device has open
 * 4. The /qr response clears the display on paid / expired / canceled / idle
 *
 * Setup:
 * 1. Create keys.h (DEVICE_ID / DEVICE_KEY + mTLS certs)
 * 2. Set WiFi credentials below
 * 3. Bind device to merchant, set Business Mode = Terminal
 *
 * Author: Arif
 * Website: https://firmngin.dev
 */

#include <Arduino.h>
#include <firmngin.h>

#include <WiFi.h>

const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

Firmngin fngin(DEVICE_ID, DEVICE_KEY);

ON_INIT(init){
  Serial.print("Terminal mode: ");
  Serial.println(init.isTerminalMode() ? "yes" : "no");

  Serial.println();
  Serial.println("Enter the Amount (in IDR) to create a QR, 'c' to cancel.");
}

void setup(){
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  fngin.onTerminalQR([](TerminalQR &qr){
    if (qr.isPending() && qr.qrString.length() > 0){
      Serial.println("--- QRIS ---");
      Serial.print("QR: ");
      Serial.println(qr.qrString);
      Serial.print("Amount: ");
      Serial.println(qr.amount);
      Serial.print("Order ID:");
      Serial.println(qr.orderCode);
    } else if (qr.hasError()){
      Serial.print("Rejected: ");
      Serial.println(qr.status);
    } else if (qr.isPaid() || qr.isFailed() || qr.isIdle()){
      Serial.print("Status ");
      Serial.println(qr.status);
    }
  });

  fngin.setDebug(true);
  fngin.setTimezone(7);
  fngin.begin();
}

void loop()
{
  fngin.loop();
  if (!Serial.available())
    return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0)
    return;
  if (line == "c"){
    Serial.println(fngin.cancelTerminalQR() ? "Cancel requested" : "Cancel failed");
    return;
  }
  const char *error = fngin.setAmountTerminalQR(line);
  Serial.println(error ? error : "QR requested");
}
