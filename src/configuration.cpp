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

#include <esp_log.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "configuration.h"
#include "lora_utils.h"
#include "board_pinout.h"
#include "display.h"
static const char *TAG = "Config";

bool Configuration::writeFile() {

    ESP_LOGI(TAG, "Saving config..");

    DynamicJsonDocument data(8192);
    File configFile = SPIFFS.open("/tracker_conf.json", "w");

    if (!configFile) {
        ESP_LOGE(TAG, "Could not open config file for writing");
        return false;
    }
    try {

        for (int i = 0; i < wifiAPs.size(); i++) {
            data["wifi"]["AP"][i]["ssid"]           = wifiAPs[i].ssid;
            data["wifi"]["AP"][i]["password"]       = wifiAPs[i].password;
        }
        data["wifi"]["autoAP"]["active"]            = wifiAutoAP.active;
        data["wifi"]["autoAP"]["password"]          = wifiAutoAP.password;
        data["wifi"]["autoAP"]["timeout"]           = wifiAutoAP.timeout;
        data["wifi"]["enabled"]                     = wifiEnabled;

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

        data["gpsConfig"]["strict3DFix"]            = gpsConfig.strict3DFix;

        data["bluetooth"]["active"]                 = bluetooth.active;
        data["bluetooth"]["deviceName"]             = bluetooth.deviceName;
        #ifdef HAS_BT_CLASSIC
            data["bluetooth"]["useBLE"]             = bluetooth.useBLE;
            data["bluetooth"]["hasBTClassic"]       = true;
        #else
            data["bluetooth"]["useBLE"]             = true; // fixed as BLE
            data["bluetooth"]["hasBTClassic"]       = false;
        #endif
        data["bluetooth"]["useKISS"]                = bluetooth.useKISS;

        data["aprs_is"]["active"]                   = aprs_is.active;
        data["aprs_is"]["server"]                   = aprs_is.server;
        data["aprs_is"]["port"]                     = aprs_is.port;
        data["aprs_is"]["passcode"]                 = aprs_is.passcode;

        for (int i = 0; i < loraTypes.size(); i++) {
            data["lora"][i]["frequency"]                = loraTypes[i].frequency;
            data["lora"][i]["spreadingFactor"]          = loraTypes[i].spreadingFactor;
            data["lora"][i]["signalBandwidth"]          = loraTypes[i].signalBandwidth;
            data["lora"][i]["codingRate4"]              = loraTypes[i].codingRate4;
            data["lora"][i]["power"]                    = loraTypes[i].power;
            data["lora"][i]["dataRate"]                 = loraTypes[i].dataRate;
        }

        // Board-specific frequency limits for web config validation
        #if defined(LORA_FREQ_MIN) && defined(LORA_FREQ_MAX)
            data["loraFreqMin"]                         = LORA_FREQ_MIN;
            data["loraFreqMax"]                         = LORA_FREQ_MAX;
        #else
            data["loraFreqMin"]                         = 100000000;
            data["loraFreqMax"]                         = 1000000000;
        #endif

        data["battery"]["sendVoltage"]              = battery.sendVoltage;
        data["battery"]["voltageAsTelemetry"]       = battery.voltageAsTelemetry;
        data["battery"]["sendVoltageAlways"]        = battery.sendVoltageAlways;
        data["battery"]["monitorVoltage"]           = battery.monitorVoltage;
        data["battery"]["sleepVoltage"]             = battery.sleepVoltage;

        data["loraConfig"]["sendInfo"]              = lora.sendInfo;
        data["loraConfig"]["repeaterMode"]          = lora.repeaterMode;

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
        data["notification"]["volume"]              = notification.volume;
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

        if (data.overflowed()) {
            ESP_LOGE(TAG, "JSON buffer overflow! Config truncated.");
            configFile.close();
            return false;
        }

        serializeJson(data, configFile);
        configFile.close();
        return true;
    } catch (...) {
        ESP_LOGE(TAG, "Exception occurred while saving config");
        configFile.close();
        return false;
    }
}

