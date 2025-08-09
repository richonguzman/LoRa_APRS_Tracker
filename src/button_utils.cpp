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

#include <OneButton.h>
#include "keyboard_utils.h"
#include "configuration.h"
#include "board_pinout.h"
#include "button_utils.h"
#include "power_utils.h"
#include "display.h"

#ifdef BUTTON_PIN

    extern Configuration    Config;
    extern int              menuDisplay;
    extern uint32_t         displayTime;
    extern uint32_t         menuTime;


    namespace BUTTON_Utils {
        
        OneButton userButton = OneButton(BUTTON_PIN, true, true);

        #ifdef RPC_Electronics_1W_LoRa_GPS
            OneButton userButton2 = OneButton(BUTTON2_PIN, true, true);
            OneButton userButton3 = OneButton(BUTTON3_PIN, true, true);
            OneButton userButton4 = OneButton(BUTTON4_PIN, true, true);
        #endif

        void singlePress1() {
            menuTime = millis();
            KEYBOARD_Utils::downArrow();
        }
        #ifdef RPC_Electronics_1W_LoRa_GPS
            void singlePress2() {
                menuTime = millis();
                KEYBOARD_Utils::upArrow();
            }
            void singlePress3() {
                menuTime = millis();
                KEYBOARD_Utils::rightArrow();
            }
            void singlePress4() {
                menuTime = millis();
                KEYBOARD_Utils::leftArrow();
            }
        #endif

        void longPress1() {
            menuTime = millis();
            KEYBOARD_Utils::rightArrow();
        }

        void doublePress1() {
            displayToggle(true);
            menuTime = millis();
            if (menuDisplay == 0) {
                menuDisplay = 1;
            } else if (menuDisplay > 0) {
                menuDisplay = 0;
                displayTime = millis();
            }
        }

        void multiPress1() {
            displayToggle(true);
            menuTime = millis();
            menuDisplay = 9000;
        }

        void loop() {
            if (!Config.simplifiedTrackerMode) {
                userButton.tick();
                #ifdef RPC_Electronics_1W_LoRa_GPS
                    userButton2.tick();
                    userButton3.tick();
                    userButton4.tick();
                #endif
            }
        }

        void setup() {
            if (!Config.simplifiedTrackerMode) {
                userButton.attachClick(singlePress1);
                userButton.attachLongPressStart(longPress1);
                userButton.attachDoubleClick(doublePress1);
                userButton.attachMultiClick(multiPress1);
                #ifdef RPC_Electronics_1W_LoRa_GPS
                    userButton2.attachClick(singlePress2);
                    userButton3.attachClick(singlePress3);
                    userButton4.attachClick(singlePress4);
                #endif
            }
        }

    }

#endif