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

#include "joystick_utils.h"
#include "configuration.h"
#include "keyboard_utils.h"
#include "board_pinout.h"
#include "button_utils.h"

extern  int                     menuDisplay;
extern  Configuration           Config;

bool    exitJoystickInterrupt  = false;

typedef void (*DirectionFunc)();

#ifdef HAS_JOYSTICK

    namespace JOYSTICK_Utils {

        int         debounceDelay       = 400;
        uint32_t    lastInterruptTime   = 0;

        bool checkLastJoystickInterrupTime() {
            if ((millis() - lastInterruptTime) > debounceDelay) {
                lastInterruptTime = millis();
                return true;
            } else {
                return false;
            }
        }

        bool checkMenuDisplayToExitInterrupt(int menu) {
            if (menu == 10 || menu == 120  || (menu >= 130 && menu <= 133) || menu == 200 || menu == 210 || menu == 1300 || menu == 1310 || (menu >= 2210 && menu <= 2212) || menu == 51 || (menu >= 50100 && menu <= 50101) || (menu >= 50110 && menu <= 50111) || menu == 9001) {
                return true;    // read / delete/ callsignIndex / loraIndex / brightness x 3 / readW / readW / delete / enter WiFiAP
            } else {
                return false;
            }
        }

        void loop() {   // for running process with SPIFFS outside interrupt
            if (checkMenuDisplayToExitInterrupt(menuDisplay) && exitJoystickInterrupt) BUTTON_Utils::longPress1();
        }

        void IRAM_ATTR joystickHandler(DirectionFunc directionFunc) {
            if (checkLastJoystickInterrupTime() && menuDisplay != 0) {
                if (checkMenuDisplayToExitInterrupt(menuDisplay) && directionFunc == BUTTON_Utils::longPress1) {
                    exitJoystickInterrupt = true;
                } else {
                    exitJoystickInterrupt = false;
                    directionFunc();
                }
            }
        }

        void IRAM_ATTR joystickUp() { joystickHandler(KEYBOARD_Utils::upArrow); }
        void IRAM_ATTR joystickDown() { joystickHandler(KEYBOARD_Utils::downArrow); }
        void IRAM_ATTR joystickLeft() { joystickHandler(KEYBOARD_Utils::leftArrow); }
        void IRAM_ATTR joystickRight() { joystickHandler(BUTTON_Utils::longPress1); }

        void setup() {
            if (!Config.simplifiedTrackerMode) {
                pinMode(JOYSTICK_CENTER, INPUT_PULLUP);
                pinMode(JOYSTICK_UP, INPUT_PULLUP);
                pinMode(JOYSTICK_DOWN, INPUT_PULLUP);
                pinMode(JOYSTICK_LEFT, INPUT_PULLUP);
                pinMode(JOYSTICK_RIGHT, INPUT_PULLUP);

                attachInterrupt(digitalPinToInterrupt(JOYSTICK_UP), joystickUp, FALLING);
                attachInterrupt(digitalPinToInterrupt(JOYSTICK_DOWN), joystickDown, FALLING);
                attachInterrupt(digitalPinToInterrupt(JOYSTICK_LEFT), joystickLeft, FALLING);
                attachInterrupt(digitalPinToInterrupt(JOYSTICK_RIGHT), joystickRight, FALLING);
            }
        }
    }

#endif