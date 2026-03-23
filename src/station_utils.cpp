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

#ifdef UNIT_TEST
#include "mock_esp_log.h"
#else
#include <esp_log.h>
#endif
static const char *TAG = "Station";

#include <APRSPacketLib.h>
#include <NMEAGPS.h>
#include "gps_utils.h"
#ifndef UNIT_TEST
#include <SPIFFS.h>
#endif
#include "telemetry_utils.h"
#include "station_utils.h"
#include "battery_utils.h"
#include "configuration.h"
#include "board_pinout.h"
#include "power_utils.h"
#include "sleep_utils.h"
#include "lora_utils.h"
#include "aprs_is_utils.h"
#include "ble_utils.h"
#include "wx_utils.h"
#include "display.h"
#ifdef USE_LVGL_UI
#include "lvgl_ui.h"
#include "ui_map_manager.h"
#include "gpx_writer.h"
#endif

extern Configuration        Config;
extern Beacon               *currentBeacon;
extern gps_fix              gpsFix;
extern uint8_t              myBeaconsIndex;
extern uint8_t              loraIndex;
extern uint8_t              screenBrightness;
extern bool                 displayEcoMode;

extern uint32_t             lastTx;
extern uint32_t             lastTxTime;

extern bool                 sendUpdate;

extern double               currentHeading;
extern double               previousHeading;

extern double               lastTxLat;
extern double               lastTxLng;
extern double               lastTxDistance;

extern bool                 miceActive;
extern bool                 smartBeaconActive;
extern bool                 winlinkCommentState;

extern int                  wxModuleType;
extern bool                 wxModuleFound;
extern bool                 gpsIsActive;
extern bool                 gpsShouldSleep;


bool	    sendStandingUpdate      = false;
uint8_t     updateCounter           = 100;


bool        sendStartTelemetry      = true;

uint32_t    lastDeleteListenedStation;
const int   nearbyStationsSize  = 4;


struct nearStation {
    String      callsign;
    float       distance;
    int         course;
    uint32_t    lastTime;
};

nearStation nearbyStations[nearbyStationsSize];

// Map stations for displaying on map
MapStation mapStations[MAP_STATIONS_MAX];
int mapStationsCount = 0;

// Expose mapStationsCount to UIMapManager namespace
#ifdef USE_LVGL_UI
namespace UIMapManager {
    int& mapStationsCount = ::mapStationsCount;
}
#endif

namespace STATION_Utils {

    void nearStationInit() {
        for (int i = 0; i < nearbyStationsSize; i++) {
            nearbyStations[i].callsign    = "";
            nearbyStations[i].distance    = 0.0;
            nearbyStations[i].course      = 0;
            nearbyStations[i].lastTime    = 0;
        }
    }

    // Initialize map stations array
    void mapStationsInit() {
        for (int i = 0; i < MAP_STATIONS_MAX; i++) {
            mapStations[i].callsign  = "";
            mapStations[i].latitude  = 0.0;
            mapStations[i].longitude = 0.0;
            mapStations[i].symbol    = "";
            mapStations[i].overlay   = "";
            mapStations[i].rssi       = 0;
            mapStations[i].lastTime   = 0;
            mapStations[i].valid      = false;
            mapStations[i].traceCount = 0;
            mapStations[i].traceHead  = 0;
        }
        mapStationsCount = 0;
    }

    // Douglas-Peucker algorithm: perpendicular distance from point to line segment
    float perpendicularDistance(float px, float py, float x1, float y1, float x2, float y2) {
        float dx = x2 - x1;
        float dy = y2 - y1;

        // Line segment length squared
        float lenSq = dx * dx + dy * dy;
        if (lenSq == 0.0f) {
            // Degenerate case: start == end
            float ddx = px - x1;
            float ddy = py - y1;
            return sqrtf(ddx * ddx + ddy * ddy);
        }

        // Project point onto line, clamped to segment
        float t = ((px - x1) * dx + (py - y1) * dy) / lenSq;
        t = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);

