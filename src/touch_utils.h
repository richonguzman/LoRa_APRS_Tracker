#ifndef TOUCH_UTILS_H_
#define TOUCH_UTILS_H_

#include <Arduino.h>


struct TouchButton {
    int Xmin;
    int Xmax;
    int Ymin;
    int Ymax;
    String label;       // Optional: for button text or identification
    int color;
    void (*action)();   // Pointer to a function for button press action
};


namespace TOUCH_Utils {

    void loop();
    void setup();

}

#endif