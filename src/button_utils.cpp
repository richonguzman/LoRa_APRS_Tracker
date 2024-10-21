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
    OneButton userButton            = OneButton(BUTTON_PIN, true, true);

    // Button interrupt handler.
    // Called every time the button is pressed or released
    void IRAM_ATTR buttonIsr()
    {
        userButton.tick();
    }

    // Set to true by the interrupt handler if the function should
    // be called in the main loop
    static bool bSinglePress;
    static bool bLongPress;
    static bool bDoublePress;
    static bool bMultiPress;

    // Functions called from the main loop
    static void singlePress() {
        menuTime = millis();
        KEYBOARD_Utils::downArrow();
    }

    static void longPress() {
        menuTime = millis();
        KEYBOARD_Utils::rightArrow();
    }

    static void doublePress() {
        displayToggle(true);
        menuTime = millis();
        if (menuDisplay == 0) {
            menuDisplay = 1;
        } else if (menuDisplay > 0) {
            menuDisplay = 0;
            displayTime = millis();
        }
    }

    static void multiPress() {
        displayToggle(true);
        menuTime = millis();
        menuDisplay = 9000;
    }

    // Functions called by the OneButton library from the interrupt handler
    // These just trigger one of the above functions to be called from the main loop
    void isrSinglePress()
    {
        bSinglePress = true;
    }

    void isrLongPress()
    {
        bLongPress = true;
    }

    void isrDoublePress()
    {
        bDoublePress = true;
    }

    void isrMultiPress()
    {
        bMultiPress = true;
    }

    // Called from the main loop
    void buttonLoop()
    {
        // Call the button tick function
        // Must be called with interrupts disabled as the OneButton
        // library is called from the interrupt handler
        noInterrupts();
        userButton.tick();
        interrupts();

        // Call the relevant function if triggered by the interrupt handler
        if( bSinglePress )
        {
            singlePress();
            bSinglePress = false;
        }

        if( bLongPress )
        {
            longPress();
            bLongPress = false;
        }

        if( bDoublePress )
        {
            doublePress();
            bDoublePress = false;
        }

        if( bMultiPress )
        {
            multiPress();
            bMultiPress = false;
        }
    }

    void buttonInit()
    {
        userButton.attachClick(BUTTON_Utils::isrSinglePress);
        userButton.attachLongPressStart(BUTTON_Utils::isrLongPress);
        userButton.attachDoubleClick(BUTTON_Utils::isrDoublePress);
        userButton.attachMultiClick(BUTTON_Utils::isrMultiPress);

        attachInterrupt(BUTTON_PIN, buttonIsr, CHANGE);
    }
}

#endif
