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

uint32_t    lastDeleteListenedTracker;

struct nearTracker {
    String      callsign;
    float       distance;
    int         course;
    uint32_t    lastTime;
};

nearTracker nearTrackers[4];


namespace STATION_Utils {

    void nearTrackerInit() {
        for (int i = 0; i < 4; i++) {
            nearTrackers[i].callsign    = "";
            nearTrackers[i].distance    = 0.0;
            nearTrackers[i].course      = 0;
            nearTrackers[i].lastTime    = 0;
        }
    }

    const String getNearTracker(uint8_t position) {
        if (nearTrackers[position].callsign == "") {
            return "";
        } else {
            return nearTrackers[position].callsign + "> " + String(nearTrackers[position].distance,2) + "km " + String(nearTrackers[position].course);
        }
    }

    void deleteListenedTrackersbyTime() {
        for (int a = 0; a < 4; a++) {                       // clean nearTrackers[] after time
            if (nearTrackers[a].callsign != "" && (millis() - nearTrackers[a].lastTime > Config.rememberStationTime * 60 * 1000)) {
                nearTrackers[a].callsign    = "";
                nearTrackers[a].distance    = 0.0;
                nearTrackers[a].course      = 0;
                nearTrackers[a].lastTime    = 0;
            }
        }

        for (int b = 0; b < 4 - 1; b++) {
            for (int c = 0; c < 4 - b - 1; c++) {
                if (nearTrackers[c].callsign == "") {       // get "" nearTrackers[] at the end
                    nearTracker temp = nearTrackers[c];
                    nearTrackers[c] = nearTrackers[c + 1];
                    nearTrackers[c + 1] = temp;
                }
            }
        }
        lastDeleteListenedTracker = millis();
    }

    void checkListenedTrackersByTimeAndDelete() {
        if (millis() - lastDeleteListenedTracker > Config.rememberStationTime * 60 * 1000) {
            deleteListenedTrackersbyTime();
        }
    }

