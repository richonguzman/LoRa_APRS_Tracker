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

#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "configuration.h"
#include "board_pinout.h"
#include "display.h"
#include "logger.h"

extern logging::Logger logger;

bool Configuration::writeFile() {

    Serial.println("Saving config..");

    StaticJsonDocument<3584> data;
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
        StaticJsonDocument<3584> data;

        DeserializationError error = deserializeJson(data, configFile);
        if (error) {
            Serial.println("Failed to read file, using default configuration");
        }

        if (!data["wifiAP"].containsKey("active") ||
            !data["wifiAP"].containsKey("password")) needsRewrite = true;
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

        if (!data["display"].containsKey("ecoMode") ||
            !data["display"].containsKey("timeout") ||
            !data["display"].containsKey("turn180") ||
            !data["display"].containsKey("showSymbol")) needsRewrite = true;
        display.ecoMode                 = data["display"]["ecoMode"] | false;
        display.timeout                 = data["display"]["timeout"] | 4;
        display.turn180                 = data["display"]["turn180"] | false;
        display.showSymbol              = data["display"]["showSymbol"] | true;

        if (!data["bluetooth"].containsKey("active") ||
            !data["bluetooth"].containsKey("deviceName") ||
            !data["bluetooth"].containsKey("useBLE") ||
            !data["bluetooth"].containsKey("useKISS")) needsRewrite = true;
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

            loraType.frequency          = LoraTypesArray[j]["frequency"] | 433775000;
            loraType.spreadingFactor    = LoraTypesArray[j]["spreadingFactor"] | 12;
            loraType.signalBandwidth    = LoraTypesArray[j]["signalBandwidth"] | 125000;
            loraType.codingRate4        = LoraTypesArray[j]["codingRate4"] | 5;
            loraType.power              = LoraTypesArray[j]["power"] | 20;
            loraTypes.push_back(loraType);
        }

        if (!data["battery"].containsKey("sendVoltage") ||
            !data["battery"].containsKey("voltageAsTelemetry") ||
            !data["battery"].containsKey("sendVoltageAlways") ||
            !data["battery"].containsKey("monitorVoltage") ||
            !data["battery"].containsKey("sleepVoltage")) needsRewrite = true;
        battery.sendVoltage             = data["battery"]["sendVoltage"] | false;
        battery.voltageAsTelemetry      = data["battery"]["voltageAsTelemetry"] | false;
        battery.sendVoltageAlways       = data["battery"]["sendVoltageAlways"] | false;
        battery.monitorVoltage          = data["battery"]["monitorVoltage"] | false;
        battery.sleepVoltage            = data["battery"]["sleepVoltage"] | 2.9;

        if (!data["telemetry"].containsKey("active") ||
            !data["telemetry"].containsKey("sendTelemetry") ||
            !data["telemetry"].containsKey("temperatureCorrection")) needsRewrite = true;
        telemetry.active                = data["telemetry"]["active"] | false;
        telemetry.sendTelemetry         = data["telemetry"]["sendTelemetry"] | false;
        telemetry.temperatureCorrection = data["telemetry"]["temperatureCorrection"] | 0.0;

        if (!data["winlink"].containsKey("password")) needsRewrite = true;
        winlink.password                = data["winlink"]["password"] | "NOPASS";

        if (!data["notification"].containsKey("ledTx") ||
            !data["notification"].containsKey("ledTxPin") ||
            !data["notification"].containsKey("ledMessage") ||
            !data["notification"].containsKey("ledMessagePin") ||
            !data["notification"].containsKey("buzzerActive") ||
            !data["notification"].containsKey("buzzerPinTone") ||
            !data["notification"].containsKey("buzzerPinVcc") ||
            !data["notification"].containsKey("bootUpBeep") ||
            !data["notification"].containsKey("txBeep") ||
            !data["notification"].containsKey("messageRxBeep") ||
            !data["notification"].containsKey("stationBeep") ||
            !data["notification"].containsKey("lowBatteryBeep") ||
            !data["notification"].containsKey("shutDownBeep") ||
            !data["notification"].containsKey("ledFlashlight") ||
            !data["notification"].containsKey("ledFlashlightPin")) needsRewrite = true;
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

        if (!data["pttTrigger"].containsKey("active") ||
            !data["pttTrigger"].containsKey("reverse") ||
            !data["pttTrigger"].containsKey("preDelay") ||
            !data["pttTrigger"].containsKey("postDelay") ||
            !data["pttTrigger"].containsKey("io_pin")) needsRewrite = true;
        ptt.active                      = data["pttTrigger"]["active"] | false;
        ptt.reverse                     = data["pttTrigger"]["reverse"] | false;
        ptt.preDelay                    = data["pttTrigger"]["preDelay"] | 0;
        ptt.postDelay                   = data["pttTrigger"]["postDelay"] | 0;
        ptt.io_pin                      = data["pttTrigger"]["io_pin"] | 4;

        if (!data["other"].containsKey("simplifiedTrackerMode") ||
            !data["other"].containsKey("sendCommentAfterXBeacons") ||
            !data["other"].containsKey("path") ||
            !data["other"].containsKey("nonSmartBeaconRate") ||
            !data["other"].containsKey("rememberStationTime") ||
            !data["other"].containsKey("standingUpdateTime") ||
            !data["other"].containsKey("sendAltitude") ||
            !data["other"].containsKey("disableGPS") ||
            !data["other"].containsKey("email")) needsRewrite = true;
        simplifiedTrackerMode           = data["other"]["simplifiedTrackerMode"] | false;
        sendCommentAfterXBeacons        = data["other"]["sendCommentAfterXBeacons"] | 10;
        path                            = data["other"]["path"] | "WIDE1-1";
        nonSmartBeaconRate              = data["other"]["nonSmartBeaconRate"] | 15;
        rememberStationTime             = data["other"]["rememberStationTime"] | 30;
        standingUpdateTime              = data["other"]["standingUpdateTime"] | 15;
        sendAltitude                    = data["other"]["sendAltitude"] | true;
        disableGPS                      = data["other"]["disableGPS"] | false;
        email                           = data["other"]["email"] | "";

        configFile.close();

        if (needsRewrite) {
            Serial.println("Config JSON incomplete, rewriting...");
            writeFile();
            delay(1000);
            ESP.restart();
        }
        Serial.println("Config read successfuly");
        return true;
    } else {
        Serial.println("Config file not found");
        return false;
    }
}

