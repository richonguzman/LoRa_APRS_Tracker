#include "board_pinout.h"
#include "sleep_utils.h"
#include "power_utils.h"


extern uint32_t         lastGPSTime;
extern bool             gpsIsActive;

bool gpsShouldSleep     = false;


namespace SLEEP_Utils {

    void gpsSleep() {
        #ifdef HAS_GPS_CTRL
            if (gpsIsActive) {
                POWER_Utils::deactivateGPS();
                lastGPSTime = millis();
                //
                Serial.println("GPS SLEEPING");
                //
            }
        #endif
    }

    void gpsWakeUp() {
        #ifdef HAS_GPS_CTRL
            if (!gpsIsActive) {
                POWER_Utils::activateGPS();
                gpsShouldSleep = false;
                //
                Serial.println("GPS WAKEUP");
                //
            }
        #endif
    }

    void checkIfGPSShouldSleep() {
        if (gpsShouldSleep) {
            gpsSleep();
        }
    }

}