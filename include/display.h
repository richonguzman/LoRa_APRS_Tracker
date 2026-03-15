/* Copyright (C) 2025 Ricardo Guzman - CA2RXU
 * 
 * This file is part of LoRa APRS Tracker.
 * 
 * LoRa APRS Tracker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or 
 * (at your option) any later version.
 * 
 * LoRa APRS Tracker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with LoRa APRS Tracker. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef DISPLAY_H_
#define DISPLAY_H_

// Hardware pins for LILYGO T-Deck Plus (LGFX / Legacy compatibility)
#define TFT_BL          42
#define TFT_CS          12
#define TFT_DC          11
#define TFT_RST         10
#define TFT_MISO        38
#define TFT_MOSI        41
#define TFT_SCLK        40

#include <Arduino.h>

#ifdef HAS_TFT
#include "LGFX_TDeck.h"
extern LGFX_TDeck tft;
#endif

void displaySetBrightness(uint8_t value);
void displaySetup();
void displayToggle(bool toggle);

void displayShow(const String& header, const String& line1, const String& line2, int wait = 0);
void displayShow(const String& header, const String& line1, const String& line2, const String& line3, const String& line4, const String& line5, int wait = 0);

void startupScreen(uint8_t index, const String& version);

#endif
