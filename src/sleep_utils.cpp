#include "sleep_utils.h"
#include "power_utils.h"


bool gpsIsActive = true;
extern bool gpsSleepActive;

namespace SLEEP_Utils {

    void gpsSleep() {
        if (gpsSleepActive && gpsIsActive) {
            POWER_Utils::deactivateGPS();
            gpsIsActive = false;
            //
            Serial.println("GPS SLEEPING");
            //
        }

    }

    void gpsWakeUp() {
        if (gpsSleepActive && !gpsIsActive) {
            POWER_Utils::activateGPS();
            gpsIsActive = true;
            //
            Serial.println("GPS WAKEUP");
            //
        }

    }

}