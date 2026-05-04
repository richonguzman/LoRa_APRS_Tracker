/* Copyright (C) 2025 Ricardo Guzman - CA2RXU
 * 
 * This file is part of LoRa APRS Tracker.
 * 
 * LoRa APRS Tracker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or 
 * (at your option) any later version.
 * 
 * LoRa APRS Tracker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with LoRa APRS Tracker. If not, see <https://www.gnu.org/licenses/>.
 */

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