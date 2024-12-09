#ifndef MENU_UTILS_H_
#define MENU_UTILS_H_

#include <Arduino.h>

namespace MENU_Utils {
    
    const String checkBTType();
    const String checkProcessActive(const bool process);
    const String screenBrightnessAsString(const uint8_t bright);
    void  showOnScreen();

}

#endif