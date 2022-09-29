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
const uint32_t LED_CHANNEL = 1;
const uint32_t LED_CHANNEL_WASHER = 2;
const uint32_t LED_CHANNEL_DRYER = 3;
// Pulses for oscilliscope
const uint32_t PROBE = 32;
// Inputs for Washer/Dryer current sensors
const uint32_t WASHER = 36;
const uint32_t DRYER = 39;

/* ------------------------------------------------- */

// Sample 10 times per 60Hz sine wave
const uint32_t SAMPLES_PER_HERTZ = 10;
const uint32_t SAMPLES_PER_SECOND = 60 * SAMPLES_PER_HERTZ;
const uint64_t MILLIS_SAMPLE_INTERVAL = 1000 / SAMPLES_PER_SECOND;
// Time to wait before considering device to be on
//  Set to 5 sec for now
const uint64_t MILLIS_HYSTERESIS_TURNED_ON = 1000 * 5;
// Time to wait before considering device to be off
//  Set to 5 min for now
//  TODO DEBUG Set to 5 sec for now
//  TODO const uint64_t MILLIS_HYSTERESIS_TURNED_OFF = 1000 * 60 * 5;
const uint64_t MILLIS_HYSTERESIS_TURNED_OFF = 1000 * 5;
const uint64_t MILLIS_HYSTERESIS_MAX = MILLIS_HYSTERESIS_TURNED_ON > MILLIS_HYSTERESIS_TURNED_OFF ? MILLIS_HYSTERESIS_TURNED_ON : MILLIS_HYSTERESIS_TURNED_OFF;
const int32_t HISTORY_LEN = MILLIS_HYSTERESIS_MAX / 1000;
// Threshold in millivolts for device to be considered on
const int32_t ON_THRESHOLD = 1770;

/* ------------------------------------------------- */

void pushMsg(const char *msg);
void onInputReceived(String str);

/* ------------------------------------------------- */

void pushMsg(const char *msg)
{
  for (uint8_t i = 0; i < RECIPIENTS_LEN; i++)
  {
    sendMsg(RECIPIENTS[i], msg);
  }
}

/* ------------------------------------------------- */

uint64_t millis_sample_interval_last = 0;

uint32_t samples_per_second_ndx = 0;
uint32_t samples_per_second_washer[SAMPLES_PER_SECOND];
uint32_t samples_per_second_dryer[SAMPLES_PER_SECOND];

uint32_t history_ndx = 0;
uint32_t history_washer[HISTORY_LEN];
uint32_t history_dryer[HISTORY_LEN];

// WASHER_OFF__DRYER_OFF
//   WASHER_OFF__DRYER_ON
//     Dryer turned on, nothing in washer
//   WASHER_ON__DRYER_OFF
//     Washer started, nothing in dryer
//   WASHER_ON__DRYER_ON
//     Dryer and washer started at same time

// WASHER_OFF__DRYER_ON
//   WASHER_OFF__DRYER_OFF
//     Nothing in washer, dryer finished
//   WASHER_ON__DRYER_OFF
//     Washer started same time that dryer finished
//   WASHER_ON__DRYER_ON
//     Washer started while dryer running

// WASHER_ON__DRYER_OFF
//   WASHER_ON__DRYER_ON
//     Dryer started while washer still running
//   WASHER_WAIT__DRYER_OFF
//     Washer finished ithout dryer running
//   WASHER_OFF__DRYER_ON
//     Washer finished and quickly loaded into dryer

// WASHER_ON__DRYER_ON
//   WASHER_ON__DRYER_OFF
//     Washer running while dryer finished
//   WASHER_WAIT__DRYER_OFF
//     Washer finished same time as dryer
//   WASHER_WAIT__DRYER_ON
//     Washer finished, waiting on dryer

// WASHER_WAIT__DRYER_OFF
//   WASHER_OFF__DRYER_ON
//     Dryer started after washer finished
//   WASHER_ON__DRYER_OFF
//     Washer restarted
//   WASHER_ON__DRYER_ON
//     Washer and dryer started at same time

// WASHER_WAIT__DRYER_ON
//   WASHER_ON__DRYER_ON
//     New washer load
//   WASHER_ON__DRYER_OFF
//     Washer rerunning
//   WASHER_WAIT__DRYER_OFF
//     Dryer finished

enum class State
{
  WASHER_OFF__DRYER_OFF,
  WASHER_OFF__DRYER_ON,
  WASHER_ON__DRYER_OFF,
  WASHER_ON__DRYER_ON,
  WASHER_WAIT__DRYER_OFF,
  WASHER_WAIT__DRYER_ON
};

