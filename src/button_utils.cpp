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
#include "board_pinout.h"
#include "button_utils.h"
#include "power_utils.h"
#include "display.h"

#ifdef BUTTON_PIN

    extern int              menuDisplay;
    extern uint32_t         displayTime;
    extern uint32_t         menuTime;

    int whichButton         = 0;

    namespace BUTTON_Utils {
        
        OneButton userButton = OneButton(BUTTON_PIN, true, true);

        #ifdef RPC_Electronics_1W_LoRa_GPS
            OneButton userButton2 = OneButton(BUTTON2_PIN, true, true);
            OneButton userButton3 = OneButton(BUTTON3_PIN, true, true);
            OneButton userButton4 = OneButton(BUTTON4_PIN, true, true);
        #endif

        static bool buttonSinglePressed;
        static bool buttonLongPressed;
        static bool buttonDoublePressed;
        static bool buttonMultiPressed;

        #ifdef RPC_Electronics_1W_LoRa_GPS
        static bool button2SinglePressed;
        static bool button3SinglePressed;
        static bool button4SinglePressed;
        #endif

        void singlePress() {
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

        void longPress() {
            menuTime = millis();
            KEYBOARD_Utils::rightArrow();
        }

        void doublePress() {
            displayToggle(true);
            menuTime = millis();
            if (menuDisplay == 0) {
                menuDisplay = 1;
            } else if (menuDisplay > 0) {
                menuDisplay = 0;
                displayTime = millis();
            }
        }

        void multiPress() {
            displayToggle(true);
            menuTime = millis();
            menuDisplay = 9000;
        }

        void isrSinglePress1() {
            buttonSinglePressed = true;
            whichButton = 1;
        }
        #ifdef RPC_Electronics_1W_LoRa_GPS
        void isrSinglePress2() {
            button2SinglePressed = true;
            whichButton = 2;
        }
        void isrSinglePress3() {
            button3SinglePressed = true;
            whichButton = 3;
        }
        void isrSinglePress4() {
            button4SinglePressed = true;
            whichButton = 4;
        }
        #endif

        void isrLongPress() {
            buttonLongPressed = true;
        }

        void isrDoublePress() {
            buttonDoublePressed = true;
        }

        void isrMultiPress() {
            buttonMultiPressed = true;
        }

        void IRAM_ATTR buttonIsr() {    //  Interrupt Service Routine
            userButton.tick();
            #ifdef RPC_Electronics_1W_LoRa_GPS
                userButton2.tick();
                userButton3.tick();
                userButton4.tick();
            #endif
        }

        void loop() {
            noInterrupts();
            userButton.tick();
            #ifdef RPC_Electronics_1W_LoRa_GPS
                userButton2.tick();
                userButton3.tick();
                userButton4.tick();
            #endif
            interrupts();

            if (buttonSinglePressed) {
                buttonSinglePressed = false;
                singlePress();                
            }
            #ifdef RPC_Electronics_1W_LoRa_GPS
            if (button2SinglePressed) {
                buttonSinglePressed = false;
                singlePress2();                
            }
            if (button3SinglePressed) {
                buttonSinglePressed = false;
                singlePress3();                
            }
            if (button4SinglePressed) {
                buttonSinglePressed = false;
                singlePress4();                
            }

            #endif
            if (buttonLongPressed) {
                buttonLongPressed = false;
                longPress();                
            }
            if (buttonDoublePressed) {
                buttonDoublePressed = false;
                doublePress();
            }
            if (buttonMultiPressed) {
                buttonMultiPressed = false;
                multiPress();                
            }
        }

        void setup() {
            userButton.attachClick(BUTTON_Utils::isrSinglePress1);
            userButton.attachLongPressStart(BUTTON_Utils::isrLongPress);
            userButton.attachDoubleClick(BUTTON_Utils::isrDoublePress);
            userButton.attachMultiClick(BUTTON_Utils::isrMultiPress);
            attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonIsr, FALLING);

            #ifdef RPC_Electronics_1W_LoRa_GPS
                userButton2.attachClick(BUTTON_Utils::isrSinglePress2);
                attachInterrupt(digitalPinToInterrupt(BUTTON2_PIN), buttonIsr, FALLING);

                userButton3.attachClick(BUTTON_Utils::isrSinglePress3);
                attachInterrupt(digitalPinToInterrupt(BUTTON3_PIN), buttonIsr, FALLING);

                userButton4.attachClick(BUTTON_Utils::isrSinglePress4);
                attachInterrupt(digitalPinToInterrupt(BUTTON4_PIN), buttonIsr, FALLING);
            #endif
        }

    }

#endif