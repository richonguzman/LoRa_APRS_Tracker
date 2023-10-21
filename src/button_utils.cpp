#include "keyboard_utils.h"
#include "button_utils.h"
#include "display.h"

extern int              menuDisplay;
extern uint32_t         displayTime;
extern uint32_t         menuTime;

namespace BUTTON_Utils {

  void singlePress() {
    KEYBOARD_Utils::downArrow();
  }

  void longPress() {
    KEYBOARD_Utils::rightArrow();
  }

  void doublePress() {
    display_toggle(true);
    if (menuDisplay == 0) {
      menuDisplay = 1;
      menuTime = millis();
    } else if (menuDisplay > 0) {
      menuDisplay = 0;
      menuTime = millis();
      displayTime = millis();
    }
  }

}