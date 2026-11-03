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

#include <ArduinoJson.h>
#ifdef ARDUINO_ARCH_NRF52
// Direct includes here (in addition to via the SPIFFS shim) so PIO's LDF
// discovers the dependency from a tracked project source — LDF does NOT crawl
// custom include paths added via -I in build_flags.
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
// First-boot defaults are sourced from data/tracker_conf.json via the
// tools/embed_config.py pre-build script, which emits this header.
#include "generated/default_config_embed.h"
#endif
#include <SPIFFS.h>
#include "configuration.h"
#include "board_pinout.h"
#include "display.h"
#include "logger.h"
#include "smartbeacon_utils.h"


extern logging::Logger logger;

bool Configuration::writeFile() {

    Serial.println("Saving config..");

    JsonDocument data;
    #ifdef ARDUINO_ARCH_NRF52
        // Adafruit_LittleFS's FILE_O_WRITE doesn't truncate (it seeks to end
        // for SD-compatible append). Earlier we used truncate(0)+seek(0) but
        // that triggered an lfs pcache assertion when called right after a
        // close(). Removing-then-creating is cleaner: lfs state is fresh on
        // open, no leftover cache from the previous handle.
        SPIFFS.remove("/tracker_conf.json");
    #endif
    File configFile = SPIFFS.open("/tracker_conf.json", "w");

    if (!configFile) {
        Serial.println("Error: Could not open config file for writing");
        return false;
    }
    #ifndef ARDUINO_ARCH_NRF52
    try {
    #else
    {   // Adafruit nRF52 BSP is built without exceptions; equivalent unwound block.
    #endif

        data["wifiAP"]["password"]                  = wifiAP.password;

        beacons[0].callsign.trim();
        beacons[0].callsign.toUpperCase();
        data["beacons"][0]["callsign"]           = beacons[0].callsign;
        data["beacons"][0]["symbol"]             = beacons[0].symbol;
        data["beacons"][0]["overlay"]            = beacons[0].overlay;
        data["beacons"][0]["micE"]               = beacons[0].micE;
        data["beacons"][0]["comment"]            = beacons[0].comment;
        data["beacons"][0]["smartBeaconActive"]  = beacons[0].smartBeaconActive;
        data["beacons"][0]["smartBeaconSetting"] = beacons[0].smartBeaconSetting;
        data["beacons"][0]["profileLabel"]       = beacons[0].profileLabel;
        data["beacons"][0]["status"]             = beacons[0].status;
        data["beacons"][0]["tacticalCallsign"]   = beacons[0].tacticalCallsign;

        data["display"]["ecoMode"]                  = display.ecoMode;
        data["display"]["timeout"]                  = display.timeout;
        data["display"]["turn180"]                  = display.turn180;
        data["display"]["invertDisplay"]            = display.invertDisplay;
        data["display"]["ledEnabled"]               = display.ledEnabled;

        data["bluetooth"]["active"]                 = bluetooth.active;
        data["bluetooth"]["deviceName"]             = bluetooth.deviceName;

        data["lora"][0]["frequency"]       = loraTypes[0].frequency;
        data["lora"][0]["spreadingFactor"] = loraTypes[0].spreadingFactor;
        data["lora"][0]["signalBandwidth"] = loraTypes[0].signalBandwidth;
        data["lora"][0]["codingRate4"]     = loraTypes[0].codingRate4;
        data["lora"][0]["power"]           = loraTypes[0].power;

        data["battery"]["sendVoltage"]              = battery.sendVoltage;
        data["battery"]["sendVoltageAlways"]        = battery.sendVoltageAlways;
        data["battery"]["sleepVoltage"]             = battery.sleepVoltage;

        data["pttTrigger"]["active"]                = ptt.active;
        data["pttTrigger"]["reverse"]               = ptt.reverse;
        data["pttTrigger"]["preDelay"]              = ptt.preDelay;
        data["pttTrigger"]["postDelay"]             = ptt.postDelay;
        data["pttTrigger"]["io_pin"]                = ptt.io_pin;

        data["other"]["sendCommentAfterXBeacons"]   = sendCommentAfterXBeacons;
        data["other"]["beaconPath"]                 = beaconPath;
        data["other"]["nonSmartBeaconRateSec"]      = nonSmartBeaconRate;
        data["other"]["sendAltitude"]               = sendAltitude;
        data["other"]["sendSpeedCourse"]            = sendSpeedCourse;
        data["other"]["cadActive"]                  = cadActive;
        data["other"]["digiMode"]                   = (int)digiMode;

        data["customSmartBeacon"]["slowRate"]       = customSmartBeacon.slowRate;
        data["customSmartBeacon"]["slowSpeed"]      = customSmartBeacon.slowSpeed;
        data["customSmartBeacon"]["fastRate"]       = customSmartBeacon.fastRate;
        data["customSmartBeacon"]["fastSpeed"]      = customSmartBeacon.fastSpeed;
        data["customSmartBeacon"]["turnMinDeg"]     = customSmartBeacon.turnMinDeg;
        data["customSmartBeacon"]["turnSlope"]      = customSmartBeacon.turnSlope;

        data["wifiSTA"]["enabled"]                  = wifiSTA.enabled;
        JsonArray wifiNetworksOut = data["wifiSTA"]["networks"].to<JsonArray>();
        for (auto& net : wifiSTA.networks) {
            JsonObject netObj = wifiNetworksOut.add<JsonObject>();
            netObj["ssid"]     = net.ssid;
            netObj["password"] = net.password;
        }

        data["deviceRole"]                          = (int)deviceRole;
        data["gpsSource"]                           = (int)gpsSource;

        data["fixedPosition"]["latitude"]            = fixedPosition.latitude;
        data["fixedPosition"]["longitude"]           = fixedPosition.longitude;
        data["fixedPosition"]["elevation"]           = fixedPosition.elevation;

        data["aprsIS"]["server"]                    = aprsIS.server;
        data["aprsIS"]["port"]                      = aprsIS.port;
        data["aprsIS"]["passcode"]                  = aprsIS.passcode;
        data["aprsIS"]["filter"]                    = aprsIS.filter;
        data["aprsIS"]["downlinkEnabled"]           = aprsIS.downlinkEnabled;

        data["tcpKISS"]["port"]                     = tcpKISS.port;

        data["phg"]["enabled"]                      = phg.enabled;
        data["phg"]["power"]                        = phg.power;
        data["phg"]["height"]                       = phg.height;
        data["phg"]["gain"]                         = phg.gain;
        data["phg"]["directivity"]                  = phg.directivity;
        data["phg"]["beaconRate"]                   = phg.beaconRate;

        data["remoteCfg"]["enabled"]                = remoteCfg.enabled;
        data["remoteCfg"]["token"]                  = remoteCfg.token;
        data["remoteCfg"]["unlockWindowSec"]        = remoteCfg.unlockWindowSec;

        serializeJson(data, configFile);
        configFile.close();
        return true;
    #ifndef ARDUINO_ARCH_NRF52
    } catch (...) {
        Serial.println("Error: Exception occurred while saving config");
        configFile.close();
        return false;
    #endif
    }
}

bool Configuration::readFile() {
    Serial.println("Reading config..");
    #ifdef ARDUINO_ARCH_NRF52
    #endif
    File configFile = SPIFFS.open("/tracker_conf.json", "r");

    if (configFile) {
        bool needsRewrite = false;
        JsonDocument data;
        #ifdef ARDUINO_ARCH_NRF52
        // Slurp into a String buffer first, then deserialize from buffer
        // rather than streaming directly from the lfs File. The lfs Stream
        // interface had pcache assertion issues during T114 bring-up; the
        // buffer-based path is more robust and bounds heap up-front. 16 KB
        // cap matches the import-paste cap.
        size_t fsz = configFile.size();
        if (fsz > 16384) {
            Serial.println("[readFile] config file too big, treating as corrupt");
            configFile.close();
            return false;
        }
        String buf;
        buf.reserve(fsz + 1);
        while (configFile.available()) {
            int c = configFile.read();
            if (c < 0) break;
            buf += (char)c;
        }
        DeserializationError error = deserializeJson(data, buf);
        #else
        DeserializationError error = deserializeJson(data, configFile);
        #endif
        if (error) {
            Serial.println("Failed to read file, using default configuration");
        }

        // Clear vectors before re-populating. Matters on nRF52: the gutted
        // Configuration constructor calls setDefaultValues() to seed the
        // vectors so file-scope globals (myBeaconsSize etc.) get sane values
        // before main() runs; readFile() then runs from setup() and would
        // otherwise append to the already-seeded vectors instead of replacing.
        beacons.clear();
        loraTypes.clear();
        wifiSTA.networks.clear();

        if (data["wifiAP"]["password"].isNull()) needsRewrite = true;
        wifiAP.password             = data["wifiAP"]["password"] | "1234567890";

        JsonArray BeaconsArray = data["beacons"];
        for (int i = 0; i < BeaconsArray.size(); i++) {
            Beacon bcn;

            bcn.callsign                = BeaconsArray[i]["callsign"] | "NOCALL-7";
            bcn.callsign.toUpperCase();
            bcn.symbol                  = BeaconsArray[i]["symbol"] | "[";
            bcn.overlay                 = BeaconsArray[i]["overlay"] | "/";
            bcn.micE                    = BeaconsArray[i]["micE"] | "";
            bcn.comment                 = BeaconsArray[i]["comment"] | "";
            bcn.status                  = BeaconsArray[i]["status"] | "";
            bcn.smartBeaconActive       = BeaconsArray[i]["smartBeaconActive"] | true;
            bcn.smartBeaconSetting      = BeaconsArray[i]["smartBeaconSetting"] | 0;
            bcn.profileLabel            = BeaconsArray[i]["profileLabel"] | "";
            bcn.tacticalCallsign        = BeaconsArray[i]["tacticalCallsign"] | "";
            beacons.push_back(bcn);
        }

        if (data["display"]["ecoMode"].isNull() ||
            data["display"]["timeout"].isNull() ||
            data["display"]["turn180"].isNull() ||
            data["display"]["invertDisplay"].isNull() ||
            data["display"]["ledEnabled"].isNull()) needsRewrite = true;
        display.ecoMode                 = data["display"]["ecoMode"] | false;
        display.timeout                 = data["display"]["timeout"] | 4;
        display.turn180                 = data["display"]["turn180"] | false;
        display.invertDisplay           = data["display"]["invertDisplay"] | false;
        display.ledEnabled              = data["display"]["ledEnabled"] | true;

        if (data["bluetooth"]["active"].isNull() ||
            data["bluetooth"]["deviceName"].isNull()) needsRewrite = true;
        bluetooth.active                = data["bluetooth"]["active"] | false;
        bluetooth.deviceName            = data["bluetooth"]["deviceName"] | "LoRaTracker";

        JsonArray LoraTypesArray = data["lora"];
        for (size_t j = 0; j < LoraTypesArray.size(); j++) {
            LoraType loraType;

            loraType.frequency          = LoraTypesArray[j]["frequency"] | 433775000;
            loraType.spreadingFactor    = LoraTypesArray[j]["spreadingFactor"] | 8;
            loraType.signalBandwidth    = LoraTypesArray[j]["signalBandwidth"] | 62500;
            loraType.codingRate4        = LoraTypesArray[j]["codingRate4"] | 5;
            loraType.power              = LoraTypesArray[j]["power"] | 20;
            loraTypes.push_back(loraType);
        }

        // Enforce single-profile: only ever use index 0
        if (beacons.size() > 1) beacons.resize(1);
        if (beacons.empty())    { Beacon b; b.callsign="NOCALL-7"; b.symbol=">"; b.overlay="/"; b.smartBeaconActive=true; b.smartBeaconSetting=2; beacons.push_back(b); needsRewrite=true; }
        if (loraTypes.size() > 1) loraTypes.resize(1);
        if (loraTypes.empty())    { LoraType l; l.frequency=433775000; l.spreadingFactor=8; l.signalBandwidth=62500; l.codingRate4=5; l.power=20; loraTypes.push_back(l); needsRewrite=true; }

        if (data["battery"]["sendVoltage"].isNull() ||
            data["battery"]["sendVoltageAlways"].isNull() ||
            data["battery"]["sleepVoltage"].isNull()) needsRewrite = true;
        battery.sendVoltage             = data["battery"]["sendVoltage"] | false;
        battery.sendVoltageAlways       = data["battery"]["sendVoltageAlways"] | false;
        battery.sleepVoltage            = data["battery"]["sleepVoltage"] | 2.9;
        #ifdef TTGO_T_BEAM_1W
            // Upgrade a single-cell default stored in flash to the 2S Li-ion
            // cutoff so the pack isn't over-discharged on first boot.
            if (battery.sleepVoltage < 5.0f) battery.sleepVoltage = 6.0f;
        #endif

        if (data["pttTrigger"]["active"].isNull() ||
            data["pttTrigger"]["reverse"].isNull() ||
            data["pttTrigger"]["preDelay"].isNull() ||
            data["pttTrigger"]["postDelay"].isNull() ||
            data["pttTrigger"]["io_pin"].isNull()) needsRewrite = true;
        ptt.active                      = data["pttTrigger"]["active"] | false;
        ptt.reverse                     = data["pttTrigger"]["reverse"] | false;
        ptt.preDelay                    = data["pttTrigger"]["preDelay"] | 0;
        ptt.postDelay                   = data["pttTrigger"]["postDelay"] | 0;
        ptt.io_pin                      = data["pttTrigger"]["io_pin"] | 4;

        if (data["other"]["sendCommentAfterXBeacons"].isNull() ||
            data["other"]["nonSmartBeaconRateSec"].isNull() ||
            data["other"]["sendAltitude"].isNull() ||
            data["other"]["sendSpeedCourse"].isNull() ||
            data["other"]["cadActive"].isNull()) needsRewrite = true;
        sendCommentAfterXBeacons        = data["other"]["sendCommentAfterXBeacons"] | 10;
        // Backward compat: accept old "path" key; new key is "beaconPath"
        if      (!data["other"]["beaconPath"].isNull()) beaconPath = data["other"]["beaconPath"].as<String>();
        else if (!data["other"]["path"].isNull())       beaconPath = data["other"]["path"].as<String>();
        else                                          { beaconPath = "WIDE1-1"; needsRewrite = true; }
        // Backward compat: accept old "nonSmartBeaconRate" (minutes); new key "nonSmartBeaconRateSec" is seconds
        if      (!data["other"]["nonSmartBeaconRateSec"].isNull()) nonSmartBeaconRate = data["other"]["nonSmartBeaconRateSec"] | 900;
        else if (!data["other"]["nonSmartBeaconRate"].isNull())    { nonSmartBeaconRate = (int)(data["other"]["nonSmartBeaconRate"] | 15) * 60; needsRewrite = true; }
        else                                                        { nonSmartBeaconRate = 900; needsRewrite = true; }
        sendAltitude                    = data["other"]["sendAltitude"] | true;
        sendSpeedCourse                 = data["other"]["sendSpeedCourse"] | true;
        cadActive                       = data["other"]["cadActive"] | true;
        // Backward compat: accept old bool "digipeating"; new field is "digiMode" (int)
        if (!data["other"]["digiMode"].isNull()) {
            digiMode = (DigiMode)(data["other"]["digiMode"] | 0);
        } else if (!data["other"]["digipeating"].isNull()) {
            digiMode = (data["other"]["digipeating"] | false) ? DIGI_WIDE1 : DIGI_OFF;
            needsRewrite = true;
        } else {
            digiMode = DIGI_OFF; needsRewrite = true;
        }

        if (data["customSmartBeacon"]["slowRate"].isNull() ||
            data["customSmartBeacon"]["slowSpeed"].isNull() ||
            data["customSmartBeacon"]["fastRate"].isNull() ||
            data["customSmartBeacon"]["fastSpeed"].isNull() ||
            data["customSmartBeacon"]["turnMinDeg"].isNull() ||
            data["customSmartBeacon"]["turnSlope"].isNull()) needsRewrite = true;
        customSmartBeacon.slowRate       = data["customSmartBeacon"]["slowRate"]       | 120;
        customSmartBeacon.slowSpeed      = data["customSmartBeacon"]["slowSpeed"]      | 5;
        customSmartBeacon.fastRate       = data["customSmartBeacon"]["fastRate"]       | 60;
        customSmartBeacon.fastSpeed      = data["customSmartBeacon"]["fastSpeed"]      | 40;
        customSmartBeacon.turnMinDeg     = data["customSmartBeacon"]["turnMinDeg"]     | 12;
        customSmartBeacon.turnSlope      = data["customSmartBeacon"]["turnSlope"]      | 60;
        SMARTBEACON_Utils::setCustomValues(customSmartBeacon);

        wifiSTA.enabled = data["wifiSTA"]["enabled"] | false;
        JsonArray wifiNetworksArray = data["wifiSTA"]["networks"];
        if (!wifiNetworksArray.isNull()) {
            // Current-format config.
            if (data["wifiSTA"]["enabled"].isNull()) needsRewrite = true;
            for (JsonObject netObj : wifiNetworksArray) {
                if (wifiSTA.networks.size() >= MAX_WIFI_NETWORKS) break;
                WiFiNetwork net;
                net.ssid     = netObj["ssid"]     | "";
                net.password = netObj["password"] | "";
                if (net.ssid.length() == 0) continue;   // drop blank entries defensively
                wifiSTA.networks.push_back(net);
            }
        } else if (!data["wifiSTA"]["ssid"].isNull() || !data["wifiSTA"]["password"].isNull()) {
            // Legacy single-SSID format -- migrate ssid/password into networks[0].
            String legacySsid     = data["wifiSTA"]["ssid"]     | "";
            String legacyPassword = data["wifiSTA"]["password"] | "";
            if (legacySsid.length() > 0) {
                WiFiNetwork net;
                net.ssid     = legacySsid;
                net.password = legacyPassword;
                wifiSTA.networks.push_back(net);
            }
            needsRewrite = true;   // rewrite once to drop the legacy keys on disk
        } else {
            needsRewrite = true;   // no wifiSTA section at all -- normalize to current schema
        }

        if (data["deviceRole"].isNull() ||
            data["gpsSource"].isNull()) needsRewrite = true;
        deviceRole                      = (DeviceRole)(data["deviceRole"] | (int)ROLE_TRACKER);
        gpsSource                       = (GPSSource)(data["gpsSource"] | (int)GPS_INTERNAL);

        if (data["fixedPosition"]["latitude"].isNull() ||
            data["fixedPosition"]["longitude"].isNull() ||
            data["fixedPosition"]["elevation"].isNull()) needsRewrite = true;
        fixedPosition.latitude          = data["fixedPosition"]["latitude"] | 0.0;
        fixedPosition.longitude         = data["fixedPosition"]["longitude"] | 0.0;
        fixedPosition.elevation         = data["fixedPosition"]["elevation"] | 0.0;

        if (data["aprsIS"]["server"].isNull() ||
            data["aprsIS"]["port"].isNull() ||
            data["aprsIS"]["passcode"].isNull() ||
            data["aprsIS"]["filter"].isNull() ||
            data["aprsIS"]["downlinkEnabled"].isNull()) needsRewrite = true;
        aprsIS.server                   = data["aprsIS"]["server"] | "rotate.aprs.net";
        aprsIS.port                     = data["aprsIS"]["port"] | 14580;
        aprsIS.passcode                 = data["aprsIS"]["passcode"] | "";
        aprsIS.filter                   = data["aprsIS"]["filter"] | "m/20";
        aprsIS.downlinkEnabled          = data["aprsIS"]["downlinkEnabled"] | true;

        if (data["tcpKISS"]["port"].isNull()) needsRewrite = true;
        tcpKISS.port                    = data["tcpKISS"]["port"] | 8001;

        if (data["phg"]["enabled"].isNull()     ||
            data["phg"]["power"].isNull()        ||
            data["phg"]["height"].isNull()       ||
            data["phg"]["gain"].isNull()         ||
            data["phg"]["directivity"].isNull()  ||
            data["phg"]["beaconRate"].isNull()) needsRewrite = true;
        phg.enabled     = data["phg"]["enabled"]     | false;
        phg.power       = data["phg"]["power"]       | 7;
        phg.height      = data["phg"]["height"]      | 2;
        phg.gain        = data["phg"]["gain"]        | 3;
        phg.directivity = data["phg"]["directivity"] | 0;
        phg.beaconRate  = data["phg"]["beaconRate"]  | 10;

        if (data["remoteCfg"]["enabled"].isNull()          ||
            data["remoteCfg"]["token"].isNull()             ||
            data["remoteCfg"]["unlockWindowSec"].isNull()) needsRewrite = true;
        remoteCfg.enabled          = data["remoteCfg"]["enabled"]          | false;
        remoteCfg.token            = data["remoteCfg"]["token"]            | "";
        remoteCfg.unlockWindowSec  = data["remoteCfg"]["unlockWindowSec"]  | 300;

        configFile.close();

        if (needsRewrite) {
            Serial.println("Config JSON incomplete, rewriting...");
            writeFile();
            delay(1000);
            #ifdef ARDUINO_ARCH_NRF52
                NVIC_SystemReset();
            #else
                ESP.restart();
            #endif
        }
        Serial.println("Config read successfuly");
        return true;
    } else {
        Serial.println("Config file not found");
        return false;
    }
}

void Configuration::setDefaultValues() {
    wifiAP.password                 = "1234567890";

    // ONE beacon
    Beacon beacon;
    beacon.callsign             = "NOCALL-7";
    beacon.symbol               = ">";
    beacon.overlay              = "/";
    beacon.micE                 = "";
    beacon.comment              = "";
    beacon.smartBeaconActive    = true;
    beacon.smartBeaconSetting   = 2;
    beacon.profileLabel         = "";
    beacon.status               = "";
    beacon.tacticalCallsign     = "";
    beacons.clear();
    beacons.push_back(beacon);

    display.ecoMode                 = false;
    display.timeout                 = 4;
    display.turn180                 = false;
    display.invertDisplay           = false;
    display.ledEnabled              = true;

    bluetooth.active                = false;
    bluetooth.deviceName            = "LoRaTracker";

    // ONE LoRa type (433 MHz APRS EU default)
    LoraType loraType;
    loraType.frequency          = 433775000;
    loraType.spreadingFactor    = 8;
    loraType.signalBandwidth    = 62500;
    loraType.codingRate4        = 5;
    loraType.power              = 20;
    loraTypes.clear();
    loraTypes.push_back(loraType);

    battery.sendVoltage             = false;
    battery.sendVoltageAlways       = false;
    #ifdef TTGO_T_BEAM_1W
        battery.sleepVoltage        = 6.0f;  // 2S Li-ion pack (2 × 3.0 V cutoff)
    #else
        battery.sleepVoltage        = 2.9f;
    #endif

    ptt.active                      = false;
    ptt.reverse                     = false;
    ptt.preDelay                    = 0;
    ptt.postDelay                   = 0;
    ptt.io_pin                      = 4;

    sendCommentAfterXBeacons        = 10;
    beaconPath                      = "WIDE1-1";
    nonSmartBeaconRate              = 900;
    sendAltitude                    = true;
    sendSpeedCourse                 = true;
    cadActive                       = true;
    digiMode                        = DIGI_OFF;

    customSmartBeacon               = SmartBeaconValues{ 120, 5, 60, 40, 100, 12 };
    SMARTBEACON_Utils::setCustomValues(customSmartBeacon);

    wifiSTA.enabled                 = false;
    wifiSTA.networks.clear();

    deviceRole                      = ROLE_TRACKER;
    gpsSource                       = GPS_INTERNAL;

    fixedPosition.latitude          = 0.0;
    fixedPosition.longitude         = 0.0;
    fixedPosition.elevation         = 0.0;

    aprsIS.server                   = "rotate.aprs.net";
    aprsIS.port                     = 14580;
    aprsIS.passcode                 = "";
    aprsIS.filter                   = "m/20";
    aprsIS.downlinkEnabled          = true;

    tcpKISS.port                    = 8001;

    phg.enabled     = false;
    phg.power       = 7;
    phg.height      = 2;
    phg.gain        = 3;
    phg.directivity = 0;
    phg.beaconRate  = 10;

    remoteCfg.enabled          = false;
    remoteCfg.token            = "";
    remoteCfg.unlockWindowSec  = 300;

    Serial.println("New Data Created... All is Written!");
}

Configuration::Configuration() {
    #ifdef ARDUINO_ARCH_NRF52
        // SPIFFS / InternalFS access is deferred to Configuration::init() on
        // nRF52 because file-scope globals run their constructors before
        // FreeRTOS is up, and InternalFS.begin() uses xSemaphoreCreateMutex.
        // Populate in-memory defaults so other file-scope globals
        // (myBeaconsSize, currentBeacon, etc.) get sane values; setup() calls
        // init() once FreeRTOS / InternalFS is ready.
        setDefaultValues();
        return;
    #endif

    // ESP32 path — Arduino-ESP32 starts FreeRTOS before static init runs, so
    // it's safe to do filesystem work right here. Keeps merge surface against
    // upstream small (no setup()-side restructuring needed for ESP32).
    if (!SPIFFS.begin(false)) {
        Serial.println("SPIFFS Mount Failed, formatting...");

        if (!SPIFFS.begin(true)) {
            Serial.println("SPIFFS Format Failed");
            return;
        }
    }
    Serial.println("SPIFFS Ready");

    if (!SPIFFS.exists("/tracker_conf.json")) {
        Serial.println("Config not found, creating default...");
        setDefaultValues();
        writeFile();
        delay(500);
        #ifndef ARDUINO_ARCH_NRF52
            ESP.restart();
        #endif
    }

    readFile();
}

#ifdef ARDUINO_ARCH_NRF52
void Configuration::init() {
    if (!SPIFFS.begin(false)) {
        Serial.println("SPIFFS Mount Failed, formatting...");

        if (!SPIFFS.begin(true)) {
            Serial.println("SPIFFS Format Failed");
            return;
        }
    }
    Serial.println("SPIFFS Ready");

    // Defensive recovery: if the on-disk config file is suspiciously large
    // (the healthy size is ~1.5-3 KB), the LittleFS partition is corrupt or
    // bloated by a previous bug (e.g. the FILE_O_WRITE-seeks-to-end append
    // loop we hit during T114 bring-up). Reformat rather than feeding a
    // multi-KB blob to ArduinoJson — a huge file can exhaust heap and hang.
    // Defensive recovery: if the on-disk config file is suspiciously large
    // (healthy size is ~1.5-3 KB), the LittleFS partition is corrupt or
    // bloated. Reformat rather than feeding a multi-KB blob to ArduinoJson.
    if (SPIFFS.exists("/tracker_conf.json")) {
        File check(InternalFS);
        if (check.open("/tracker_conf.json", Adafruit_LittleFS_Namespace::FILE_O_READ)) {
            size_t sz = check.size();
            check.close();
            if (sz > 8192) {
                Serial.print("[init] config file size ");
                Serial.print((unsigned)sz);
                Serial.println(" bytes — too large, reformatting partition");
                InternalFS.format();
                if (!InternalFS.begin()) return;
            }
        }
    }

    if (!SPIFFS.exists("/tracker_conf.json")) {
        // nRF52 has no PlatformIO `uploadfs` equivalent for InternalFS, so
        // first-boot writes the embedded JSON (generated from data/tracker_conf.json
        // by tools/embed_config.py) directly to LittleFS, then reboots so the
        // normal readFile() path picks it up.
        Serial.println("Config not found, writing embedded default JSON...");
        // No need to remove — exists() returned false, file genuinely doesn't exist.
        File f = SPIFFS.open("/tracker_conf.json", "w");
        if (f) {
            f.write((const uint8_t*)DEFAULT_CONFIG_JSON, strlen(DEFAULT_CONFIG_JSON));
            f.close();
        } else {
            Serial.println("Embedded default write failed; falling back to setDefaultValues()");
            setDefaultValues();
            writeFile();
        }
        delay(500);
        NVIC_SystemReset();
    }
    readFile();
}
#endif