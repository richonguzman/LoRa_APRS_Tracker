#ifndef MENU_UTILS_H_
#define MENU_UTILS_H_

#include <Arduino.h>

namespace MENU_Utils {
    
    String  BluetoothTypeAsString();
    String  stateAsString(bool process);
    String  checkScreenBrightness(uint8_t bright);
    void    showOnScreen();

}

#endif