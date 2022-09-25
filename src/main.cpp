#include <Arduino.h>

// For WIFI
#include <WiFi.h>
// Provides:
//   const char* WIFI_SSID = "***";
//   const char* WIFI_PASS = "***";
#include "WiFiCredentials.h"

// For OTA Updates
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// For Email
#include <ESP_Mail_Client.h>
// Provides:
//   char* SMTP_HOST "smtp.gmail.com"
//   SMTP_PORT 465
//   char* AUTHOR_EMAIL "***@gmail.com"
//   char* AUTHOR_PASSWORD "***"
#include "EmailCredentials.h"
// Provides:
//   char* RECIPIENTS[] = {};
//   int RECIPIENTS_LEN = sizeof(RECIPIENTS)/sizeof(RECIPIENTS[0]);
#include "Recipients.h"
/* The SMTP Session object used for Email sending */
SMTPSession smtp;

// For telnet
#include <ESPTelnet.h>
ESPTelnet telnet;
IPAddress ip;
const uint16_t TELNET_PORT = 23;

// ESP LED
const uint32_t LED = 2;
const uint32_t LED_WASHER = 25;
const uint32_t LED_DRYER = 26;
const uint32_t LED_CHANNEL = 0;
const uint32_t LED_CHANNEL_WASHER = 1;
const uint32_t LED_CHANNEL_DRYER = 2;
// Pulses for oscilliscope
const uint32_t PROBE = 32;
// Inputs for Washer/Dryer current sensors
const uint32_t WASHER = 36;
const uint32_t DRYER = 39;

// WIFI
const uint32_t MILLIS_WIFI_REFRESH = 3600000;       // Time in milliseconds to refresh epoch from wifi
const uint32_t MILLIS_WIFI_CONNECTION_WAIT = 10000; // Time in milliseconds to wait after starting wifi connection
uint64_t millis_wifi_start_connection = 0;          // Time in milliseconds from when WiFi connection attempt started

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status);

bool connectedToWifi();
void connectToWiFi();
void pushMsg();

void errorMsg(String error, bool restart = true)
{
  Serial.println(error);
  if (restart)
  {
    Serial.println("Rebooting now...");
    delay(2000);
    ESP.restart();
    delay(2000);
  }
}

// (optional) callback functions for telnet events
void onTelnetConnect(String ip)
{
  Serial.print("- Telnet: ");
  Serial.print(ip);
  Serial.println(" connected");
  telnet.println("\nWelcome " + telnet.getIP());
  telnet.println("(Use ^] + q  to disconnect.)");
}

void onTelnetDisconnect(String ip)
{
  Serial.print("- Telnet: ");
  Serial.print(ip);
  Serial.println(" disconnected");
}

void onTelnetReconnect(String ip)
{
  Serial.print("- Telnet: ");
  Serial.print(ip);
  Serial.println(" reconnected");
}

void onTelnetConnectionAttempt(String ip)
{
  Serial.print("- Telnet: ");
  Serial.print(ip);
  Serial.println(" tried to connected");
}
void setupTelnet()
{
  // passing on functions for various telnet events
  telnet.onConnect(onTelnetConnect);
  telnet.onConnectionAttempt(onTelnetConnectionAttempt);
  telnet.onReconnect(onTelnetReconnect);
  telnet.onDisconnect(onTelnetDisconnect);

  // passing a lambda function
  telnet.onInputReceived([](String str)
                         {
    // checks for a certain command
    if (str == "ping") {
      telnet.println("> pong");
      Serial.println("- Telnet: pong");
    // disconnect the client
    } else if (str == "bye") {
      telnet.println("> disconnecting you...");
      telnet.disconnectClient();
      } });

  Serial.print("- Telnet: ");
  if (telnet.begin(TELNET_PORT))
  {
    Serial.println("running");
  }
  else
  {
    Serial.println("error.");
    errorMsg("Will reboot...");
  }
}

// void recvMsg(uint8_t *data, size_t len)
//{
//   WebSerial.println("Received Data...");
//   String d = "";
//   for (int i = 0; i < len; i++)
//   {
//     d += char(data[i]);
//   }
//   WebSerial.println(d);
//   if (d == "ON")
//   {
//     digitalWrite(LED, HIGH);
//   }
//   if (d == "OFF")
//   {
//     digitalWrite(LED, LOW);
//   }
//   if (d == "push")
//   {
//     pushMsg();
//   }
// }