const char *stateStr[] = {
    "Washer Off / Dryer Off",
    "Washer Off / Dryer On",
    "Washer On / Dryer Off",
    "Washer On / Dryer On",
    "Washer Wait / Dryer Off",
    "Washer Wait / Dryer On"};

State currentState = State::WASHER_OFF__DRYER_OFF;

/* ------------------------------------------------- */

bool haveSecondOfData()
{
  return samples_per_second_ndx >= SAMPLES_PER_SECOND;
}

void sampleProbes()
{
  if ((millis() - millis_sample_interval_last) > MILLIS_SAMPLE_INTERVAL)
  {

    millis_sample_interval_last = millis();

    // Reset index if past end of arrays
    if (haveSecondOfData())
    {
      samples_per_second_ndx = 0;
    }

    digitalWrite(PROBE, HIGH);
    samples_per_second_washer[samples_per_second_ndx] = analogReadMilliVolts(WASHER);
    samples_per_second_dryer[samples_per_second_ndx] = analogReadMilliVolts(DRYER);
    digitalWrite(PROBE, LOW);

    samples_per_second_ndx++;
  }
}

void updateHistory()
{
  if (haveSecondOfData())
  {
    uint32_t washer_max_millivolts = 0;
    uint32_t dryer_max_millivolts = 0;
    for (int32_t i = 0; i < SAMPLES_PER_SECOND; i++)
    {
      // Washer
      if (samples_per_second_washer[i] > washer_max_millivolts)
      {
        washer_max_millivolts = samples_per_second_washer[i];
      }
      // Dryer
      if (samples_per_second_dryer[i] > dryer_max_millivolts)
      {
        dryer_max_millivolts = samples_per_second_dryer[i];
      }
    }

    history_washer[history_ndx] = washer_max_millivolts;
    history_dryer[history_ndx] = dryer_max_millivolts;
    history_ndx++;
    if (history_ndx >= HISTORY_LEN)
    {
      history_ndx = 0;
    }
  }
}

bool washerOn()
{
  for (int32_t i = 0; i < HISTORY_LEN; i++)
  {
    if (history_washer[i] > ON_THRESHOLD)
    {
      return true;
    }
  }
  return false;
}

bool dryerOn()
{
  for (int32_t i = 0; i < HISTORY_LEN; i++)
  {
    if (history_dryer[i] > ON_THRESHOLD)
    {
      return true;
    }
  }
  return false;
}

void stateLoop()
{
  switch (currentState)
  {
  case State::WASHER_OFF__DRYER_OFF:
    if (washerOn() && dryerOn())
    {
      pushMsg("Washer and Dryer Started");
      currentState = State::WASHER_ON__DRYER_ON;
    }
    if (washerOn() && !dryerOn())
    {
      pushMsg("Washer Started");
      currentState = State::WASHER_ON__DRYER_OFF;
    }
    if (!washerOn() && dryerOn())
    {
      pushMsg("Dryer Started");
      currentState = State::WASHER_OFF__DRYER_ON;
    }
    if (!washerOn() && !dryerOn())
    {
    }
    break;

  case State::WASHER_OFF__DRYER_ON:
    if (washerOn() && dryerOn())
    {
      pushMsg("Washer Started");
      currentState = State::WASHER_ON__DRYER_ON;
    }
    if (washerOn() && !dryerOn())
    {
      pushMsg("Washer Started and Dryer Finished");
      currentState = State::WASHER_ON__DRYER_OFF;
    }
    if (!washerOn() && dryerOn())
    {
    }
    if (!washerOn() && !dryerOn())
    {
      pushMsg("Dryer Finished");
      currentState = State::WASHER_OFF__DRYER_OFF;
    }
    break;

  case State::WASHER_ON__DRYER_OFF:
    if (washerOn() && dryerOn())
    {
      pushMsg("Dryer Started");
      currentState = State::WASHER_ON__DRYER_ON;
    }
    if (washerOn() && !dryerOn())
    {
    }
    if (!washerOn() && dryerOn())
    {
      pushMsg("Washer Finished and Dryer Started");
      currentState = State::WASHER_OFF__DRYER_ON;
    }
    if (!washerOn() && !dryerOn())
    {
      pushMsg("Washer Waiting");
      currentState = State::WASHER_WAIT__DRYER_OFF;
    }
    break;

  case State::WASHER_ON__DRYER_ON:
    if (washerOn() && dryerOn())
    {
    }
    if (washerOn() && !dryerOn())
    {
      pushMsg("Dryer Finished");
      currentState = State::WASHER_ON__DRYER_OFF;
    }
    if (!washerOn() && dryerOn())
    {
      pushMsg("Washer Waiting and Dryer Running");
      currentState = State::WASHER_WAIT__DRYER_ON;
    }
    if (!washerOn() && !dryerOn())
    {
      pushMsg("Washer Waiting and Dryer Finished");
      currentState = State::WASHER_WAIT__DRYER_OFF;
    }
    break;

  case State::WASHER_WAIT__DRYER_OFF:
    if (washerOn() && dryerOn())
    {
      pushMsg("Washer Started and Dryer Started");
      currentState = State::WASHER_ON__DRYER_ON;
    }
    if (washerOn() && !dryerOn())
    {
      pushMsg("Washer Restarted");
      currentState = State::WASHER_ON__DRYER_OFF;
    }
    if (!washerOn() && dryerOn())
    {
      pushMsg("Washer Finished and Dryer Started");
      currentState = State::WASHER_OFF__DRYER_ON;
    }
    if (!washerOn() && !dryerOn())
    {
    }
    break;

  case State::WASHER_WAIT__DRYER_ON:
    if (washerOn() && dryerOn())
    {
      pushMsg("Washer Restarted and Dryer Running");
      currentState = State::WASHER_ON__DRYER_ON;
    }
    if (washerOn() && !dryerOn())
    {
      pushMsg("Washer Restarted and Dryer Finished");
      currentState = State::WASHER_ON__DRYER_OFF;
    }
    if (!washerOn() && dryerOn())
    {
    }
    if (!washerOn() && !dryerOn())
    {
      pushMsg("Washer Waiting and Dryer Finished");
      currentState = State::WASHER_WAIT__DRYER_OFF;
    }
    break;
  }
}

