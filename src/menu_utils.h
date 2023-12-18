#ifndef MENU_UTILS_H_
#define MENU_UTILS_H_

#include <Arduino.h>

namespace MENU_Utils {
    
    String checkBTType();
    String checkProcessActive(bool process);
    String checkScreenBrightness(int bright);
    void showOnScreen();

}

#endif