        float projX = x1 + t * dx;
        float projY = y1 + t * dy;

        float ddx = px - projX;
        float ddy = py - projY;
        return sqrtf(ddx * ddx + ddy * ddy);
    }

    // Douglas-Peucker simplification: reduce trace points while keeping shape
    void douglasPeuckerSimplify(TracePoint* trace, int start, int end, bool* keep, float epsilon) {
        if (end <= start + 1) return;

        // Find point with max distance from line segment [start, end]
        float maxDist = 0.0f;
        int maxIndex = start;

        for (int i = start + 1; i < end; i++) {
            float dist = perpendicularDistance(
                trace[i].lat, trace[i].lon,
                trace[start].lat, trace[start].lon,
                trace[end].lat, trace[end].lon
            );
            if (dist > maxDist) {
                maxDist = dist;
                maxIndex = i;
            }
        }

        // If max distance > epsilon, keep that point and recurse
        if (maxDist > epsilon) {
            keep[maxIndex] = true;
            douglasPeuckerSimplify(trace, start, maxIndex, keep, epsilon);
            douglasPeuckerSimplify(trace, maxIndex, end, keep, epsilon);
        }
    }

    // Simplify station trace when it exceeds TRACE_MAX_POINTS
    static void simplifyTrace(MapStation* station) {
        if (station->traceCount < TRACE_MAX_POINTS) return;

        // Build linear array from circular buffer
        TracePoint linear[TRACE_MAX_POINTS];
        for (int i = 0; i < station->traceCount; i++) {
            int idx = (station->traceHead - station->traceCount + i + TRACE_MAX_POINTS) % TRACE_MAX_POINTS;
            linear[i] = station->trace[idx];
        }

        // Mark which points to keep (first and last always kept)
        bool keep[TRACE_MAX_POINTS];
        for (int i = 0; i < TRACE_MAX_POINTS; i++) keep[i] = false;
        keep[0] = true;
        keep[station->traceCount - 1] = true;

        // Apply Douglas-Peucker with tolerance (0.00008° ≈ 9m)
        douglasPeuckerSimplify(linear, 0, station->traceCount - 1, keep, 0.00008f);

        // Rebuild trace with kept points only
        int newCount = 0;
        for (int i = 0; i < station->traceCount; i++) {
            if (keep[i]) {
                station->trace[newCount++] = linear[i];
            }
        }

        station->traceCount = newCount;
        station->traceHead = newCount % TRACE_MAX_POINTS;

        ESP_LOGD(TAG, "Trace simplified: %d -> %d points", TRACE_MAX_POINTS, newCount);
    }

    // Add or update a station for the map
    void addMapStation(const String& callsign, float lat, float lon, const String& symbol, const String& overlay, int rssi) {
        // Skip if no valid position
        if (lat == 0.0 && lon == 0.0) return;

        // Check if station already exists (update it)
        for (int i = 0; i < MAP_STATIONS_MAX; i++) {
            if (mapStations[i].valid && mapStations[i].callsign == callsign) {
                // Record old position in trace if station moved (delta > ~11m)
                float dlat = fabs(mapStations[i].latitude - lat);
                float dlon = fabs(mapStations[i].longitude - lon);
                if (dlat > 0.0001f || dlon > 0.0001f) {
                    // Simplify trace if full, to keep origin and end visible
                    if (mapStations[i].traceCount >= TRACE_MAX_POINTS) {
                        simplifyTrace(&mapStations[i]);
                    }

                    // Add old position to trace
                    mapStations[i].trace[mapStations[i].traceHead].lat = mapStations[i].latitude;
                    mapStations[i].trace[mapStations[i].traceHead].lon = mapStations[i].longitude;
                    mapStations[i].trace[mapStations[i].traceHead].time = millis();
                    mapStations[i].traceHead = (mapStations[i].traceHead + 1) % TRACE_MAX_POINTS;
                    if (mapStations[i].traceCount < TRACE_MAX_POINTS) {
                        mapStations[i].traceCount++;
                    }
                }
                mapStations[i].latitude  = lat;
                mapStations[i].longitude = lon;
                mapStations[i].symbol    = symbol;
                mapStations[i].overlay   = overlay;
                mapStations[i].rssi      = rssi;
                mapStations[i].lastTime  = millis();
                return;
            }
        }

        // Find empty slot or replace oldest
        int oldestIndex = 0;
        uint32_t oldestTime = UINT32_MAX;

        for (int i = 0; i < MAP_STATIONS_MAX; i++) {
            if (!mapStations[i].valid) {
                // Empty slot found
                mapStations[i].callsign  = callsign;
                mapStations[i].latitude  = lat;
                mapStations[i].longitude = lon;
                mapStations[i].symbol    = symbol;
                mapStations[i].overlay   = overlay;
                mapStations[i].rssi      = rssi;
                mapStations[i].lastTime   = millis();
                mapStations[i].valid      = true;
                mapStations[i].traceCount = 0;
                mapStations[i].traceHead  = 0;
                mapStationsCount++;
                return;
            }
            if (mapStations[i].lastTime < oldestTime) {
                oldestTime = mapStations[i].lastTime;
                oldestIndex = i;
            }
        }

        // Replace oldest station
        mapStations[oldestIndex].callsign  = callsign;
        mapStations[oldestIndex].latitude  = lat;
        mapStations[oldestIndex].longitude = lon;
        mapStations[oldestIndex].symbol    = symbol;
        mapStations[oldestIndex].overlay   = overlay;
        mapStations[oldestIndex].rssi       = rssi;
        mapStations[oldestIndex].lastTime   = millis();
        mapStations[oldestIndex].valid      = true;
        mapStations[oldestIndex].traceCount = 0;
        mapStations[oldestIndex].traceHead  = 0;
    }

    // Clean old map stations (older than rememberStationTime)
    void cleanOldMapStations() {
        uint32_t timeout = Config.rememberStationTime * 60 * 1000;
        for (int i = 0; i < MAP_STATIONS_MAX; i++) {
            if (mapStations[i].valid && (millis() - mapStations[i].lastTime > timeout)) {
                mapStations[i].valid      = false;
                mapStations[i].callsign   = "";
                mapStations[i].traceCount = 0;
                mapStations[i].traceHead  = 0;
                mapStationsCount--;
            }
        }
    }

    // Get station by index
    MapStation* getMapStation(int index) {
        if (index < 0 || index >= MAP_STATIONS_MAX) return nullptr;
        if (!mapStations[index].valid) return nullptr;
        return &mapStations[index];
    }

    // Find station by callsign
    MapStation* findMapStation(const String& callsign) {
        for (int i = 0; i < MAP_STATIONS_MAX; i++) {
            if (mapStations[i].valid && mapStations[i].callsign == callsign) {
                return &mapStations[i];
            }
        }
        return nullptr;
    }

    String getNearStation(uint8_t position) {
        if (nearbyStations[position].callsign == "") return "";
        return nearbyStations[position].callsign + "> " + String(nearbyStations[position].distance,2) + "km " + String(nearbyStations[position].course);
    }

    void deleteListenedStationsByTime() {
        for (int a = 0; a < nearbyStationsSize; a++) {                       // clean nearbyStations[] after time
            if (nearbyStations[a].callsign != "" && (millis() - nearbyStations[a].lastTime > Config.rememberStationTime * 60 * 1000)) {
                nearbyStations[a].callsign    = "";
                nearbyStations[a].distance    = 0.0;
                nearbyStations[a].course      = 0;
                nearbyStations[a].lastTime    = 0;
            }
        }

        for (int b = 0; b < nearbyStationsSize - 1; b++) {
            for (int c = 0; c < nearbyStationsSize - b - 1; c++) {
                if (nearbyStations[c].callsign == "") {       // get "" nearbyStations[] at the end
                    nearStation temp    = nearbyStations[c];
                    nearbyStations[c]     = nearbyStations[c + 1];
                    nearbyStations[c + 1] = temp;
                }
            }
        }
        lastDeleteListenedStation = millis();
    }

    void checkListenedStationsByTimeAndDelete() {
        if (millis() - lastDeleteListenedStation > Config.rememberStationTime * 60 * 1000) deleteListenedStationsByTime();
    }

    void orderListenedStationsByDistance(const String& callsign, float distance, float course) {   
        bool shouldSortbyDistance = false;
        bool callsignInNearStations = false;

        for (int a = 0; a < nearbyStationsSize; a++) {                       // check if callsign is in nearbyStations[]
            if (nearbyStations[a].callsign == callsign) {
                callsignInNearStations  = true;
                nearbyStations[a].lastTime = millis();        // update listened millis()
                if (nearbyStations[a].distance != distance) { // update distance if needed
                    nearbyStations[a].distance    = distance;
                    shouldSortbyDistance        = true;
                }
                break;
            }
        }
    
        if (!callsignInNearStations) {                      // callsign not in nearbyStations[]
            for (int b = 0; b < nearbyStationsSize; b++) {                   // if nearbyStations[] is available
                if (nearbyStations[b].callsign == "") {
                    shouldSortbyDistance        = true;
                    nearbyStations[b].callsign    = callsign;
                    nearbyStations[b].distance    = distance;
                    nearbyStations[b].course      = int(course);
                    nearbyStations[b].lastTime    = millis();
                    break;
                }
            }

            if (!shouldSortbyDistance) {                    // if no more nearbyStations[] available , it compares distances to move and replace
                for (int c = 0; c < nearbyStationsSize; c++) {
                    if (nearbyStations[c].distance > distance) {
                        for (int d = nearbyStationsSize - 1; d > c; d--) nearbyStations[d] = nearbyStations[d - 1]; // move all one position down
                        nearbyStations[c].callsign    = callsign;
                        nearbyStations[c].distance    = distance;
                        nearbyStations[c].course      = int(course);
                        nearbyStations[c].lastTime    = millis();
                        break;
                    }
                }
            }
        }

        if (shouldSortbyDistance) { /*  BUBLE SORT  */      // sorts by distance (only nearbyStations[] that are not "")
            for (int e = 0; e < nearbyStationsSize - 1; e++) {
                for (int f = 0; f < nearbyStationsSize - e - 1; f++) {
                    if (nearbyStations[f].callsign != "" && nearbyStations[f + 1].callsign != "") {
                        if (nearbyStations[f].distance > nearbyStations[f + 1].distance) {
                            nearStation temp        = nearbyStations[f];
                            nearbyStations[f]       = nearbyStations[f + 1];
                            nearbyStations[f + 1]   = temp;
                        }
                    }
                }
            }
        }
    }

    void checkStandingUpdateTime() {
        if (!sendUpdate && lastTx >= Config.standingUpdateTime * 60 * 1000) {
            sendUpdate = true;
            sendStandingUpdate = true;
            if (!gpsIsActive) {
                SLEEP_Utils::gpsWakeUp();
            }
        }
    }

    void sendBeacon() {
        if (sendStartTelemetry && ((Config.battery.sendVoltage && Config.battery.voltageAsTelemetry) || (Config.telemetry.sendTelemetry && wxModuleFound)) && lastTxTime > 0) TELEMETRY_Utils::sendEquationsUnitsParameters();

        String path = Config.path;
        if (gpsFix.speed_kph() > 200 || gpsFix.alt.whole > 9000) path = ""; // avoid plane speed and altitude
        String packet;
        if (miceActive) {
            packet = APRSPacketLib::generateMiceGPSBeaconPacket(currentBeacon->micE, currentBeacon->callsign, currentBeacon->symbol, currentBeacon->overlay, path, gpsFix.latitude(), gpsFix.longitude(), gpsFix.heading(), gpsSpeedKnots(), gpsFix.alt.whole);
        } else {
            packet = APRSPacketLib::generateBase91GPSBeaconPacket(currentBeacon->callsign, "APLRT1", path, currentBeacon->overlay, APRSPacketLib::encodeGPSIntoBase91(gpsFix.latitude(), gpsFix.longitude(), gpsFix.heading(), gpsSpeedKnots(), currentBeacon->symbol, Config.sendAltitude, gpsAltFeet(), sendStandingUpdate));
        }

        String batteryVoltage = BATTERY_Utils::getBatteryInfoVoltage();
        bool shouldSleepLowVoltage = false;
        #if defined(BATTERY_PIN) || defined(HAS_AXP192) || defined(HAS_AXP2101)
            if (Config.battery.monitorVoltage && batteryVoltage.toFloat() < Config.battery.sleepVoltage) shouldSleepLowVoltage = true;
        #endif
        
        if (!shouldSleepLowVoltage) {
            String comment = (winlinkCommentState ? "winlink" : currentBeacon->comment);
            if (comment == "") comment = "LoRa APRS Tracker";
            int sendCommentAfterXBeacons = ((winlinkCommentState || Config.battery.sendVoltageAlways) ? 1 : Config.sendCommentAfterXBeacons);

            if (Config.battery.sendVoltage && !Config.battery.voltageAsTelemetry) {
                #if defined(HAS_AXP192) || defined(HAS_AXP2101)
                    String batteryChargeCurrent = POWER_Utils::getBatteryInfoCurrent();
                    #if defined(HAS_AXP192)
                        comment += " Batt=";
                        comment += batteryVoltage;
                        comment += "V (";
                        comment += batteryChargeCurrent;
                        comment += "mA)";
                    #elif defined(HAS_AXP2101)
                        comment += " Batt=";
                        comment += String(batteryVoltage.toFloat(),2);
                        comment += "V (";
                        comment += batteryChargeCurrent;
                        comment += "%)";
                    #endif
                #elif defined(BATTERY_PIN)
                    comment += " Batt=";
                    comment += String(batteryVoltage.toFloat(),2);
                    comment += "V (";
                    comment += BATTERY_Utils::getPercentVoltageBattery(batteryVoltage.toFloat());
                    comment += "%)";
                #endif
            }

            // Add LoRa frequency and data rate info
            if (Config.lora.sendInfo) {
                comment += " ";
                comment += String(Config.loraTypes[loraIndex].frequency / 1000000.0, 3);
                comment += "MHz ";
                comment += String(Config.loraTypes[loraIndex].dataRate);
                comment += "bps";
            }

            if (comment != "" || (Config.battery.sendVoltage && Config.battery.voltageAsTelemetry) || (Config.telemetry.sendTelemetry && wxModuleFound)) {
                updateCounter++;
                if (updateCounter >= sendCommentAfterXBeacons) {
                    if (comment != "") packet += comment;
                    if ((Config.battery.sendVoltage && Config.battery.voltageAsTelemetry) || (Config.telemetry.sendTelemetry && wxModuleFound)) packet += TELEMETRY_Utils::generateEncodedTelemetry();
                    updateCounter = 0;
                }
            }
        } else {
            packet += "**LowVoltagePowerOff**";
        }

        #ifdef USE_LVGL_UI
            LVGL_UI::showTxPacket(packet.c_str());
        #else
            displayShow("<<< TX >>>", "", packet, 100);
        #endif
        LoRa_Utils::sendNewPacket(packet);

        // Upload to APRS-IS if connected
        if (APRS_IS_Utils::isConnected()) {
            APRS_IS_Utils::upload(packet);
        }

        if (Config.bluetooth.useBLE) BLE_Utils::sendToPhone(packet);   // send Tx packets to Phone too

        if (shouldSleepLowVoltage) POWER_Utils::shutdown();
        
        if (smartBeaconActive) {
            lastTxLat       = gpsFix.latitude();
            lastTxLng       = gpsFix.longitude();
            previousHeading = currentHeading;
            lastTxDistance  = 0.0;

            // Add own position to GPS trace on map + GPX recording
            #ifdef USE_LVGL_UI
                UIMapManager::addOwnTracePoint();
                GPXWriter::addPoint(lastTxLat, lastTxLng,
                                    gpsFix.alt.whole, gpsHdop(), gpsFix.speed_kph());
            #endif
        }
        lastTxTime  = millis();
        sendUpdate  = false;
        if (currentBeacon->gpsEcoMode) gpsShouldSleep = true;
    }

    void saveIndex(uint8_t type, uint8_t index) {
        String filePath;
        switch (type) {
            case 0: filePath = "/callsignIndex.txt"; break;
            case 1: filePath = "/freqIndex.txt"; break;
            case 2: filePath = "/brightness.txt"; break;
            case 3: filePath = "/displayEcoMode.txt"; break;
            case 4: filePath = "/ecoTimeout.txt"; break;
            default: return; // Invalid type, exit function
        }

        File fileIndex = SPIFFS.open(filePath, "w");
        if (!fileIndex) return;

        String dataToSave = String(index);
        if (fileIndex.println(dataToSave)) {
            String logMessage;
            switch (type) {
                case 0: logMessage = "New Callsign Index"; break;
                case 1: logMessage = "New Frequency Index"; break;
                case 2: logMessage = "New Brightness"; break;
                case 3: logMessage = "Display Eco Mode"; break;
                case 4: logMessage = "ECO Timeout"; break;
                default: return; // Invalid type, exit function
            }
            ESP_LOGD(TAG, "%s = %d saved to SPIFFS", logMessage.c_str(), index);
        }
        fileIndex.close();
    }

    void loadIndex(uint8_t type) {
        String filePath;
        switch (type) {
            case 0: filePath = "/callsignIndex.txt"; break;
            case 1: filePath = "/freqIndex.txt"; break;
            case 2: filePath = "/brightness.txt"; break;
            case 3: filePath = "/displayEcoMode.txt"; break;
            case 4: filePath = "/ecoTimeout.txt"; break;
            default: return; // Invalid type, exit function
        }

        if (!SPIFFS.exists(filePath)) {
            switch (type) {
                case 0: myBeaconsIndex = 0; break;
                case 1: loraIndex = 0; break;
                case 2:
                    #ifdef HAS_TFT
                        screenBrightness = 255;
                    #else
                        screenBrightness = 1;
                    #endif
                    break;
                case 3: displayEcoMode = false; break;  // Default: off
                case 4: break;  // Keep Config.display.timeout default from JSON
                default: return; // Invalid type, exit function
            }
            return;
        } else {
            File fileIndex = SPIFFS.open(filePath, "r");
            while (fileIndex.available()) {
                String firstLine = fileIndex.readStringUntil('\n');
                int index = firstLine.toInt();
                String logMessage;
                if (type == 0) {
                    myBeaconsIndex = index;
                    logMessage = "Callsign Index:";
                } else if (type == 1) {
                    loraIndex = index;
                    logMessage = "LoRa Freq Index:";
                } else if (type == 2) {
                    screenBrightness = index;
                    logMessage = "Brightness:";
                } else if (type == 3) {
                    displayEcoMode = (index != 0);
                    logMessage = "Display Eco Mode:";
                } else if (type == 4) {
                    if (index >= 2 && index <= 15) {
                        Config.display.timeout = index;
                    }
                    logMessage = "ECO Timeout:";
                }
                ESP_LOGD(TAG, "%s %d (from SPIFFS)", logMessage.c_str(), index);
            }
            fileIndex.close();
        }
    }

}