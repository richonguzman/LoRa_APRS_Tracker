#include "joystick_utils.h"
#include "keyboard_utils.h"
#include "boards_pinout.h"

extern int menuDisplay;

#ifdef HAS_JOYSTICK

    namespace JOYSTICK_Utils {

        int         debounceDelay       = 300;
        uint32_t    lastInterruptTime   = 0;

        bool checkLastJoystickInterrupTime() {
            if ((millis() - lastInterruptTime) > debounceDelay) {
                lastInterruptTime = millis();
                return true;
            } else {
                return false;
            }
        }

        void IRAM_ATTR joystickHandler(void (*directionFunc)()) {
            if (checkLastJoystickInterrupTime() && menuDisplay != 0) directionFunc();
        }

        void IRAM_ATTR joystickUp() { joystickHandler(KEYBOARD_Utils::upArrow); }
        void IRAM_ATTR joystickDown() { joystickHandler(KEYBOARD_Utils::downArrow); }
        void IRAM_ATTR joystickLeft() { joystickHandler(KEYBOARD_Utils::leftArrow); }
        void IRAM_ATTR joystickRight() { joystickHandler(KEYBOARD_Utils::rightArrow); }

        void setup() {
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

#endif