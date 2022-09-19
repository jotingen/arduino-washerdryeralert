#include <Arduino.h>

#include "WiFi.h"

#include "WiFiCredentials.h"

// WIFI
char ssid[] = WIFI_SSID;                            // Set SSID from WiFiCredentials.h
char pass[] = WIFI_PASS;                            // Set SSID from WiFiCredentials.h
const uint32_t MILLIS_WIFI_REFRESH = 3600000;       // Time in milliseconds to refresh epoch from wifi
const uint32_t MILLIS_WIFI_CONNECTION_WAIT = 10000; // Time in milliseconds to wait after starting wifi connection
uint64_t millis_wifi_start_connection = 0;          // Time in milliseconds from when WiFi connection attempt started

bool connectedToWifi();
void connectToWiFi();

void setup() {
  // Open serial communications and wait for port to open:
    Serial.begin(115200);
    // while (!Serial)
    //     ;

  //Connect to WIFI
  void connectToWiFi();

  Serial.println("Setup Done");
}

void loop() {
  Serial.println("Test");
  // put your main code here, to run repeatedly:
}

bool connectedToWifi()
{
    return WiFi.status() == WL_CONNECTED;
}

void connectToWiFi()
{
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);

    // Connect to WPA/WPA2 network:
    WiFi.begin(ssid, pass);

    millis_wifi_start_connection = millis();
}