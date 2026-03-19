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

#ifndef STATION_UTILS_H_
#define STATION_UTILS_H_

#include <Arduino.h>

// Structure for stations to display on map
#define MAP_STATIONS_MAX 15
#define TRACE_MAX_POINTS 100

struct TracePoint {
    float lat;
    float lon;
    uint32_t time;  // millis() timestamp for TTL filtering
};

struct MapStation {
    String     callsign;
    float      latitude;
    float      longitude;
    String     symbol;
    String     overlay;
    int        rssi;
    uint32_t   lastTime;
    bool       valid;
    TracePoint trace[TRACE_MAX_POINTS];
    uint8_t    traceCount;
    uint8_t    traceHead;
};

extern MapStation mapStations[MAP_STATIONS_MAX];
extern int mapStationsCount;

namespace STATION_Utils {

    void    nearStationInit();
    String  getNearStation(uint8_t position);

    void    deleteListenedStationsByTime();
    void    checkListenedStationsByTimeAndDelete();
    void    orderListenedStationsByDistance(const String& callsign, float distance, float course);
    
    void    checkStandingUpdateTime();
    void    sendBeacon();
    void    saveIndex(uint8_t type, uint8_t index);
    void    loadIndex(uint8_t type);

    // Map stations management
    void    mapStationsInit();
    void    addMapStation(const String& callsign, float lat, float lon, const String& symbol, const String& overlay, int rssi);
    void    cleanOldMapStations();
    MapStation* getMapStation(int index);
    MapStation* findMapStation(const String& callsign);

    // Douglas-Peucker trace simplification (shared by stations and own trace)
    float perpendicularDistance(float px, float py, float x1, float y1, float x2, float y2);
    void  douglasPeuckerSimplify(TracePoint* trace, int start, int end, bool* keep, float epsilon);

}

#endif
