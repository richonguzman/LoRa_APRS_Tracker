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

#ifndef GPS_UTILS_H_
#define GPS_UTILS_H_

#include <Arduino.h>


namespace GPS_Utils {

    void    setup();
    void    calculateDistanceCourse(const String& callsign, double checkpointLatitude, double checkPointLongitude);
    void    getData();
    void    setDateFromData();
    void    calculateDistanceTraveled();
    void    calculateHeadingDelta(int speed);
    void    checkStartUpFrames();
    String  getCardinalDirection(float course);

    // Returns the current lat/lon/elev from whichever GPS source is active.
    bool    getCurrentLocation(double &lat, double &lng, float &elev);

    // Returns a parseable one-liner for serial_config.html 'gps read' command.
    // "gps.lat=47.123456 gps.lon=-122.345678 gps.alt=150.0 gps.sats=8 gps.valid=1"
    String  getStatusString();

}

#endif