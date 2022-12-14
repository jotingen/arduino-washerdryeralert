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
const uint32_t LED_WASHER = 26;
const uint32_t LED_DRYER = 25;
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
//Number of samples to keep until adding to history
// Set to 4 per second
const uint32_t SAMPLES_LEN = SAMPLES_PER_SECOND / 4;
// Number of samples to see power on with before considering device to be on
//  Set to 1.5 sec for now
const uint64_t HISTORY_HYSTERESIS_TURNED_ON = 6;
// Time to wait before considering device to be off
//  Set to 5 min for now
const uint64_t HISTORY_HYSTERESIS_TURNED_OFF = 4 * 60 * 5;
const uint64_t HISTORY_LEN = HISTORY_HYSTERESIS_TURNED_ON > HISTORY_HYSTERESIS_TURNED_OFF ? HISTORY_HYSTERESIS_TURNED_ON : HISTORY_HYSTERESIS_TURNED_OFF;
// Threshold in millivolts for device to be considered on
const int32_t ON_THRESHOLD = 1780;

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

uint32_t samples_ndx = 0;
uint32_t samples_washer[SAMPLES_LEN];
uint32_t samples_dryer[SAMPLES_LEN];

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

State state;

/* ------------------------------------------------- */

bool haveSamplesReady()
{
  return samples_ndx >= SAMPLES_LEN;
}

void sampleProbes()
{
  if ((millis() - millis_sample_interval_last) > MILLIS_SAMPLE_INTERVAL)
  {

    millis_sample_interval_last = millis();

    // Reset index if past end of arrays
    if (haveSamplesReady())
    {
      samples_ndx = 0;
    }

    digitalWrite(PROBE, HIGH);
    samples_washer[samples_ndx] = analogReadMilliVolts(WASHER);
    samples_dryer[samples_ndx] = analogReadMilliVolts(DRYER);
    digitalWrite(PROBE, LOW);

    samples_ndx++;
  }
}

void updateHistory()
{
  if (haveSamplesReady())
  {
    uint32_t washer_max_millivolts = 0;
    uint32_t dryer_max_millivolts = 0;
    for (int32_t i = 0; i < SAMPLES_LEN; i++)
    {
      // Washer
      if (samples_washer[i] > washer_max_millivolts)
      {
        washer_max_millivolts = samples_washer[i];
      }
      // Dryer
      if (samples_dryer[i] > dryer_max_millivolts)
      {
        dryer_max_millivolts = samples_dryer[i];
      }
    }

    history_washer[history_ndx] = washer_max_millivolts;
    history_dryer[history_ndx] = dryer_max_millivolts;
    println((String)washer_max_millivolts + " " + (String)dryer_max_millivolts + " " + (String)stateStr[(int)state]);
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
    bool on = true;
    for (int32_t j = i; j < i + HISTORY_HYSTERESIS_TURNED_ON; j++)
    {
      if (history_washer[j % HISTORY_LEN] < ON_THRESHOLD)
      {
        on = false;
      }
    }
    if (on)
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
    bool on = true;
    for (int32_t j = i; j < i + HISTORY_HYSTERESIS_TURNED_ON; j++)
    {
      if (history_dryer[j % HISTORY_LEN] < ON_THRESHOLD)
      {
        on = false;
      }
    }
    if (on)
    {
      return true;
    }
  }
  return false;
}

