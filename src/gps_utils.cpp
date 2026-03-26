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

#include <esp_log.h>
#include <NMEAGPS.h>
#include "TimeLib.h"
#include <sys/time.h>
#include <APRSPacketLib.h>
#include "smartbeacon_utils.h"
#include "configuration.h"
#include "station_utils.h"
#include "board_pinout.h"
#include "power_utils.h"
#include "sleep_utils.h"
#include "gps_utils.h"
#include "gps_math.h"
#include "display.h"
#ifdef GPS_BAUDRATE
    #define GPS_BAUD    GPS_BAUDRATE
#else
    #define GPS_BAUD    9600
#endif


extern Configuration        Config;
extern HardwareSerial       gpsSerial;
extern NMEAGPS              nmeaGPS;
extern gps_fix              gpsFix;
extern Beacon               *currentBeacon;
extern bool                 sendUpdate;
extern bool		            sendStandingUpdate;

extern uint32_t             lastTxTime;
extern uint32_t             txInterval;
extern double               lastTxLat;
extern double               lastTxLng;
extern double               lastTxDistance;
extern uint32_t             lastTx;
extern bool                 disableGPS;
extern bool                 gpsShouldSleep;
extern SmartBeaconValues    currentSmartBeaconValues;

double      currentHeading  = 0;
double      previousHeading = 0;
float       bearing         = 0;

static const char *TAG = "GPS";

bool        gpsIsActive     = true;


namespace GPS_Utils {

