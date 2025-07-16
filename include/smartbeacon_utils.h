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

#ifndef SMARTBEACON_UTILS_H_
#define SMARTBEACON_UTILS_H_

#include <Arduino.h>

struct SmartBeaconValues {
    int     slowRate;
    int     slowSpeed;
    int     fastRate;
    int     fastSpeed;
    int     minTxDist;
    int     minDeltaBeacon;
    int     turnMinDeg;
    int     turnSlope;
};


namespace SMARTBEACON_Utils {

    void checkSettings(byte index);
    void checkInterval(int speed);
    void checkFixedBeaconTime();
    void checkState();
    
}

#endif