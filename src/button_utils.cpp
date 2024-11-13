#include <OneButton.h>
#include "keyboard_utils.h"
#include "boards_pinout.h"
#include "button_utils.h"
#include "power_utils.h"
#include "display.h"

#ifdef BUTTON_PIN

    extern int              menuDisplay;
    extern uint32_t         displayTime;
    extern uint32_t         menuTime;


    namespace BUTTON_Utils {
        
        OneButton userButton = OneButton(BUTTON_PIN, true, true);

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

        void loop() {
            userButton.tick();
        }

        void setup() {
            userButton.attachClick(BUTTON_Utils::singlePress);
            userButton.attachLongPressStart(BUTTON_Utils::longPress);
            userButton.attachDoubleClick(BUTTON_Utils::doublePress);
            userButton.attachMultiClick(BUTTON_Utils::multiPress);

            /*userButton.attachClick(BUTTON_Utils::isrSinglePress);
            userButton.attachLongPressStart(BUTTON_Utils::isrLongPress);
            userButton.attachDoubleClick(BUTTON_Utils::isrDoublePress);
            userButton.attachMultiClick(BUTTON_Utils::isrMultiPress);
            attachInterrupt(BUTTON_PIN, buttonIsr, CHANGE);*/
        }

    }

#endif