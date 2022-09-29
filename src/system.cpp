#include <Arduino.h>

/* ------------------------------------------------- */

// For WIFI
#include <WiFi.h>

/* ------------------------------------------------- */

// For OTA Updates
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

/* ------------------------------------------------- */

// For Telnet
#include <ESPTelnet.h>

/* ------------------------------------------------- */

// Provides:
//   const char* WIFI_SSID = "***";
//   const char* WIFI_PASS = "***";
#include "WiFiCredentials.h"

/* ------------------------------------------------- */

ESPTelnet telnet;
IPAddress ip;
const uint16_t TELNET_PORT = 23;

/* ------------------------------------------------- */

// WIFI
const uint64_t MILLIS_WIFI_REFRESH = 3600000;       // Time in milliseconds to refresh epoch from wifi
const uint64_t MILLIS_WIFI_CONNECTION_WAIT = 10000; // Time in milliseconds to wait after attempting to start wifi connection
uint64_t millis_wifi_connection_wait = 0;           // Time in milliseconds from when WiFi connection attempt started

/* ------------------------------------------------- */

void print(const char c)
{
    Serial.print(c);
    telnet.print(c);
}

/* ------------------------------------------------- */

void print(const String &str)
{
    Serial.print(str);
    telnet.print(str);
}

/* ------------------------------------------------- */

void println(const char c)
{
    Serial.println(c);
    telnet.println(c);
}

/* ------------------------------------------------- */

void println(const String &str)
{
    Serial.println(str);
    telnet.println(str);
}

/* ------------------------------------------------- */

void println()
{
    Serial.println();
    telnet.println();
}

/* ------------------------------------------------- */

bool connectedToWifi()
{
    return WiFi.status() == WL_CONNECTED;
}

/* ------------------------------------------------- */

void connectToWiFi()
{
    print("Attempting to connect to WPA SSID: ");
    println(WIFI_SSID);

    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    millis_wifi_connection_wait = millis();
}

/* ------------------------------------------------- */

void errorMsg(String error, bool restart = true)
{
    println(error);
    if (restart)
    {
        println("Rebooting now...");
        delay(2000);
        ESP.restart();
        delay(2000);
    }
}

/* ------------------------------------------------- */

// (optional) callback functions for telnet events
void onTelnetConnect(String ip)
{
    println("\nWelcome" + telnet.getIP());
    println("(Use ^] + q  to disconnect.)");
}

/* ------------------------------------------------- */

void onTelnetDisconnect(String ip)
{
    print("- Telnet: ");
    print(ip);
    println(" disconnected");
}

/* ------------------------------------------------- */

void onTelnetReconnect(String ip)
{
    print("- Telnet: ");
    print(ip);
    println(" reconnected");
}

/* ------------------------------------------------- */

void onTelnetConnectionAttempt(String ip)
{
    print("- Telnet: ");
    print(ip);
    println(" tried to connected");
}

/* ------------------------------------------------- */

void setupTelnet( void (*onInputRecieved)(String) )
{
    // passing on functions for various telnet events
    telnet.onConnect(onTelnetConnect);
    telnet.onConnectionAttempt(onTelnetConnectionAttempt);
    telnet.onReconnect(onTelnetReconnect);
    telnet.onDisconnect(onTelnetDisconnect);
    telnet.onInputReceived(onInputRecieved);


    print("- Telnet: ");
    if (telnet.begin(TELNET_PORT))
    {
        println("running");
    }
    else
    {
        println("error.");
        errorMsg("Will reboot...");
    }
}

/* ------------------------------------------------- */

void systemSetup ( void (*onInputRecieved)(String) )
{
    connectToWiFi();
    println("WIFI Connected");

    ArduinoOTA
        .onStart([]()
                 {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      println("Start updating " + type); })
        .onEnd([]()
               { println("\nEnd"); })
        .onProgress([](unsigned int progress, unsigned int total)
                    { printf("Progress: %u%%\r", (progress / (total / 100))); })
        .onError([](ota_error_t error)
                 {
      printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) println("Receive Failed");
      else if (error == OTA_END_ERROR) println("End Failed"); });

    ArduinoOTA.begin();
    println("ArduinoOTA Running");

    setupTelnet(onInputRecieved);
    println("Telnet Running");
}

/* ------------------------------------------------- */

void systemLoop()
{
    // If not connected to WIFI, attempt to connect
    if (!connectedToWifi() && (millis() - millis_wifi_connection_wait) > MILLIS_WIFI_CONNECTION_WAIT)
    {
        connectToWiFi();
    }

    ArduinoOTA.handle();

    telnet.loop();
}