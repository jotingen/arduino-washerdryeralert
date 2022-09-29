#ifndef SYSTEM_H
#define SYSTEM_H

#include <Arduino.h>
#include <ESPTelnet.h>

extern ESPTelnet telnet;

/* ------------------------------------------------- */

void print(const char c);

/* ------------------------------------------------- */

void print(const String &str);

/* ------------------------------------------------- */

void println(const char c);

/* ------------------------------------------------- */

void println(const String &str);
void println(const String &str);

/* ------------------------------------------------- */

void println();

/* ------------------------------------------------- */

void systemSetup ( void (*onInputRecieved)(String) );

/* ------------------------------------------------- */

void systemLoop();

/* ------------------------------------------------- */

#endif
