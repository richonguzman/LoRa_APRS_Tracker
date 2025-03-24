#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <Arduino.h>

void displaySetup();
void displayToggle(bool toggle);

void displayShow(const String& header, const String& line1, const String& line2, int wait = 0);
void displayShow(const String& header, const String& line1, const String& line2, const String& line3, const String& line4, const String& line5, int wait = 0);

void startupScreen(uint8_t index, const String& version);

void displayMessage(const String& sender, const String& message, bool next, int wait = 0);

#endif