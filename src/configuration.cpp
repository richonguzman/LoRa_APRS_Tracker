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
#include <SPIFFS.h>
#include "configuration.h"
#include "board_pinout.h"
#include "display.h"
#include "logger.h"
#include "lora_utils.h"

extern logging::Logger logger;

namespace {

    const size_t NAME_MAX_LENGTH = 8;

    bool nameTaken(const String& name, const std::vector<String>& taken) {
        for (size_t i = 0; i < taken.size(); i++) {
            if (taken[i].equalsIgnoreCase(name)) return true;
        }
        return false;
    }

    // A usable name is trimmed, at most 8 characters, not empty and not used before in the same list.
    // Otherwise it falls back to PREFIX1, PREFIX2, ... so every entry always has a distinct name.
    String cleanName(String name, const char* prefix, size_t index, const std::vector<String>& taken) {
        name.trim();
        if (name.length() > NAME_MAX_LENGTH) {
            name = name.substring(0, NAME_MAX_LENGTH);
            name.trim();
        }
        if (name.length() > 0 && !nameTaken(name, taken)) return name;

        for (size_t n = index + 1; ; n++) {
            String candidate = String(prefix) + String((unsigned long)n);
            if (candidate.length() > NAME_MAX_LENGTH) candidate = String("N") + String((unsigned long)n);
            if (!nameTaken(candidate, taken)) return candidate;
        }
    }

    void addDefaultBeacons(std::vector<Beacon>& list) {
        for (int i = 0; i < 3; i++) {
            Beacon beacon;
            beacon.callsign             = "NOCALL-7";
            beacon.symbol               = "[";
            beacon.overlay              = "/";
            beacon.micE                 = "";
            beacon.comment              = "";
            beacon.smartBeaconActive    = true;
            beacon.smartBeaconSetting   = 0;
            beacon.gpsEcoMode           = false;
            beacon.profileLabel         = String("PROFILE") + String(i + 1);
            beacon.status               = "";
            list.push_back(beacon);
        }
    }

    // Default entries for a device with no config file. The first entry is the one a new device starts on.
    // Default entries for a device with no config file: the usual LoRa APRS settings of the regions.
    // The first entry is the one a new device starts on. Power is the most the board can do.
    void addDefaultLoraTypes(std::vector<LoraType>& list) {
        struct Preset { const char* name; long frequency; int spreadingFactor; int codingRate4; };
        const Preset presets[4] = {
            { "EU/WORLD", 433775000, 12, 5 },
            { "POLAND",   434855000,  9, 7 },
            { "UK",       439912500, 12, 5 },
            { "US",       433775000,  7, 6 }
        };
        for (int j = 0; j < 4; j++) {
            LoraType loraType;
            loraType.name               = presets[j].name;
            loraType.frequency          = presets[j].frequency;
            loraType.spreadingFactor    = presets[j].spreadingFactor;
            loraType.codingRate4        = presets[j].codingRate4;
            loraType.signalBandwidth    = 125000;
            loraType.power              = LoRa_Utils::boardMaxPower();
            list.push_back(loraType);
        }
    }
}

// Trims, shortens to 8 characters and de-duplicates the names shown on screen and in the web page.
// Returns true when anything had to change.
bool Configuration::normalizeNames() {
    bool changed = false;

    std::vector<String> taken;
    for (size_t i = 0; i < beacons.size(); i++) {
        String clean = cleanName(beacons[i].profileLabel, "PROFILE", i, taken);
        if (clean != beacons[i].profileLabel) changed = true;
        beacons[i].profileLabel = clean;
        taken.push_back(clean);
    }

    taken.clear();
    for (size_t j = 0; j < loraTypes.size(); j++) {
        String clean = cleanName(loraTypes[j].name, "LORA", j, taken);
        if (clean != loraTypes[j].name) changed = true;
        loraTypes[j].name = clean;
        taken.push_back(clean);
    }
    return changed;
}

