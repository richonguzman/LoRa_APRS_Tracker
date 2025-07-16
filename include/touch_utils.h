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

#ifndef TOUCH_UTILS_H_
#define TOUCH_UTILS_H_

#include <Arduino.h>


struct TouchButton {
    int Xmin;
    int Xmax;
    int Ymin;
    int Ymax;
    String label;       // Optional: for button text or identification
    int color;
    void (*action)();   // Pointer to a function for button press action
};


namespace TOUCH_Utils {

    void loop();
    void setup();

}

#endif