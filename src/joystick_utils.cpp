#include "joystick_utils.h"
#include "keyboard_utils.h"
#include "boards_pinout.h"

#ifdef HAS_JOYSTICK

    namespace JOYSTICK_Utils {

        int         debounceDelay       = 200; // 50 ms debounce time
        uint32_t    lastInterruptTime   = 0;

        bool checkLastJoystickInterrupTime() {
            if ((millis() - lastInterruptTime) > debounceDelay) {
                lastInterruptTime = millis();
                return true;
            } else {
                return false;
            }
        }

        void IRAM_ATTR joyStickUp() {
            if (checkLastJoystickInterrupTime()) KEYBOARD_Utils::upArrow();
        }

        void IRAM_ATTR joyStickDown() {
            if (checkLastJoystickInterrupTime()) KEYBOARD_Utils::downArrow();
        }

        void IRAM_ATTR joyStickLeft() {
            if (checkLastJoystickInterrupTime()) KEYBOARD_Utils::leftArrow();
        }

        void IRAM_ATTR joyStickRight() {
            if (checkLastJoystickInterrupTime()) KEYBOARD_Utils::rightArrow();
        }

        void setup() {
            pinMode(JOYSTICK_CENTER, INPUT_PULLUP);
            pinMode(JOYSTICK_UP, INPUT_PULLUP);
            pinMode(JOYSTICK_DOWN, INPUT_PULLUP);
            pinMode(JOYSTICK_LEFT, INPUT_PULLUP);
            pinMode(JOYSTICK_RIGHT, INPUT_PULLUP);

            attachInterrupt(digitalPinToInterrupt(JOYSTICK_UP), joyStickUp, FALLING);
            attachInterrupt(digitalPinToInterrupt(JOYSTICK_DOWN), joyStickDown, FALLING);
            attachInterrupt(digitalPinToInterrupt(JOYSTICK_LEFT), joyStickLeft, FALLING);
            attachInterrupt(digitalPinToInterrupt(JOYSTICK_RIGHT), joyStickRight, FALLING);
        }
    }

#endif