    void setup() {
        if (disableGPS) {
            ESP_LOGW(TAG, "GPS disabled");
            return;
        }
        #ifdef LIGHTTRACKER_PLUS_1_0
            pinMode(GPS_VCC, OUTPUT);
            digitalWrite(GPS_VCC, LOW);
            delay(200);
        #endif
        #if defined(F4GOH_1W_LoRa_Tracker) || defined(F4GOH_1W_LoRa_Tracker_LLCC68)
            pinMode(GPS_VCC, OUTPUT);
            digitalWrite(GPS_VCC, HIGH);
            delay(200);
        #endif
        
        gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_TX, GPS_RX);
        #ifdef TTGO_T_DECK_PLUS
            // L76K: restrict output to RMC, GGA, and GSA only (reduces UART load, speeds up parsing)
            // Note: The L76K module on the T-Deck may ignore this command depending on firmware/wiring,
            // but it is kept as best practice.
            delay(100);
            gpsSerial.print("$PMTK314,0,1,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29\r\n");
        #endif
    }

    static bool newFixAvailable = false;

    void calculateDistanceCourse(const String& callsign, double checkpointLatitude, double checkPointLongitude) {
        double distanceKm = calcDist(gpsFix.latitude(), gpsFix.longitude(), checkpointLatitude, checkPointLongitude) / 1000.0;
        double courseTo   = calcCourse(gpsFix.latitude(), gpsFix.longitude(), checkpointLatitude, checkPointLongitude);
        STATION_Utils::deleteListenedStationsByTime();
        STATION_Utils::orderListenedStationsByDistance(callsign, distanceKm, courseTo);
    }

    void getData() {
        if (disableGPS) return;
        newFixAvailable = false;
        while (nmeaGPS.available(gpsSerial)) {
            gpsFix = nmeaGPS.read();
            newFixAvailable = true;
        }
    }

    bool hasNewFix() { return newFixAvailable; }

    void setDateFromData() {
        if (gpsFix.valid.time && gpsFix.valid.date) {
            int year = 2000 + gpsFix.dateTime.year;
            setTime(gpsFix.dateTime.hours, gpsFix.dateTime.minutes, gpsFix.dateTime.seconds,
                    gpsFix.dateTime.date, gpsFix.dateTime.month, year);
            // Sync system clock so FAT32 timestamps use GPS time
            struct tm t = {};
            t.tm_year = year - 1900;
            t.tm_mon  = gpsFix.dateTime.month - 1;
            t.tm_mday = gpsFix.dateTime.date;
            t.tm_hour = gpsFix.dateTime.hours;
            t.tm_min  = gpsFix.dateTime.minutes;
            t.tm_sec  = gpsFix.dateTime.seconds;
            struct timeval tv = { .tv_sec = mktime(&t), .tv_usec = 0 };
            settimeofday(&tv, nullptr);
        }
    }

    void calculateDistanceTraveled() {
        // Guard against being called twice per GPS cycle (e.g. from two call sites in the main loop).
        static uint32_t lastCalcMs = 0;
        uint32_t now = millis();
        if (now - lastCalcMs < 500) return;   // same GPS epoch → skip duplicate call
        lastCalcMs = now;

        currentHeading  = gpsFix.valid.heading ? gpsFix.heading() : 0.0;

        // Anti-jitter filter: Calculate raw distance jump
        double rawDistance = calcDist(gpsFix.latitude(), gpsFix.longitude(), lastTxLat, lastTxLng);

        // If speed is very low (< 5 km/h) but distance jump is large (> 50m), it's likely GPS multipath jitter.
        // We only accept large distances at low speeds if enough time has passed (standing update).
        float speedKmph = gpsFix.valid.speed ? gpsFix.speed_kph() : 0.0f;
        if (speedKmph < 5.0 && rawDistance > 50.0 && lastTx < Config.standingUpdateTime * 60 * 1000) {
            // Rate-limit this log to once every 30 s to avoid serial flood
            static uint32_t lastJitterLog = 0;
            if (now - lastJitterLog >= 30000) {
                lastJitterLog = now;
                ESP_LOGD(TAG, "Suppressed GPS jitter: speed %.1f km/h, raw jump %.1f m", speedKmph, rawDistance);
            }
            lastTxDistance = 0.0; // Ignore this jump for beaconing logic
        } else {
            lastTxDistance = rawDistance;
        }

        if (lastTx >= txInterval) {
            if (lastTxDistance > currentSmartBeaconValues.minTxDist) {
                sendUpdate = true;
                sendStandingUpdate = false;
            } else {
                if (currentBeacon->gpsEcoMode) {
                    ESP_LOGD(TAG, "minTxDistance not achieved: %f", lastTxDistance);
                    gpsShouldSleep = true;
                }
            }
        }
    }

    void calculateHeadingDelta(int speed) {
        uint8_t TurnMinAngle;
        double headingDelta = abs(previousHeading - currentHeading);
        if (lastTx > currentSmartBeaconValues.minDeltaBeacon * 1000) {
            if (speed == 0) {
                TurnMinAngle = currentSmartBeaconValues.turnMinDeg + (currentSmartBeaconValues.turnSlope/(speed + 1));
            } else {
                TurnMinAngle = currentSmartBeaconValues.turnMinDeg + (currentSmartBeaconValues.turnSlope/speed);
            }
            if (headingDelta > TurnMinAngle && lastTxDistance > currentSmartBeaconValues.minTxDist) {
                sendUpdate = true;
                sendStandingUpdate = false;
            }
        }
    }

    void checkStartUpFrames() {
        if (disableGPS) return;
        if ((millis() > 10000 && nmeaGPS.statistics.chars < 10)) {
            ESP_LOGE(TAG, "No GPS frames detected! Try to reset the GPS Chip with this "
                        "firmware: https://github.com/richonguzman/TTGO_T_BEAM_GPS_RESET");
            displayShow("ERROR", "No GPS frames!", "Reset the GPS Chip", 2000);
        }
    }

    String getHumanBearing(const String& left, const String& center, const String& right) {
        String bearing = ">.";
        bearing += left;
        for (int i = 0; i < 9; i++) {
            bearing += ".";
        }
        bearing += "(";
        bearing += center;
        bearing += ").....";
        if (right.length() == 1 && center.length() != 2) bearing += ".";
        bearing += right;
        bearing += ".<";
        return bearing;
    }

    String getCardinalDirection(float course) {
        if (gpsFix.valid.speed && gpsFix.speed_kph() > 0.5) bearing = course;

        if (bearing >= 354.375 || bearing < 5.625)    return ">.NW.....(N).....NE.<"; // N
        if (bearing >= 5.675 && bearing < 16.875)     return ">.......N.|.....NE..<";
        if (bearing >= 16.875 && bearing < 28.125)    return ">.....N...|...NE....<"; // NEN
        if (bearing >= 28.125 && bearing < 39.375)    return ">...N.....|.NE......<";
        if (bearing >= 39.375 && bearing < 50.625)    return ">.N......(NE).....E.<"; // NE
        if (bearing >= 50.625 && bearing < 61.875)    return ">.......NE|.....E...<"; 
        if (bearing >= 61.875 && bearing < 73.125)    return ">.....NE..|...E.....<"; // ENE
        if (bearing >= 73.125 && bearing < 84.375)    return ">...NE....|.E.......<"; 
        if (bearing >= 84.375 && bearing < 95.625)    return ">.NE.....(E).....SE.<"; // E
        if (bearing >= 95.625 && bearing < 106.875)   return ">.......E.|.....SE..<";
        if (bearing >= 106.875 && bearing < 118.125)  return ">.....E...|...SE....<"; // ESE
        if (bearing >= 118.125 && bearing < 129.375)  return ">...E.....|.SE......<";
        if (bearing >= 129.375 && bearing < 140.625)  return ">.E......(SE).....S.<"; // SE
        if (bearing >= 140.625 && bearing < 151.875)  return ">.......SE|.....S...<";
        if (bearing >= 151.875 && bearing < 163.125)  return ">.....SE..|...S.....<"; // SES
        if (bearing >= 163.125 && bearing < 174.375)  return ">...SE....|.S.......<";
        if (bearing >= 174.375 && bearing < 185.625)  return ">.SE.....(S).....SW.<"; // S
        if (bearing >= 185.625 && bearing < 196.875)  return ">.......S.|.....SW..<";
        if (bearing >= 196.875 && bearing < 208.125)  return ">.....S...|...SW....<"; // SWS
        if (bearing >= 208.125 && bearing < 219.375)  return ">...S.....|.SW......<";
        if (bearing >= 219.375 && bearing < 230.625)  return ">.S......(SW).....W.<"; // SW
        if (bearing >= 230.625 && bearing < 241.875)  return ">.......SW|.....W...<";
        if (bearing >= 241.875 && bearing < 253.125)  return ">.....SW..|...W.....<"; // WSW
        if (bearing >= 253.125 && bearing < 264.375)  return ">...SW....|.W.......<";
        if (bearing >= 264.375 && bearing < 275.625)  return ">.SW.....(W).....NW.<"; // W
        if (bearing >= 275.625 && bearing < 286.875)  return ">.......W.|.....NW..<";
        if (bearing >= 286.875 && bearing < 298.125)  return ">.....W...|...NW....<"; // WNW
        if (bearing >= 298.125 && bearing < 309.375)  return ">...W.....|.NW......<";
        if (bearing >= 309.375 && bearing < 320.625)  return ">.W......(NW).....N.<"; // NW
        if (bearing >= 320.625 && bearing < 331.875)  return ">.......NW|.....N...<";
        if (bearing >= 331.875 && bearing < 343.125)  return ">.....NW..|...N.....<"; // NWN
        if (bearing >= 343.125 && bearing < 354.375)  return ">...NW....|.N.......<";
        return "";
    }

}