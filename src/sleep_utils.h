#ifndef SLEEP_UTILS_H_
#define SLEEP_UTILS_H_

#include <Arduino.h>

namespace SLEEP_Utils {

    void gpsSleep();
    void gpsWakeUp();
    void checkIfGPSShouldSleep();

}

#endif