void Configuration::setDefaultValues() {
    wifiAP.active                   = true;
    wifiAP.password                 = "1234567890";

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
        beacon.profileLabel         = "";
        beacon.status               = "";
        beacons.push_back(beacon);
    }

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

    for (int j = 0; j < 4; j++) {
        LoraType loraType;
        switch (j) {
            case 0:
                loraType.frequency           = 433775000;
                loraType.spreadingFactor     = 12;
                loraType.codingRate4         = 5;
                break;
            case 1:
                loraType.frequency           = 434855000;
                loraType.spreadingFactor     = 9;
                loraType.codingRate4         = 7;
                break;
            case 2:
                loraType.frequency           = 439912500;
                loraType.spreadingFactor     = 12;
                loraType.codingRate4         = 5;
                break;
            case 3:
                loraType.frequency           = 915000000;
                loraType.spreadingFactor     = 12;
                loraType.codingRate4         = 5;
                break;
        }
        loraType.signalBandwidth    = 125000;
        loraType.power              = 20;
        loraTypes.push_back(loraType);
    }

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
    email                           = "";

    Serial.println("New Data Created... All is Written!");
}

Configuration::Configuration() {
    if (!SPIFFS.begin(false)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    } else {
        Serial.println("SPIFFS Mounted");
    }

    bool exists = SPIFFS.exists("/tracker_conf.json");
    if (!exists) {
        setDefaultValues();
        writeFile();
        delay(1000);
        ESP.restart();
    }

    readFile();
}