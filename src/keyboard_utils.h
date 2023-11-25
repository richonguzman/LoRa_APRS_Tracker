#ifndef KEYBOARD_UTILS_H
#define KEYBOARD_UTILS_H

#include <Arduino.h>

namespace KEYBOARD_Utils {

  void upArrow();
  void downArrow();
  void leftArrow();
  void rightArrow();
  void processPressedKey(int key);  
  void read();
  void setup();
}

#endif