    void orderListenedTrackersByDistance(const String& callsign, float distance, float course) {   
        bool shouldSortbyDistance = false;
        bool callsignInNearTrackers = false;

        for (int a = 0; a < 4; a++) {                       // check if callsign is in nearTrackers[]
            if (nearTrackers[a].callsign == callsign) {
                callsignInNearTrackers  = true;
                nearTrackers[a].lastTime = millis();        // update listened millis()
                if (nearTrackers[a].distance != distance) { // update distance if needed
                    nearTrackers[a].distance    = distance;
                    shouldSortbyDistance        = true;
                }
                break;           
            }
        }
    
        if (!callsignInNearTrackers) {                      // callsign not in nearTrackers[]
            for (int b = 0; b < 4; b++) {                   // if nearTrackers[] is available
                if (nearTrackers[b].callsign == "") {
                    shouldSortbyDistance        = true;
                    nearTrackers[b].callsign    = callsign;
                    nearTrackers[b].distance    = distance;
                    nearTrackers[b].course      = int(course);
                    nearTrackers[b].lastTime    = millis();
                    break;
                }
            }

            if (!shouldSortbyDistance) {                    // if no more nearTrackers[] available , it compares distances to move and replace
                for (int c = 0; c < 4; c++) {
                    if (nearTrackers[c].distance > distance) {
                        for (int d = 3; d > c; d--) {
                            nearTrackers[d] = nearTrackers[d - 1];
                        }
                        nearTrackers[c].callsign    = callsign;
                        nearTrackers[c].distance    = distance;
                        nearTrackers[c].course      = int(course);
                        nearTrackers[c].lastTime    = millis();
                        break;
                    }
                }
            }
        }

        if (shouldSortbyDistance) {                         // sorts by distance (only nearTrackers[] that are not "")
            for (int f = 0; f < 4 - 1; f++) {
                for (int g = 0; g < 4 - f - 1; g++) {
                    if (nearTrackers[g].callsign != "" && nearTrackers[g + 1].callsign != "") {
                        if (nearTrackers[g].distance > nearTrackers[g + 1].distance) {
                            nearTracker temp = nearTrackers[g];
                            nearTrackers[g] = nearTrackers[g + 1];
                            nearTrackers[g + 1] = temp;
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
        if (sendStartTelemetry && ((Config.battery.sendVoltage && Config.battery.voltageAsTelemetry) || (Config.telemetry.sendTelemetry && wxModuleFound)) && lastTxTime > 0) {                
            TELEMETRY_Utils::sendEquationsUnitsParameters();
            sendStartTelemetry = false;
        }

        String path = Config.path;
        if (gps.speed.kmph() > 200 || gps.altitude.meters() > 9000) path = ""; // avoid plane speed and altitude
        String packet;
        if (miceActive) {
            packet = APRSPacketLib::generateMiceGPSBeaconPacket(currentBeacon->micE, currentBeacon->callsign, currentBeacon->symbol, currentBeacon->overlay, path, gps.location.lat(), gps.location.lng(), gps.course.deg(), gps.speed.knots(), gps.altitude.meters());
        } else {
            packet = APRSPacketLib::generateBase91GPSBeaconPacket(currentBeacon->callsign, "APLRT1", path, currentBeacon->overlay, APRSPacketLib::encodeGPSIntoBase91(gps.location.lat(),gps.location.lng(), gps.course.deg(), gps.speed.knots(), currentBeacon->symbol, Config.sendAltitude, gps.altitude.feet(), sendStandingUpdate, "GPS"));
        }
        String comment;
        int sendCommentAfterXBeacons;
        if (winlinkCommentState || Config.battery.sendVoltageAlways) {
            if (winlinkCommentState) comment = " winlink";
            sendCommentAfterXBeacons = 1;
        } else {
            comment = currentBeacon->comment;
            sendCommentAfterXBeacons = Config.sendCommentAfterXBeacons;
        }

        String batteryVoltage = POWER_Utils::getBatteryInfoVoltage();
        bool shouldSleepLowVoltage = false;
        #if defined(BATTERY_PIN) || defined(HAS_AXP192) || defined(HAS_AXP2101)
            if (Config.battery.monitorVoltage && batteryVoltage.toFloat() < Config.battery.sleepVoltage) {
                shouldSleepLowVoltage   = true;
            }
        #endif

        if (Config.battery.sendVoltage && !Config.battery.voltageAsTelemetry) {
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
            #elif defined(BATTERY_PIN) && !defined(HAS_AXP192) && !defined(HAS_AXP2101)
                comment += " Bat=";
                comment += String(batteryVoltage.toFloat(),2);
                comment += "V";
                comment += BATTERY_Utils::getPercentVoltageBattery(batteryVoltage.toFloat());
                comment += "%";
            #endif
        }
        
        if (shouldSleepLowVoltage) {
            packet += " **LowVoltagePowerOff**";
        } else {
            if (comment != "" || (Config.battery.sendVoltage && Config.battery.voltageAsTelemetry) || (Config.telemetry.sendTelemetry && wxModuleFound)) {
                updateCounter++;
                if (updateCounter >= sendCommentAfterXBeacons) {
                    if (comment != "") packet += comment;
                    if ((Config.battery.sendVoltage && Config.battery.voltageAsTelemetry) || (Config.telemetry.sendTelemetry && wxModuleFound)) packet += TELEMETRY_Utils::generateEncodedTelemetry();
                    updateCounter = 0;
                }
            }
        }
        displayShow("<<< TX >>>", "", packet, 100);
        LoRa_Utils::sendNewPacket(packet);

        if (Config.bluetooth.useBLE) BLE_Utils::sendToPhone(packet);   // send Tx packets to Phone too

        if (shouldSleepLowVoltage) {
            delay(3000);
            POWER_Utils::shutdown();
        }
        
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