/* ------------------------------------------------- */

void onInputReceived(String str)
{
  if (str == "ping")
  {
    println("pong");
  }
  else if (str == "bye")
  {
    println("disconnecting you...");
    telnet.disconnectClient();
  }
  else if (str == "status")
  {
    println("State is " + (String)stateStr[(int)currentState]);
    println("Washer is " + (String)(washerOn() ? "on" : "off"));
    println("Dryer is " + (String)(dryerOn() ? "on" : "off"));
  }
  else
  {
    println("Do not recognize: " + str);
  }
}

/* ------------------------------------------------- */

void setup()
{
  Serial.begin(115200);

  // Setup IO
  pinMode(PROBE, OUTPUT);

  ledcSetup(LED_CHANNEL, 10, 8);
  ledcSetup(LED_CHANNEL_WASHER, 5, 8);
  ledcSetup(LED_CHANNEL_DRYER, 5, 8);

  ledcAttachPin(LED, LED_CHANNEL);
  ledcAttachPin(LED_WASHER, LED_CHANNEL_WASHER);
  ledcAttachPin(LED_DRYER, LED_CHANNEL_DRYER);

  ledcWrite(LED_CHANNEL, 128);
  ledcWrite(LED_CHANNEL_WASHER, 128);
  ledcWrite(LED_CHANNEL_DRYER, 128);

  systemSetup(onInputReceived);

  // Clear history
  for (int32_t i = 0; i < HISTORY_LEN; i++)
  {
    history_washer[i] = 0;
    history_dryer[i] = 0;
  }

  delay(1000);
  ledcWrite(LED_CHANNEL, 0);

  println("Setup Done");
}

/* ------------------------------------------------- */

void loop()
{

  systemLoop();

  sampleProbes();

  updateHistory();

  stateLoop();

  // Washer LED
  if (currentState == State::WASHER_ON__DRYER_OFF || currentState == State::WASHER_ON__DRYER_ON)
  {
    ledcWrite(LED_CHANNEL_WASHER, 255);
  }
  else if (currentState == State::WASHER_WAIT__DRYER_OFF || currentState == State::WASHER_WAIT__DRYER_ON)
  {
    ledcWrite(LED_CHANNEL_WASHER, 128);
  }
  else
  {
    ledcWrite(LED_CHANNEL_WASHER, 0);
  }

  // Dryer LED
  if (currentState == State::WASHER_OFF__DRYER_ON || currentState == State::WASHER_ON__DRYER_ON || currentState == State::WASHER_WAIT__DRYER_ON)
  {
    ledcWrite(LED_CHANNEL_DRYER, 255);
  }
  else
  {
    ledcWrite(LED_CHANNEL_DRYER, 0);
  }
}
