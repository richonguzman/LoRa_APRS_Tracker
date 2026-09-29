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

#include <TinyGPS++.h>
#include <SPIFFS.h>
#include "TimeLib.h"
#include <APRSPacketLib.h>
#include "smartbeacon_utils.h"
#include "configuration.h"
#include "station_utils.h"
#include "board_pinout.h"
#include "power_utils.h"
#include "sleep_utils.h"
#include "gps_utils.h"
#include "display.h"
#include "logger.h"


#ifdef GPS_BAUDRATE
    #define GPS_BAUD    GPS_BAUDRATE
#else
    #define GPS_BAUD    9600
#endif

#define GPS_BAUD_FILE           "/gps_baud.dat"
#define GPS_BAUD_PROBE_TIME_MS  1500
const uint32_t gpsBaudRates[] = {9600, 115200, 38400, 4800, 57600};     // most common first


extern Configuration        Config;
extern HardwareSerial       gpsSerial;
extern TinyGPSPlus          gps;
extern Beacon               *currentBeacon;
extern logging::Logger      logger;
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

bool        gpsIsActive     = true;


namespace GPS_Utils {

    bool isKnownBaudRate(uint32_t baud) {
        if (baud == GPS_BAUD) return true;
        for (uint32_t knownBaud : gpsBaudRates) {
            if (knownBaud == baud) return true;
        }
        return false;
    }

    uint32_t readSavedBaudRate() {
        File file = SPIFFS.open(GPS_BAUD_FILE, "r");
        if (!file) return 0;
        uint32_t baud = file.readString().toInt();
        file.close();
        return isKnownBaudRate(baud) ? baud : 0;
    }

    void saveBaudRate(uint32_t baud) {
        File file = SPIFFS.open(GPS_BAUD_FILE, "w");
        if (!file) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "GPS", "Could not save baud rate");
            return;
        }
        file.print(baud);
        file.close();
    }

    bool probeBaudRate(uint32_t baud) {
        gpsSerial.end();
        delay(50);
        gpsSerial.begin(baud, SERIAL_8N1, GPS_TX, GPS_RX);
        uint32_t validSentences = gps.passedChecksum();
        uint32_t probeStart     = millis();
        while (millis() - probeStart < GPS_BAUD_PROBE_TIME_MS) {
            while (gpsSerial.available() > 0) gps.encode(gpsSerial.read());
            if (gps.passedChecksum() > validSentences) return true;   // NMEA with valid checksum
            delay(10);
        }
        return false;
    }

    uint32_t detectBaudRate() {
        uint32_t savedBaud = readSavedBaudRate();

        uint32_t candidates[2 + sizeof(gpsBaudRates) / sizeof(gpsBaudRates[0])];
        size_t   count = 0;
        auto addCandidate = [&](uint32_t baud) {
            if (baud == 0) return;
            for (size_t i = 0; i < count; i++) {
                if (candidates[i] == baud) return;
            }
            candidates[count++] = baud;
        };
        addCandidate(savedBaud);                                // saved first, then board default, then common ones
        addCandidate(GPS_BAUD);
        for (uint32_t baud : gpsBaudRates) addCandidate(baud);

        for (size_t i = 0; i < count; i++) {
            if (i == 1) displayShow("GPS", "Searching", "baud rate...", 0);
            bool found = probeBaudRate(candidates[i]);
            if (!found && i == 0) found = probeBaudRate(candidates[i]);    // retry first one: GPS may be slow to start
            if (found) {
                if (candidates[i] != savedBaud) saveBaudRate(candidates[i]);
                return candidates[i];
            }
        }
        return 0;
    }

    void setup() {
        if (disableGPS) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "Main", "GPS disabled");
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

        uint32_t detectedBaud = detectBaudRate();
        if (detectedBaud != 0) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "GPS", "Baud rate detected: %u", detectedBaud);
        } else {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "GPS", "No NMEA data found, using default baud rate: %u", GPS_BAUD);
            gpsSerial.end();
            delay(50);
            gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_TX, GPS_RX);
        }
    }

    void calculateDistanceCourse(const String& callsign, double checkpointLatitude, double checkPointLongitude) {
        double distanceKm = TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(), checkpointLatitude, checkPointLongitude) / 1000.0;
        double courseTo   = TinyGPSPlus::courseTo(gps.location.lat(), gps.location.lng(), checkpointLatitude, checkPointLongitude);
        STATION_Utils::deleteListenedStationsByTime();
        STATION_Utils::orderListenedStationsByDistance(callsign, distanceKm, courseTo);
    }

    void getData() {
        if (disableGPS) return;
        while (gpsSerial.available() > 0) gps.encode(gpsSerial.read());
    }

    void setDateFromData() {
        if (gps.time.isValid()) setTime(gps.time.hour(), gps.time.minute(), gps.time.second(), gps.date.day(), gps.date.month(), gps.date.year());
    }

    void calculateDistanceTraveled() {
        currentHeading  = gps.course.deg();
        lastTxDistance  = TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(), lastTxLat, lastTxLng);
        if (lastTx >= txInterval) {
            if (lastTxDistance > currentSmartBeaconValues.minTxDist) {
                sendUpdate = true;
                sendStandingUpdate = false;
            } else {
                if (currentBeacon->gpsEcoMode) {
                    //
                    Serial.print("minTxDistance not achieved : ");
                    Serial.println(lastTxDistance);
                    //
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
        if ((millis() > 10000 && gps.charsProcessed() < 10)) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "GPS",
                        "No GPS frames detected! Try to reset the GPS Chip with this "
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
        if (gps.speed.kmph() > 0.5) bearing = course;

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