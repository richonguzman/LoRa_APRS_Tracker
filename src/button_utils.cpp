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

    // ---- T-LoRa-Pager rotary encoder (A/B quadrature + center press) ---------
    #ifdef LILYGO_T_LORA_PAGER
        static volatile int  rotaryPosition  = 0;
        static uint8_t       lastEncState    = 0;
        static uint32_t      lastEncTime     = 0;
        static constexpr uint32_t ENC_DEBOUNCE_MS = 5;

        static void IRAM_ATTR rotaryISR() {
            uint32_t now = millis();
            if (now - lastEncTime < ENC_DEBOUNCE_MS) return;
            lastEncTime = now;

            uint8_t a     = digitalRead(ROTARY_A);
            uint8_t b     = digitalRead(ROTARY_B);
            uint8_t state = (a << 1) | b;

            // Half-step quadrature: direction determined by state transition
            if      (lastEncState == 0b10 && state == 0b00) rotaryPosition++;
            else if (lastEncState == 0b00 && state == 0b10) rotaryPosition--;
            else if (lastEncState == 0b01 && state == 0b11) rotaryPosition++;
            else if (lastEncState == 0b11 && state == 0b01) rotaryPosition--;

            lastEncState = state;
        }
    #endif
    // -------------------------------------------------------------------------

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
                #ifdef LILYGO_T_LORA_PAGER
                    if (rotaryPosition != 0) {
                        int pos = rotaryPosition;
                        rotaryPosition = 0;
                        menuTime = millis();
                        if (pos > 0) {
                            KEYBOARD_Utils::downArrow();
                        } else {
                            KEYBOARD_Utils::upArrow();
                        }
                    }
                #endif
            }
        }

        void setup() {
            if (!Config.simplifiedTrackerMode) {
                #ifdef LILYGO_T_LORA_PAGER
                    // Center press: single = select (rightArrow), double = menu toggle
                    // Rotation handled by ISR — rotation is up/down, not the center button
                    userButton.attachClick(longPress1);       // single press = select
                    userButton.attachDoubleClick(doublePress1);
                    userButton.attachMultiClick(multiPress1);

                    pinMode(ROTARY_A, INPUT_PULLUP);
                    pinMode(ROTARY_B, INPUT_PULLUP);
                    lastEncState = (digitalRead(ROTARY_A) << 1) | digitalRead(ROTARY_B);
                    attachInterrupt(digitalPinToInterrupt(ROTARY_A), rotaryISR, CHANGE);
                    attachInterrupt(digitalPinToInterrupt(ROTARY_B), rotaryISR, CHANGE);
                #else
                userButton.attachClick(singlePress1);
                userButton.attachLongPressStart(longPress1);
                userButton.attachDoubleClick(doublePress1);
                userButton.attachMultiClick(multiPress1);
                #ifdef RPC_Electronics_1W_LoRa_GPS
                    userButton2.attachClick(singlePress2);
                    userButton3.attachClick(singlePress3);
                    userButton4.attachClick(singlePress4);
                #endif
                #endif // LILYGO_T_LORA_PAGER
            }
        }

    }

#endif