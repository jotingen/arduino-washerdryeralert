#ifndef PRINT_H
#define PRINT_H

#include <Arduino.h>

/* ------------------------------------------------- */
    
void print(const char c);

/* ------------------------------------------------- */

void print(const String &str);

/* ------------------------------------------------- */

void println(const char c);

/* ------------------------------------------------- */

void println(const String &str);

/* ------------------------------------------------- */

void println();

/* ------------------------------------------------- */

#endif