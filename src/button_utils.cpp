#include "keyboard_utils.h"
#include "button_utils.h"
#include "display.h"

extern int              menuDisplay;
extern uint32_t         displayTime;
extern uint32_t         menuTime;

namespace BUTTON_Utils {

  void singlePress() {
    menuTime = millis();
    KEYBOARD_Utils::downArrow();
  }

  void longPress() {
    menuTime = millis();
    KEYBOARD_Utils::rightArrow();
  }

  void doublePress() {
    display_toggle(true);
    menuTime = millis();
    if (menuDisplay == 0) {
      menuDisplay = 1;
    } else if (menuDisplay > 0) {
      menuDisplay = 0;
      displayTime = millis();
    }
  }

}