bool Configuration::writeFile() {

    Serial.println("Saving config..");
    normalizeNames();

    JsonDocument data;
    File configFile = SPIFFS.open("/tracker_conf.json", "w");

    if (!configFile) {
        Serial.println("Error: Could not open config file for writing");
        return false;
    }
    try {

        data["wifiAP"]["active"]                    = wifiAP.active;
        data["wifiAP"]["password"]                  = wifiAP.password;

        for (int i = 0; i < beacons.size(); i++) {
            beacons[i].callsign.trim();
            beacons[i].callsign.toUpperCase();
            data["beacons"][i]["callsign"]              = beacons[i].callsign;
            data["beacons"][i]["symbol"]                = beacons[i].symbol;
            data["beacons"][i]["overlay"]               = beacons[i].overlay;
            data["beacons"][i]["micE"]                  = beacons[i].micE;
            data["beacons"][i]["comment"]               = beacons[i].comment;
            data["beacons"][i]["smartBeaconActive"]     = beacons[i].smartBeaconActive;
            data["beacons"][i]["smartBeaconSetting"]    = beacons[i].smartBeaconSetting;
            data["beacons"][i]["gpsEcoMode"]            = beacons[i].gpsEcoMode;
            data["beacons"][i]["profileLabel"]          = beacons[i].profileLabel;
            data["beacons"][i]["status"]                = beacons[i].status;
        }

        data["display"]["ecoMode"]                  = display.ecoMode;
        data["display"]["timeout"]                  = display.timeout;
        data["display"]["turn180"]                  = display.turn180;
        data["display"]["showSymbol"]               = display.showSymbol;

        data["bluetooth"]["active"]                 = bluetooth.active;
        data["bluetooth"]["deviceName"]             = bluetooth.deviceName;
        #ifdef HAS_BT_CLASSIC
            data["bluetooth"]["useBLE"]             = bluetooth.useBLE;
        #else
            data["bluetooth"]["useBLE"]             = true; // fixed as BLE
        #endif
        data["bluetooth"]["useKISS"]                = bluetooth.useKISS;

        for (int i = 0; i < loraTypes.size(); i++) {
            data["lora"][i]["name"]                     = loraTypes[i].name;
            data["lora"][i]["frequency"]                = loraTypes[i].frequency;
            data["lora"][i]["spreadingFactor"]          = loraTypes[i].spreadingFactor;
            data["lora"][i]["signalBandwidth"]          = loraTypes[i].signalBandwidth;
            data["lora"][i]["codingRate4"]              = loraTypes[i].codingRate4;
            data["lora"][i]["power"]                    = loraTypes[i].power;
        }

        data["battery"]["sendVoltage"]              = battery.sendVoltage;
        data["battery"]["voltageAsTelemetry"]       = battery.voltageAsTelemetry;
        data["battery"]["sendVoltageAlways"]        = battery.sendVoltageAlways;
        data["battery"]["monitorVoltage"]           = battery.monitorVoltage;
        data["battery"]["sleepVoltage"]             = battery.sleepVoltage;

        data["telemetry"]["active"]                 = telemetry.active;
        data["telemetry"]["sendTelemetry"]          = telemetry.sendTelemetry;
        data["telemetry"]["temperatureCorrection"]  = telemetry.temperatureCorrection;

        data["winlink"]["password"]                 = winlink.password;

        data["notification"]["ledTx"]               = notification.ledTx;
        data["notification"]["ledTxPin"]            = notification.ledTxPin;
        data["notification"]["ledMessage"]          = notification.ledMessage;
        data["notification"]["ledMessagePin"]       = notification.ledMessagePin;
        data["notification"]["buzzerActive"]        = notification.buzzerActive;
        data["notification"]["buzzerPinTone"]       = notification.buzzerPinTone;
        data["notification"]["buzzerPinVcc"]        = notification.buzzerPinVcc;
        data["notification"]["bootUpBeep"]          = notification.bootUpBeep;
        data["notification"]["txBeep"]              = notification.txBeep;
        data["notification"]["messageRxBeep"]       = notification.messageRxBeep;
        data["notification"]["stationBeep"]         = notification.stationBeep;
        data["notification"]["lowBatteryBeep"]      = notification.lowBatteryBeep;
        data["notification"]["shutDownBeep"]        = notification.shutDownBeep;
        data["notification"]["ledFlashlight"]       = notification.ledFlashlight;
        data["notification"]["ledFlashlightPin"]    = notification.ledFlashlightPin;

        data["pttTrigger"]["active"]                = ptt.active;
        data["pttTrigger"]["reverse"]               = ptt.reverse;
        data["pttTrigger"]["preDelay"]              = ptt.preDelay;
        data["pttTrigger"]["postDelay"]             = ptt.postDelay;
        data["pttTrigger"]["io_pin"]                = ptt.io_pin;

        data["other"]["simplifiedTrackerMode"]      = simplifiedTrackerMode;
        data["other"]["sendCommentAfterXBeacons"]   = sendCommentAfterXBeacons;
        data["other"]["path"]                       = path;
        data["other"]["nonSmartBeaconRate"]         = nonSmartBeaconRate;
        data["other"]["rememberStationTime"]        = rememberStationTime;
        data["other"]["standingUpdateTime"]         = standingUpdateTime;
        data["other"]["sendAltitude"]               = sendAltitude;
        data["other"]["disableGPS"]                 = disableGPS;
        data["other"]["cadActive"]                  = cadActive;
        data["other"]["email"]                      = email;

        serializeJson(data, configFile);
        configFile.close();
        return true;
    } catch (...) {
        Serial.println("Error: Exception occurred while saving config");
        configFile.close();
        return false;
    }
}