State stateLoop(State state)
{
  switch (state)
  {
  case State::WASHER_OFF__DRYER_OFF:
    if (washerOn() && dryerOn())
    {
      pushMsg("Washer and Dryer Started");
      return State::WASHER_ON__DRYER_ON;
    }
    if (washerOn() && !dryerOn())
    {
      pushMsg("Washer Started");
      return State::WASHER_ON__DRYER_OFF;
    }
    if (!washerOn() && dryerOn())
    {
      pushMsg("Dryer Started");
      return State::WASHER_OFF__DRYER_ON;
    }
    if (!washerOn() && !dryerOn())
    {
    }
    break;

  case State::WASHER_OFF__DRYER_ON:
    if (washerOn() && dryerOn())
    {
      pushMsg("Washer Started");
      return State::WASHER_ON__DRYER_ON;
    }
    if (washerOn() && !dryerOn())
    {
      pushMsg("Washer Started and Dryer Finished");
      return State::WASHER_ON__DRYER_OFF;
    }
    if (!washerOn() && dryerOn())
    {
    }
    if (!washerOn() && !dryerOn())
    {
      pushMsg("Dryer Finished");
      return State::WASHER_OFF__DRYER_OFF;
    }
    break;

  case State::WASHER_ON__DRYER_OFF:
    if (washerOn() && dryerOn())
    {
      pushMsg("Dryer Started");
      return State::WASHER_ON__DRYER_ON;
    }
    if (washerOn() && !dryerOn())
    {
    }
    if (!washerOn() && dryerOn())
    {
      pushMsg("Washer Finished and Dryer Started");
      return State::WASHER_OFF__DRYER_ON;
    }
    if (!washerOn() && !dryerOn())
    {
      pushMsg("Washer Waiting");
      return State::WASHER_WAIT__DRYER_OFF;
    }
    break;

  case State::WASHER_ON__DRYER_ON:
    if (washerOn() && dryerOn())
    {
    }
    if (washerOn() && !dryerOn())
    {
      pushMsg("Dryer Finished");
      return State::WASHER_ON__DRYER_OFF;
    }
    if (!washerOn() && dryerOn())
    {
      pushMsg("Washer Waiting and Dryer Running");
      return State::WASHER_WAIT__DRYER_ON;
    }
    if (!washerOn() && !dryerOn())
    {
      pushMsg("Washer Waiting and Dryer Finished");
      return State::WASHER_WAIT__DRYER_OFF;
    }
    break;

  case State::WASHER_WAIT__DRYER_OFF:
    if (washerOn() && dryerOn())
    {
      pushMsg("Washer Started and Dryer Started");
      return State::WASHER_ON__DRYER_ON;
    }
    if (washerOn() && !dryerOn())
    {
      pushMsg("Washer Restarted");
      return State::WASHER_ON__DRYER_OFF;
    }
    if (!washerOn() && dryerOn())
    {
      pushMsg("Washer Finished and Dryer Started");
      return State::WASHER_OFF__DRYER_ON;
    }
    if (!washerOn() && !dryerOn())
    {
    }
    break;

  case State::WASHER_WAIT__DRYER_ON:
    if (washerOn() && dryerOn())
    {
      pushMsg("Washer Restarted and Dryer Running");
      return State::WASHER_ON__DRYER_ON;
    }
    if (washerOn() && !dryerOn())
    {
      pushMsg("Washer Restarted and Dryer Finished");
      return State::WASHER_ON__DRYER_OFF;
    }
    if (!washerOn() && dryerOn())
    {
    }
    if (!washerOn() && !dryerOn())
    {
      pushMsg("Washer Waiting and Dryer Finished");
      return State::WASHER_WAIT__DRYER_OFF;
    }
    break;
  }
  return state;
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
    println("State is " + (String)stateStr[(int)state]);
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

  state = State::WASHER_OFF__DRYER_OFF;

  delay(1000);

  ledcWrite(LED_CHANNEL, 0);
  ledcWrite(LED_CHANNEL_WASHER, 0);
  ledcWrite(LED_CHANNEL_DRYER, 0);

  println("Setup Done");
}

/* ------------------------------------------------- */

void loop()
{

  systemLoop();

  sampleProbes();

  updateHistory();

  State nextState = stateLoop(state);

  if (nextState != state)
  {
    state = nextState;
    // Washer LED
    if (state == State::WASHER_ON__DRYER_OFF || state == State::WASHER_ON__DRYER_ON)
    {
      ledcWrite(LED_CHANNEL_WASHER, 255);
    }
    else if (state == State::WASHER_WAIT__DRYER_OFF || state == State::WASHER_WAIT__DRYER_ON)
    {
      ledcWrite(LED_CHANNEL_WASHER, 128);
    }
    else
    {
      ledcWrite(LED_CHANNEL_WASHER, 0);
    }

    // Dryer LED
    if (state == State::WASHER_OFF__DRYER_ON || state == State::WASHER_ON__DRYER_ON || state == State::WASHER_WAIT__DRYER_ON)
    {
      ledcWrite(LED_CHANNEL_DRYER, 255);
    }
    else
    {
      ledcWrite(LED_CHANNEL_DRYER, 0);
    }
  }
}
