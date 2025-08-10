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

#include <APRSPacketLib.h>
#include <TinyGPS++.h>
#include <SPIFFS.h>
#include "telemetry_utils.h"
#include "station_utils.h"
#include "battery_utils.h"
#include "configuration.h"
#include "board_pinout.h"
#include "power_utils.h"
#include "sleep_utils.h"
#include "lora_utils.h"
#include "ble_utils.h"
#include "wx_utils.h"
#include "display.h"
#include "logger.h"

extern Configuration        Config;
extern Beacon               *currentBeacon;
extern logging::Logger      logger;
extern TinyGPSPlus          gps;
extern uint8_t              myBeaconsIndex;
extern uint8_t              loraIndex;
extern uint8_t              screenBrightness;

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


namespace STATION_Utils {

    void nearStationInit() {
        for (int i = 0; i < nearbyStationsSize; i++) {
            nearbyStations[i].callsign    = "";
            nearbyStations[i].distance    = 0.0;
            nearbyStations[i].course      = 0;
            nearbyStations[i].lastTime    = 0;
        }
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
        if (gps.speed.kmph() > 200 || gps.altitude.meters() > 9000) path = ""; // avoid plane speed and altitude
        String packet;
        if (miceActive) {
            packet = APRSPacketLib::generateMiceGPSBeaconPacket(currentBeacon->micE, currentBeacon->callsign, currentBeacon->symbol, currentBeacon->overlay, path, gps.location.lat(), gps.location.lng(), gps.course.deg(), gps.speed.knots(), gps.altitude.meters());
        } else {
            packet = APRSPacketLib::generateBase91GPSBeaconPacket(currentBeacon->callsign, "APLRT1", path, currentBeacon->overlay, APRSPacketLib::encodeGPSIntoBase91(gps.location.lat(),gps.location.lng(), gps.course.deg(), gps.speed.knots(), currentBeacon->symbol, Config.sendAltitude, gps.altitude.feet(), sendStandingUpdate));
        }

        String batteryVoltage = BATTERY_Utils::getBatteryInfoVoltage();
        bool shouldSleepLowVoltage = false;
        #if defined(BATTERY_PIN) || defined(HAS_AXP192) || defined(HAS_AXP2101)
            if (Config.battery.monitorVoltage && batteryVoltage.toFloat() < Config.battery.sleepVoltage) shouldSleepLowVoltage = true;
        #endif
        
        if (!shouldSleepLowVoltage) {
            String comment = (winlinkCommentState ? "winlink" : currentBeacon->comment);
            int sendCommentAfterXBeacons = ((winlinkCommentState || Config.battery.sendVoltageAlways) ? 1 : Config.sendCommentAfterXBeacons);

            if (Config.battery.sendVoltage && !Config.battery.voltageAsTelemetry) {
                #if defined(HAS_AXP192) || defined(HAS_AXP2101)
                    String batteryChargeCurrent = POWER_Utils::getBatteryInfoCurrent();
                    #if defined(HAS_AXP192)
                        comment += " Bat=";
                        comment += batteryVoltage;
                        comment += "V (";
                        comment += batteryChargeCurrent;
                        comment += "mA)";
                    #elif defined(HAS_AXP2101)
                        comment += " Bat=";
                        comment += String(batteryVoltage.toFloat(),2);
                        comment += "V (";
                        comment += batteryChargeCurrent;
                        comment += "%)";
                    #endif
                #elif defined(BATTERY_PIN)
                    comment += " Bat=";
                    comment += String(batteryVoltage.toFloat(),2);
                    comment += "V";
                    comment += BATTERY_Utils::getPercentVoltageBattery(batteryVoltage.toFloat());
                    comment += "%";
                #endif
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

        displayShow("<<< TX >>>", "", packet, 100);
        LoRa_Utils::sendNewPacket(packet);

        if (Config.bluetooth.useBLE) BLE_Utils::sendToPhone(packet);   // send Tx packets to Phone too

        if (shouldSleepLowVoltage) POWER_Utils::shutdown();
        
        if (smartBeaconActive) {
            lastTxLat       = gps.location.lat();
            lastTxLng       = gps.location.lng();
            previousHeading = currentHeading;
            lastTxDistance  = 0.0;
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
                default: return; // Invalid type, exit function
            }
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Main", "%s saved to SPIFFS", logMessage.c_str());
        }
        fileIndex.close();
    }
    
    void loadIndex(uint8_t type) {
        String filePath;
        switch (type) {
            case 0: filePath = "/callsignIndex.txt"; break;
            case 1: filePath = "/freqIndex.txt"; break;
            case 2: filePath = "/brightness.txt"; break;
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
                } else {
                    screenBrightness = index;
                    logMessage = "Brightness:";
                }
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Main", "%s %s", logMessage.c_str(), firstLine);
            }        
            fileIndex.close();
        }
    }

}