bool Configuration::readFile() {
    Serial.println("Reading config..");
    File configFile = SPIFFS.open("/tracker_conf.json", "r");

    if (configFile) {
        bool needsRewrite = false;
        JsonDocument data;
        DeserializationError error = deserializeJson(data, configFile);
        if (error) {
            Serial.println("Failed to read file, using default configuration");
        }

        if (data["wifiAP"]["active"].isNull() ||
            data["wifiAP"]["password"].isNull()) needsRewrite = true;
        wifiAP.active               = data["wifiAP"]["active"] | true;
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
            bcn.gpsEcoMode              = BeaconsArray[i]["gpsEcoMode"] | false;
            bcn.profileLabel            = BeaconsArray[i]["profileLabel"] | "";
            beacons.push_back(bcn);
        }

        if (data["display"]["ecoMode"].isNull() ||
            data["display"]["timeout"].isNull() ||
            data["display"]["turn180"].isNull() ||
            data["display"]["showSymbol"].isNull()) needsRewrite = true;
        display.ecoMode                 = data["display"]["ecoMode"] | false;
        display.timeout                 = data["display"]["timeout"] | 4;
        display.turn180                 = data["display"]["turn180"] | false;
        display.showSymbol              = data["display"]["showSymbol"] | true;

        if (data["bluetooth"]["active"].isNull() ||
            data["bluetooth"]["deviceName"].isNull() ||
            data["bluetooth"]["useBLE"].isNull() ||
            data["bluetooth"]["useKISS"].isNull()) needsRewrite = true;
        bluetooth.active                = data["bluetooth"]["active"] | false;
        bluetooth.deviceName            = data["bluetooth"]["deviceName"] | "LoRaTracker";
        #ifdef HAS_BT_CLASSIC
            bluetooth.useBLE            = data["bluetooth"]["useBLE"] | false;
            bluetooth.useKISS           = data["bluetooth"]["useKISS"] | false;
        #else
            bluetooth.useBLE            = true;    // fixed as BLE
            bluetooth.useKISS           = data["bluetooth"]["useKISS"] | true;    // true=KISS,  false=TNC2
        #endif

        JsonArray LoraTypesArray = data["lora"];
        for (int j = 0; j < LoraTypesArray.size(); j++) {
            LoraType loraType;

            loraType.name               = LoraTypesArray[j]["name"] | "";
            loraType.frequency          = LoraTypesArray[j]["frequency"] | 433775000;
            loraType.spreadingFactor    = LoraTypesArray[j]["spreadingFactor"] | 12;
            loraType.signalBandwidth    = LoraTypesArray[j]["signalBandwidth"] | 125000;
            loraType.codingRate4        = LoraTypesArray[j]["codingRate4"] | 5;
            loraType.power              = LoraTypesArray[j]["power"] | 20;
            loraTypes.push_back(loraType);
        }

        if (beacons.empty()) {      // damaged or incomplete file: the profile list must never be empty
            addDefaultBeacons(beacons);
            needsRewrite = true;
        }
        if (loraTypes.empty()) {    // same for LoRa entries
            addDefaultLoraTypes(loraTypes);
            needsRewrite = true;
        }
        if (normalizeNames()) needsRewrite = true;

        if (data["battery"]["sendVoltage"].isNull() ||
            data["battery"]["voltageAsTelemetry"].isNull() ||
            data["battery"]["sendVoltageAlways"].isNull() ||
            data["battery"]["monitorVoltage"].isNull() ||
            data["battery"]["sleepVoltage"].isNull()) needsRewrite = true;
        battery.sendVoltage             = data["battery"]["sendVoltage"] | false;
        battery.voltageAsTelemetry      = data["battery"]["voltageAsTelemetry"] | false;
        battery.sendVoltageAlways       = data["battery"]["sendVoltageAlways"] | false;
        battery.monitorVoltage          = data["battery"]["monitorVoltage"] | false;
        battery.sleepVoltage            = data["battery"]["sleepVoltage"] | 2.9;

        if (data["telemetry"]["active"].isNull() ||
            data["telemetry"]["sendTelemetry"].isNull() ||
            data["telemetry"]["temperatureCorrection"].isNull()) needsRewrite = true;
        telemetry.active                = data["telemetry"]["active"] | false;
        telemetry.sendTelemetry         = data["telemetry"]["sendTelemetry"] | false;
        telemetry.temperatureCorrection = data["telemetry"]["temperatureCorrection"] | 0.0;

        if (data["winlink"]["password"].isNull()) needsRewrite = true;
        winlink.password                = data["winlink"]["password"] | "NOPASS";

        if (data["notification"]["ledTx"].isNull() ||
            data["notification"]["ledTxPin"].isNull() ||
            data["notification"]["ledMessage"].isNull() ||
            data["notification"]["ledMessagePin"].isNull() ||
            data["notification"]["buzzerActive"].isNull() ||
            data["notification"]["buzzerPinTone"].isNull() ||
            data["notification"]["buzzerPinVcc"].isNull() ||
            data["notification"]["bootUpBeep"].isNull() ||
            data["notification"]["txBeep"].isNull() ||
            data["notification"]["messageRxBeep"].isNull() ||
            data["notification"]["stationBeep"].isNull() ||
            data["notification"]["lowBatteryBeep"].isNull() ||
            data["notification"]["shutDownBeep"].isNull() ||
            data["notification"]["ledFlashlight"].isNull() ||
            data["notification"]["ledFlashlightPin"].isNull()) needsRewrite = true;
        notification.ledTx              = data["notification"]["ledTx"] | false;
        notification.ledTxPin           = data["notification"]["ledTxPin"]| 13;
        notification.ledMessage         = data["notification"]["ledMessage"] | false;
        notification.ledMessagePin      = data["notification"]["ledMessagePin"] | 2;
        notification.buzzerActive       = data["notification"]["buzzerActive"] | false;
        notification.buzzerPinTone      = data["notification"]["buzzerPinTone"] | 33;
        notification.buzzerPinVcc       = data["notification"]["buzzerPinVcc"] | 25;
        notification.bootUpBeep         = data["notification"]["bootUpBeep"] | false;
        notification.txBeep             = data["notification"]["txBeep"] | false;
        notification.messageRxBeep      = data["notification"]["messageRxBeep"] | false;
        notification.stationBeep        = data["notification"]["stationBeep"] | false;
        notification.lowBatteryBeep     = data["notification"]["lowBatteryBeep"] | false;
        notification.shutDownBeep       = data["notification"]["shutDownBeep"] | false;
        notification.ledFlashlight      = data["notification"]["ledFlashlight"] | false;
        notification.ledFlashlightPin   = data["notification"]["ledFlashlightPin"] | 14;

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

        if (data["other"]["simplifiedTrackerMode"].isNull() ||
            data["other"]["sendCommentAfterXBeacons"].isNull() ||
            data["other"]["path"].isNull() ||
            data["other"]["nonSmartBeaconRate"].isNull() ||
            data["other"]["rememberStationTime"].isNull() ||
            data["other"]["standingUpdateTime"].isNull() ||
            data["other"]["sendAltitude"].isNull() ||
            data["other"]["disableGPS"].isNull() ||
            data["other"]["cadActive"].isNull() ||
            data["other"]["email"].isNull()) needsRewrite = true;
        simplifiedTrackerMode           = data["other"]["simplifiedTrackerMode"] | false;
        sendCommentAfterXBeacons        = data["other"]["sendCommentAfterXBeacons"] | 10;
        path                            = data["other"]["path"] | "WIDE1-1";
        nonSmartBeaconRate              = data["other"]["nonSmartBeaconRate"] | 15;
        rememberStationTime             = data["other"]["rememberStationTime"] | 30;
        standingUpdateTime              = data["other"]["standingUpdateTime"] | 15;
        sendAltitude                    = data["other"]["sendAltitude"] | true;
        disableGPS                      = data["other"]["disableGPS"] | false;
        cadActive                       = data["other"]["cadActive"] | true;
        email                           = data["other"]["email"] | "";

        configFile.close();

        if (needsRewrite) {
            Serial.println("Config JSON incomplete, rewriting...");
            if (writeFile()) {
                delay(1000);
                ESP.restart();
            } else {
                Serial.println("Could not save the rewritten config, continuing with the values in memory");
            }
        }        Serial.println("Config read successfully");
        return true;
    } else {
        Serial.println("Config file not found");
        return false;
    }
}

