#include "sleep_utils.h"
#include "power_utils.h"


extern bool     gpsSleepActive; // currentBeacon->gpsEcoMode // true!
extern uint32_t lastGPSTime;
extern bool     gpsIsActive;


namespace SLEEP_Utils {

    void gpsSleep() {
        if (gpsSleepActive && gpsIsActive) {
            POWER_Utils::deactivateGPS();
            lastGPSTime = millis();
            //
            Serial.println("GPS SLEEPING");
            //
        }

    }

    void gpsWakeUp() {
        if (gpsSleepActive && !gpsIsActive) {
            POWER_Utils::activateGPS();
            //
            Serial.println("GPS WAKEUP");
            //
        }

    }

}