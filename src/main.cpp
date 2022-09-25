#include "system.h"
#include "messaging.h"

#include <Arduino.h>

/* ------------------------------------------------- */

// Provides:
//   char* RECIPIENTS[] = {};
//   int RECIPIENTS_LEN = sizeof(RECIPIENTS)/sizeof(RECIPIENTS[0]);
#include "Recipients.h"

/* ------------------------------------------------- */

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

/* ------------------------------------------------- */

bool connectedToWifi();
void connectToWiFi();
void pushMsg();

/* ------------------------------------------------- */

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

/* ------------------------------------------------- */

void pushMsg()
{
  for (uint8_t i = 0; i < RECIPIENTS_LEN; i++)
  {
    sendMsg((char *)RECIPIENTS[i], (char *)"Testing");
  }
}

/* ------------------------------------------------- */

void setup()
{
  Serial.begin(115200);

  // Setup IO
  pinMode(LED, OUTPUT);
  pinMode(PROBE, OUTPUT);
  pinMode(LED_WASHER, OUTPUT);
  pinMode(LED_DRYER, OUTPUT);

  digitalWrite(LED, HIGH);

  systemSetup();

  delay(1000);
  digitalWrite(LED, LOW);

  println("Setup Done");
}

/* ------------------------------------------------- */

int count = 0;
void loop()
{

  systemLoop();

  digitalWrite(PROBE, HIGH);
  println((String)analogReadMilliVolts(WASHER));
  digitalWrite(PROBE, LOW);

  digitalWrite(LED_WASHER, count % 2 == 0);
  digitalWrite(LED_DRYER, count % 3 == 0);

  count++;

  delay(1000);
}
