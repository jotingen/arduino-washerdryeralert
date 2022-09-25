// Provide print overrides that will send
// to both the serial and telnet

#include "print.h"
#include <Arduino.h>
#include <ESPTelnet.h>

extern ESPTelnet telnet;

/* ------------------------------------------------- */
    
void print(const char c) {
  Serial.print(c);
  telnet.print(c);
}

/* ------------------------------------------------- */

void print(const String &str) {
  Serial.print(str);
  telnet.print(str);
}

/* ------------------------------------------------- */

void println(const char c) { 
  Serial.println(c);
  telnet.println(c);
}

/* ------------------------------------------------- */

void println(const String &str) { 
  Serial.println(str);
  telnet.println(str);
}

/* ------------------------------------------------- */

void println() { 
  Serial.println();
  telnet.println();
}

/* ------------------------------------------------- */