bool Configuration::readFile() {
    ESP_LOGI(TAG, "Reading config..");
    File configFile = SPIFFS.open("/tracker_conf.json", "r");

    if (configFile) {
        bool needsRewrite = false;
        DynamicJsonDocument data(8192);

        DeserializationError error = deserializeJson(data, configFile);
        if (error) {
            ESP_LOGW(TAG, "Failed to read file, using default configuration");
        }

        JsonArray WifiAPArray = data["wifi"]["AP"];
        for (int i = 0; i < WifiAPArray.size(); i++) {
            WiFi_AP wifiap;
            wifiap.ssid             = WifiAPArray[i]["ssid"] | "";
            wifiap.password         = WifiAPArray[i]["password"] | "";
            wifiAPs.push_back(wifiap);
        }
        wifiAutoAP.active           = data["wifi"]["autoAP"]["active"] | false;
        wifiAutoAP.password         = data["wifi"]["autoAP"]["password"] | "1234567890";
        wifiAutoAP.timeout          = data["wifi"]["autoAP"]["timeout"] | 10;
        wifiEnabled                 = data["wifi"]["enabled"] | true;  // Default to enabled

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

        if (!data["gpsConfig"].containsKey("strict3DFix")) needsRewrite = true;
        gpsConfig.strict3DFix           = data["gpsConfig"]["strict3DFix"] | false;

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

        if (!data["aprs_is"].containsKey("active") ||
            !data["aprs_is"].containsKey("server") ||
            !data["aprs_is"].containsKey("port") ||
            !data["aprs_is"].containsKey("passcode")) needsRewrite = true;
        aprs_is.active                  = data["aprs_is"]["active"] | false;
        aprs_is.server                  = data["aprs_is"]["server"] | "euro.aprs2.net";
        aprs_is.port                    = data["aprs_is"]["port"] | 14580;
        aprs_is.passcode                = data["aprs_is"]["passcode"] | "-1";

        JsonArray LoraTypesArray = data["lora"];
        for (int j = 0; j < LoraTypesArray.size(); j++) {
            LoraType loraType;

            loraType.frequency          = LoraTypesArray[j]["frequency"] | 433775000;
            loraType.spreadingFactor    = LoraTypesArray[j]["spreadingFactor"] | 12;
            loraType.signalBandwidth    = LoraTypesArray[j]["signalBandwidth"] | 125000;
            loraType.codingRate4        = LoraTypesArray[j]["codingRate4"] | 5;
            loraType.power              = LoraTypesArray[j]["power"] | 20;

            loraType.dataRate = LoRa_Utils::calculateDataRate(loraType.spreadingFactor, loraType.codingRate4, loraType.signalBandwidth);

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

        if (!data["loraConfig"].containsKey("sendInfo")) needsRewrite = true;
        lora.sendInfo                   = data["loraConfig"]["sendInfo"] | true;
        if (!data["loraConfig"].containsKey("repeaterMode")) needsRewrite = true;
        lora.repeaterMode               = data["loraConfig"]["repeaterMode"] | false;

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
            !data["notification"].containsKey("volume") ||
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
        notification.volume             = data["notification"]["volume"] | 50;
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

        // Add defaults if vectors are empty (old config format)
        if (wifiAPs.size() == 0) {
            WiFi_AP defaultWifi;
            defaultWifi.ssid = "";
            defaultWifi.password = "";
            wifiAPs.push_back(defaultWifi);
            needsRewrite = true;
            ESP_LOGI(TAG, "Added default WiFi AP entry");
        }
        if (beacons.size() == 0) {
            Beacon bcn;
            bcn.callsign = "NOCALL-7";
            bcn.symbol = "[";
            bcn.overlay = "/";
            bcn.smartBeaconActive = true;
            bcn.smartBeaconSetting = 0;
            bcn.gpsEcoMode = false;
            beacons.push_back(bcn);
            needsRewrite = true;
            ESP_LOGI(TAG, "Added default beacon entry");
        }
        if (loraTypes.size() == 0) {
            LoraType loraType;
            loraType.frequency = 433775000;
            loraType.spreadingFactor = 12;
            loraType.signalBandwidth = 125000;
            loraType.codingRate4 = 5;
            loraType.power = 20;
            loraType.dataRate = 300;
            loraTypes.push_back(loraType);
            needsRewrite = true;
            ESP_LOGI(TAG, "Added default LoRa entry");
        }

        if (needsRewrite) {
            ESP_LOGW(TAG, "Config JSON incomplete, rewriting...");
            writeFile();
            delay(1000);
            ESP.restart();
        }
        ESP_LOGI(TAG, "Config read successfully");
        return true;
    } else {
        ESP_LOGW(TAG, "Config file not found");
        return false;
    }
}

void Configuration::setDefaultValues() {
    WiFi_AP defaultWifi;
    defaultWifi.ssid                = "";
    defaultWifi.password            = "";
    wifiAPs.push_back(defaultWifi);
    wifiAutoAP.active               = false;
    wifiAutoAP.password             = "1234567890";
    wifiAutoAP.timeout              = 10;
    wifiEnabled                     = true;  // WiFi enabled by default

    {
        Beacon b1;
        b1.callsign = "NOCALL-7"; b1.symbol = "["; b1.overlay = "/";
        b1.smartBeaconActive = true; b1.smartBeaconSetting = 0; b1.gpsEcoMode = false;
        beacons.push_back(b1);

        Beacon b2;
        b2.callsign = "NOCALL-8"; b2.symbol = "<"; b2.overlay = "/";
        b2.smartBeaconActive = true; b2.smartBeaconSetting = 1; b2.gpsEcoMode = false;
        beacons.push_back(b2);

        Beacon b3;
        b3.callsign = "NOCALL-9"; b3.symbol = ">"; b3.overlay = "/";
        b3.smartBeaconActive = true; b3.smartBeaconSetting = 2; b3.gpsEcoMode = false;
        beacons.push_back(b3);
    }

    display.ecoMode                 = false;
    display.timeout                 = 4;
    display.turn180                 = false;
    display.showSymbol              = true;

    gpsConfig.strict3DFix           = false;

    bluetooth.active                = false;
    bluetooth.deviceName            = "LoRaTracker";
    #ifdef HAS_BT_CLASSIC
        bluetooth.useBLE            = false;
        bluetooth.useKISS           = false;
    #else
        bluetooth.useBLE            = true;    // fixed as BLE
        bluetooth.useKISS           = true;
    #endif

    aprs_is.active                  = false;
    aprs_is.server                  = "euro.aprs2.net";
    aprs_is.port                    = 14580;
    aprs_is.passcode                = "-1";

    auto addLoraType = [&](long freq, int sf, int cr4, int dataRate) {
        LoraType lt;
        lt.frequency        = freq;
        lt.spreadingFactor  = sf;
        lt.codingRate4      = cr4;
        lt.signalBandwidth  = 125000;
        lt.power            = 20;
        lt.dataRate         = dataRate;
        loraTypes.push_back(lt);
    };

    #if defined(LORA_FREQ_MIN) && LORA_FREQ_MIN < 500000000
        // 433 MHz boards: EU, PL, UK
        addLoraType(433775000, 12, 5, 300);   // EU  — SF12 CR4:5
        addLoraType(434855000,  9, 7, 1200);  // PL  — SF9  CR4:7
        addLoraType(439912500, 12, 5, 300);   // UK  — SF12 CR4:5
    #elif defined(LORA_FREQ_MIN) && LORA_FREQ_MIN >= 800000000
        // 868/915 MHz boards: EU868, US915
        addLoraType(868200000, 12, 5, 300);   // EU868
        addLoraType(915000000, 12, 5, 300);   // US915
    #else
        // Fallback: EU 433
        addLoraType(433775000, 12, 5, 300);
    #endif

    battery.sendVoltage             = false;
    battery.voltageAsTelemetry      = false;
    battery.sendVoltageAlways       = false;
    battery.monitorVoltage          = false;
    battery.sleepVoltage            = 2.9;

    lora.sendInfo                   = true;
    lora.repeaterMode               = false;

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
    notification.volume             = 50;
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
    standingUpdateTime              = 30;  // 30 minutes between beacons when stationary
    sendAltitude                    = true;
    disableGPS                      = false;
    email                           = "";

    ESP_LOGI(TAG, "New Data Created... All is Written!");
}

Configuration::Configuration() {
    // No SPIFFS access here — global constructor runs before FreeRTOS.
    // Just load defaults. init() will be called from setup() to load from SPIFFS.
    setDefaultValues();
}

void Configuration::init() {
    // Called from setup() after STORAGE_Utils::setup() has mounted/formatted SPIFFS.
    if (SPIFFS.exists("/tracker_conf.json")) {
        beacons.clear();
        wifiAPs.clear();
        loraTypes.clear();
        readFile();
    } else {
        writeFile();
    }
}
