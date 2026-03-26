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

#ifndef GPS_UTILS_H_
#define GPS_UTILS_H_

#include <Arduino.h>

namespace GPS_Utils {

    void    setup();
    void    calculateDistanceCourse(const String& callsign, double checkpointLatitude, double checkPointLongitude);
    void    getData();
    bool    hasNewFix();
    void    setDateFromData();
    void    calculateDistanceTraveled();
    void    calculateHeadingDelta(int speed);
    void    checkStartUpFrames();
    String  getCardinalDirection(float course);

}

// NeoGPS convenience helpers (avoid repeating conversion formulas)
#include <GPSfix.h>
extern gps_fix  gpsFix;

inline float gpsSpeedKnots() { return gpsFix.valid.speed ? gpsFix.speed_kph() / 1.852f : 0.0f; }
inline int32_t gpsAltFeet()  { return gpsFix.valid.altitude ? (int32_t)(gpsFix.alt.whole * 3.28084f) : 0; }
inline float gpsHdop()       { return gpsFix.valid.hdop ? (float)gpsFix.hdop / 1000.0f : 99.0f; }
inline float gpsVdop()       { return gpsFix.valid.vdop ? (float)gpsFix.vdop / 1000.0f : 99.0f; }
inline float gpsPdop()       { return gpsFix.valid.pdop ? (float)gpsFix.pdop / 1000.0f : 99.0f; }

#endif