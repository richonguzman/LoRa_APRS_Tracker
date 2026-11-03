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

#include "smartbeacon_utils.h"
#include "configuration.h"
#include "logger.h"

extern logging::Logger logger;

extern Configuration    Config;
extern Beacon           *currentBeacon;
extern uint32_t         txInterval;
extern uint32_t         lastTxTime;
extern bool             sendUpdate;

// SmartBeacon runtime state — owned here, read via extern in station_utils.cpp.
bool                smartBeaconActive       = false;
SmartBeaconValues   currentSmartBeaconValues;
byte                smartBeaconSettingsIndex    = 10;
bool                wxRequestStatus             = false;
uint32_t            wxRequestTime               = 0;


// {slowRate(s), slowSpeed(km/h), fastRate(s), fastSpeed(km/h), turnMinDeg, turnSlope}
SmartBeaconValues   smartBeaconSettings[SMARTBEACON_PROFILE_COUNT] = {
    { 90,  5, 45,  18, 12,  60},    // Runner  — walk→sprint (5–18 km/h), ~200 m/beacon at pace
    {120,  5, 45,  50, 12,  60},    // Bike    — casual→e-bike (5–50 km/h), ~600 m/beacon
    { 90, 10, 36,  97, 10,  80},    // Car     — city→highway, ~0.6 mi/beacon at 60 mph
    { 60, 30, 10,  97,  5, 150},    // Jetboat — marina→WOT; clips to 10 s above 60 mph
    {120,  5, 60,  40, 12,  60}     // Custom  — filled from Config.customSmartBeacon at load
};

static const char* SMARTBEACON_PROFILE_LABELS[SMARTBEACON_PROFILE_COUNT] = {
    "Runner", "Bike", "Car", "Jetboat", "Custom"
};


namespace SMARTBEACON_Utils {

    void checkSettings(byte index) {
        if (index >= SMARTBEACON_PROFILE_COUNT) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "SmartBeacon", "smartBeaconSetting %u out of range, clamping to 0", index);
            index = 0;
        }
        if (smartBeaconSettingsIndex != index) {
            currentSmartBeaconValues = smartBeaconSettings[index];
            smartBeaconSettingsIndex = index;
        }
    }

    void setCustomValues(const SmartBeaconValues& v) {
        smartBeaconSettings[SMARTBEACON_CUSTOM_INDEX] = v;
        if (smartBeaconSettingsIndex == SMARTBEACON_CUSTOM_INDEX) {
            smartBeaconSettingsIndex = 255;             // force re-copy on next checkSettings
            checkSettings(SMARTBEACON_CUSTOM_INDEX);
        }
    }

    const char* profileLabel(byte index) {
        if (index >= SMARTBEACON_PROFILE_COUNT) return "?";
        return SMARTBEACON_PROFILE_LABELS[index];
    }

    void checkInterval(int speed) {
        if (smartBeaconActive) {
            if (speed <= 1) {
                // GPS Doppler noise on a stationary device typically rounds to 0–1 km/h.
                // Treat both as parked and use the configured non-smart rate.
                txInterval = (uint32_t)Config.nonSmartBeaconRate * 1000UL;
            } else if (speed < currentSmartBeaconValues.slowSpeed) {
                txInterval = currentSmartBeaconValues.slowRate * 1000;
            } else if (speed > currentSmartBeaconValues.fastSpeed) {
                txInterval = currentSmartBeaconValues.fastRate * 1000;
            } else {
                int range = currentSmartBeaconValues.fastSpeed - currentSmartBeaconValues.slowSpeed;
                if (range <= 0) {
                    txInterval = (uint32_t)currentSmartBeaconValues.fastRate * 1000;
                } else {
                    txInterval = (uint32_t)(currentSmartBeaconValues.fastRate
                        + (currentSmartBeaconValues.slowRate - currentSmartBeaconValues.fastRate)
                        * (currentSmartBeaconValues.fastSpeed - speed)
                        / range) * 1000;
                }
            }
        }
    }

    void checkFixedBeaconTime() {
        if (!smartBeaconActive) {
            uint32_t lastTxSmartBeacon = millis() - lastTxTime;
            if (lastTxSmartBeacon >= (uint32_t)Config.nonSmartBeaconRate * 1000UL) sendUpdate = true;
        }
    }

    void checkState() {
        if (wxRequestStatus && (millis() - wxRequestTime) > 20000) wxRequestStatus = false;
        smartBeaconActive = !wxRequestStatus ? currentBeacon->smartBeaconActive : false;
    }

}