void Configuration::setDefaultValues() {
    wifiAP.active                   = true;
    wifiAP.password                 = "1234567890";

    addDefaultBeacons(beacons);

    display.ecoMode                 = false;
    display.timeout                 = 4;
    display.turn180                 = false;
    display.showSymbol              = true;

    bluetooth.active                = false;
    bluetooth.deviceName            = "LoRaTracker";
    #ifdef HAS_BT_CLASSIC
        bluetooth.useBLE            = false;
        bluetooth.useKISS           = false;
    #else
        bluetooth.useBLE            = true;    // fixed as BLE
        bluetooth.useKISS           = true;
    #endif

    addDefaultLoraTypes(loraTypes);

    battery.sendVoltage             = false;
    battery.voltageAsTelemetry      = false;
    battery.sendVoltageAlways       = false;
    battery.monitorVoltage          = false;
    battery.sleepVoltage            = 2.9;

    telemetry.active                 = false;
    telemetry.sendTelemetry          = false;
    telemetry.temperatureCorrection  = 0.0;

    winlink.password                = "NOPASS";

    notification.ledTx              = false;
    notification.ledTxPin           = 13;
    notification.ledMessage         = false;
    notification.ledMessagePin      = 2;
    notification.buzzerActive       = false;
    notification.buzzerPinTone      = 33;
    notification.buzzerPinVcc       = 25;
    notification.bootUpBeep         = false;
    notification.txBeep             = false;
    notification.messageRxBeep      = false;
    notification.stationBeep        = false;
    notification.lowBatteryBeep     = false;
    notification.shutDownBeep       = false;
    notification.ledFlashlight      = false;
    notification.ledFlashlightPin   = 14;

    ptt.active                      = false;
    ptt.reverse                     = false;
    ptt.preDelay                    = 0;
    ptt.postDelay                   = 0;
    ptt.io_pin                      = 4;

    simplifiedTrackerMode           = false;
    sendCommentAfterXBeacons        = 10;
    path                            = "WIDE1-1";
    nonSmartBeaconRate              = 15;
    rememberStationTime             = 30;
    standingUpdateTime              = 15;
    sendAltitude                    = true;
    disableGPS                      = false;
    cadActive                       = true;
    email                           = "";

    Serial.println("New Data Created... All is Written!");
}

Configuration::Configuration() {
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
        ESP.restart();
    }

    readFile();
}