void pushMsg()
{
  for (uint8_t i = 0; i < RECIPIENTS_LEN; i++)
  {
    /** Enable the debug via Serial port
     * none debug or 0
     * basic debug or 1
     */
    smtp.debug(1);

    /* Set the callback function to get the sending results */
    smtp.callback(smtpCallback);

    /* Declare the session config data */
    ESP_Mail_Session session;

    /* Set the session config */
    session.server.host_name = SMTP_HOST;
    session.server.port = SMTP_PORT;
    session.login.email = AUTHOR_EMAIL;
    session.login.password = AUTHOR_PASSWORD;
    session.login.user_domain = "";

    /* Declare the message class */
    SMTP_Message message;

    /* Set the message headers */
    message.sender.name = "Washer Dryer Alert";
    message.sender.email = AUTHOR_EMAIL;
    // message.subject = "Washer Done";
    message.addRecipient("Main", RECIPIENTS[i]);

    /*Send HTML message*/
    String htmlMsg = "Washer Done";
    message.html.content = htmlMsg.c_str();
    message.text.charSet = "us-ascii";
    message.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

    /* Connect to server with the session config */
    if (!smtp.connect(&session))
      return;

    /* Start sending Email and close the session */
    if (!MailClient.sendMail(&smtp, &message))
      Serial.println("Error sending Email, " + smtp.errorReason());
  }
}

void setup()
{
  Serial.begin(115200);

  //Setup IO
  pinMode(LED, OUTPUT);
  pinMode(PROBE, OUTPUT);
  pinMode(LED_WASHER, OUTPUT);
  pinMode(LED_DRYER, OUTPUT);

  digitalWrite(LED, HIGH);

  // Connect to WIFI
  connectToWiFi();
  while (!connectedToWifi())
  {
    delay(500);
  }
  Serial.println("WIFI Connected");

  ArduinoOTA
      .onStart([]()
               {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type); })
      .onEnd([]()
             { Serial.println("\nEnd"); })
      .onProgress([](unsigned int progress, unsigned int total)
                  { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); })
      .onError([](ota_error_t error)
               {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed"); });

  ArduinoOTA.begin();
  Serial.println("ArduinoOTA Running");

  setupTelnet();
  Serial.println("Telnet Running");

  delay(1000);
  digitalWrite(LED, LOW);

  Serial.println("Setup Done");
}

int count = 0;
void loop()
{
  ArduinoOTA.handle();

  telnet.loop();

  digitalWrite(PROBE, HIGH);
  telnet.println((String)analogReadMilliVolts(WASHER));
  digitalWrite(PROBE, LOW);

  digitalWrite(LED_WASHER, count%2==0);
  digitalWrite(LED_DRYER, count%3==0);

  count++;

  delay(1000);
}

bool connectedToWifi()
{
  return WiFi.status() == WL_CONNECTED;
}

void connectToWiFi()
{
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(WIFI_SSID);

  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  millis_wifi_start_connection = millis();
}

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status)
{
  /* Print the current status */
  Serial.println(status.info());

  /* Print the sending result */
  if (status.success())
  {
    Serial.println("----------------");
    ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount());
    ESP_MAIL_PRINTF("Message sent failled: %d\n", status.failedCount());
    Serial.println("----------------\n");
    struct tm dt;

    for (size_t i = 0; i < smtp.sendingResult.size(); i++)
    {
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(i);
      time_t ts = (time_t)result.timestamp;
      localtime_r(&ts, &dt);

      ESP_MAIL_PRINTF("Message No: %d\n", i + 1);
      ESP_MAIL_PRINTF("Status: %s\n", result.completed ? "success" : "failed");
      ESP_MAIL_PRINTF("Date/Time: %d/%d/%d %d:%d:%d\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
      ESP_MAIL_PRINTF("Recipient: %s\n", result.recipients);
      ESP_MAIL_PRINTF("Subject: %s\n", result.subject);
    }
    Serial.println("----------------\n");
  }
}