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

}

#endif