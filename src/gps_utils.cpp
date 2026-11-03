/* Copyright (C) 2026 Ricardo Guzman - CA2RXU
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

#include <APRSPacketLib.h>
#include <TinyGPS++.h>
#include <SPIFFS.h>
#include "TimeLib.h"
#include "smartbeacon_utils.h"
#include "configuration.h"
#include "station_utils.h"
#include "board_pinout.h"
#include "power_utils.h"
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
#ifdef ARDUINO_ARCH_NRF52
    // Adafruit BSP's Serial1 is our GPS port — see LoRa_APRS_Tracker.cpp.
    #define gpsSerial Serial1
#else
    extern HardwareSerial   gpsSerial;
#endif
extern TinyGPSPlus          gps;
extern Beacon               *currentBeacon;
extern logging::Logger      logger;
extern bool                 sendUpdate;

extern uint32_t             lastTxTime;
extern uint32_t             txInterval;
extern uint32_t             lastTx;
extern bool                 disableGPS;
extern SmartBeaconValues    currentSmartBeaconValues;

double      currentHeading  = 0;
double      previousHeading = 0;
float       bearing         = 0;

bool        gpsIsActive     = false;  // set to true in setup() only when GPS hardware is started


namespace GPS_Utils {

    struct PositionData {
        double latitude;
        double longitude;
        float elevation;
        uint8_t satelliteCount;
        uint32_t timestamp;
        bool isValid;
    };

    PositionData lastValidPosition = {0, 0, 0, 0, 0, false};

    bool getPositionData(PositionData &posData) {
        posData.timestamp = millis();
        posData.isValid = false;

        switch (Config.gpsSource) {
            case GPS_INTERNAL:
                if (disableGPS) {
                    // No GPS hardware on this board.  If fixed coordinates are
                    // configured, use them so iGate/Digi beacons still work even
                    // when the user forgot to set gpsSource = GPS_FIXED.
                    if (Config.fixedPosition.latitude != 0 || Config.fixedPosition.longitude != 0) {
                        posData.latitude  = Config.fixedPosition.latitude;
                        posData.longitude = Config.fixedPosition.longitude;
                        posData.elevation = Config.fixedPosition.elevation;
                        posData.isValid   = true;
                        lastValidPosition = posData;
                        return true;
                    }
                    return false;
                }
                if (gps.location.isValid()) {
                    posData.latitude = gps.location.lat();
                    posData.longitude = gps.location.lng();
                    posData.elevation = gps.altitude.meters();
                    posData.satelliteCount = gps.satellites.value();
                    posData.isValid = true;
                    lastValidPosition = posData;
                    return true;
                }
                break;

            case GPS_FIXED:
                posData.latitude = Config.fixedPosition.latitude;
                posData.longitude = Config.fixedPosition.longitude;
                posData.elevation = Config.fixedPosition.elevation;
                posData.satelliteCount = 0;
                posData.isValid = (posData.latitude != 0 || posData.longitude != 0);
                if (posData.isValid) {
                    lastValidPosition = posData;
                }
                return posData.isValid;

            case GPS_NONE:
                return false;  // no position source configured

            default:
                break;
        }

        // Fallback to last valid position if current source has no data
        if (lastValidPosition.isValid) {
            posData = lastValidPosition;
            return true;
        }

        return false;
    }

    bool getCurrentLocation(double &lat, double &lng, float &elev) {
        PositionData posData;
        if (getPositionData(posData)) {
            lat = posData.latitude;
            lng = posData.longitude;
            elev = posData.elevation;
            return true;
        }
        return false;
    }

    // ---- GPS BAUD autodetect (adopted from upstream V4.0) ----
    // ESP32-only: probeBaudRate() calls gpsSerial.begin(baud, SERIAL_8N1, GPS_TX, GPS_RX)
    // which is a HardwareSerial-specific signature; Adafruit nRF52 BSP's Serial1 only
    // supports .begin(baud). On nRF52 we fall back to a fixed GPS_BAUD in setup().
    #ifndef ARDUINO_ARCH_NRF52
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
            if (i == 1) bootStatus("Searching GPS baud rate...");
            bool found = probeBaudRate(candidates[i]);
            if (!found && i == 0) found = probeBaudRate(candidates[i]);    // retry first one: GPS may be slow to start
            if (found) {
                if (candidates[i] != savedBaud) saveBaudRate(candidates[i]);
                return candidates[i];
            }
        }
        return 0;
    }
    #endif  // !ARDUINO_ARCH_NRF52

    void setup() {
        // No hardware needed for fixed position or when GPS is explicitly disabled.
        if (Config.gpsSource == GPS_FIXED) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "GPS", "Fixed position — GPS hardware not started");
            gpsIsActive = false;
            return;
        }
        if (Config.gpsSource == GPS_NONE) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "GPS", "GPS_NONE — no position source, no beaconing");
            gpsIsActive = false;
            return;
        }

        // Board has no GPS connector or pin assignment.
        #ifdef HAS_NO_GPS
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "GPS", "No GPS hardware on this board");
            gpsIsActive = false;
            return;
        #endif

        // Runtime override (e.g. WiFi AP active on T-Beam)
        if (disableGPS) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "GPS", "GPS disabled (runtime override)");
            gpsIsActive = false;
            return;
        }

        #ifdef LIGHTTRACKER_PLUS_1_0
            pinMode(GPS_VCC, OUTPUT);
            digitalWrite(GPS_VCC, LOW);
            delay(200);
        #endif
        #if defined(HELTEC_T114)
            // T114: VEXT_ENABLE (P0.21) gates the L76K GPS power rail; must be
            // HIGH before any UART activity or the GPS reports nothing.
            pinMode(GPS_VCC, OUTPUT);
            digitalWrite(GPS_VCC, HIGH);
            delay(200);
        #endif

        #ifdef ARDUINO_ARCH_NRF52
            // Adafruit nRF52 Serial1.begin() only takes baud; no autodetect.
            gpsSerial.begin(GPS_BAUD);
        #else
            uint32_t detectedBaud = detectBaudRate();
            if (detectedBaud != 0) {
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "GPS", "Baud rate detected: %u", detectedBaud);
            } else {
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "GPS", "No NMEA data found, using default baud rate: %u", GPS_BAUD);
                gpsSerial.end();
                delay(50);
                gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_TX, GPS_RX);
            }
        #endif

        gpsIsActive = true;
    }

    void calculateDistanceCourse(const String& callsign, double checkpointLatitude, double checkPointLongitude) {
        (void)TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(), checkpointLatitude, checkPointLongitude);
        (void)TinyGPSPlus::courseTo(gps.location.lat(), gps.location.lng(), checkpointLatitude, checkPointLongitude);
        STATION_Utils::updateLastHeard(callsign);  // track heard station for display
    }

    void getData() {
        if (disableGPS) return;
        while (gpsSerial.available() > 0) gps.encode(gpsSerial.read());
    }

    void setDateFromData() {
        if (!gps.time.isValid()) return;
        if (gps.date.isValid() && gps.date.year() >= 2026 && gps.date.year() < 2080) {
            setTime(gps.time.hour(), gps.time.minute(), gps.time.second(), gps.date.day(), gps.date.month(), gps.date.year());
        } else {
            setTime(gps.time.hour(), gps.time.minute(), gps.time.second(), day(), month(), year());   // time only, date not received yet
        }
    }

    void calculateDistanceTraveled() {
        // EMA (α=0.3) smooths GPS course jitter (~±10°) without lagging real turns.
        // Time constant ~2.3 s at 1 Hz; a 30° real turn reaches 90% in ~4 updates.
        static float smoothedHeading = -1.0f;
        if (gps.course.isValid() && gps.course.age() <= 2000) {
            float raw = (float)gps.course.deg();
            if (smoothedHeading < 0.0f) {
                smoothedHeading = raw;
            } else {
                float delta = raw - smoothedHeading;
                if (delta >  180.0f) delta -= 360.0f;
                if (delta < -180.0f) delta += 360.0f;
                smoothedHeading += 0.3f * delta;
                if (smoothedHeading <   0.0f) smoothedHeading += 360.0f;
                if (smoothedHeading >= 360.0f) smoothedHeading -= 360.0f;
            }
            currentHeading = smoothedHeading;
        }
        if (lastTx >= txInterval) {
            sendUpdate = true;
        }
    }

    void calculateHeadingDelta(int speed) {
        double headingDelta = fabs(previousHeading - currentHeading);
        if (headingDelta > 180.0) headingDelta = 360.0 - headingDelta;    // shortest angle: 355 -> 5 is 10 deg, not 350
        // Don't allow a heading-change beacon sooner than the current beacon interval.
        // When parked (speed==0) txInterval equals nonSmartBeaconRate, so GPS heading
        // noise cannot trigger a beacon during the stationary wait.
        if (speed > 1 && lastTx >= txInterval) {
            int TurnMinAngle = currentSmartBeaconValues.turnMinDeg + (currentSmartBeaconValues.turnSlope / speed);
            if (headingDelta > TurnMinAngle) {
                sendUpdate = true;
            }
        }
    }

    void checkStartUpFrames() {
        if (disableGPS) return;
        if ((millis() > 10000 && gps.charsProcessed() < 10)) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "GPS",
                        "No GPS frames detected! Try to reset the GPS Chip with this "
                        "firmware: https://github.com/richonguzman/TTGO_T_BEAM_GPS_RESET");
            bootStatus("ERROR: No GPS frames!");
        }
    }

    // Returns a parseable single-line status string consumed by serial_config.html.
    // Format: "gps.lat=47.123456 gps.lon=-122.345678 gps.alt=150.0 gps.sats=8 gps.valid=1"
    String getStatusString() {
        PositionData pd;
        bool ok = getPositionData(pd);
        String s = "gps.lat=";
        s += ok ? String(pd.latitude,  6) : "0.000000";
        s += " gps.lon=";
        s += ok ? String(pd.longitude, 6) : "0.000000";
        s += " gps.alt=";
        s += ok ? String(pd.elevation, 1) : "0.0";
        s += " gps.sats=";
        s += ok ? String(pd.satelliteCount) : "0";
        s += " gps.valid=";
        s += ok ? "1" : "0";
        return s;
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
        if (bearing >= 5.625 && bearing < 16.875)     return ">.......N.|.....NE..<";
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