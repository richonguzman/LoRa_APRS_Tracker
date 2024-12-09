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


    namespace BUTTON_Utils {
        
        OneButton userButton = OneButton(BUTTON_PIN, true, true);

        static bool buttonSinglePressed;
        static bool buttonLongPressed;
        static bool buttonDoublePressed;
        static bool buttonMultiPressed;

        void singlePress() {
            menuTime = millis();
            KEYBOARD_Utils::downArrow();
        }

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

        void isrSinglePress() {
            buttonSinglePressed = true;
        }

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
        }

        void loop() {
            noInterrupts();
            userButton.tick();
            interrupts();

            if (buttonSinglePressed) {
                buttonSinglePressed = false;
                singlePress();                
            }
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
            userButton.attachClick(BUTTON_Utils::isrSinglePress);
            userButton.attachLongPressStart(BUTTON_Utils::isrLongPress);
            userButton.attachDoubleClick(BUTTON_Utils::isrDoublePress);
            userButton.attachMultiClick(BUTTON_Utils::isrMultiPress);
            attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonIsr, FALLING);
        }

    }

#endif