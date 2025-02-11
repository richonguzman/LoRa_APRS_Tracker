#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "configuration.h"
#include "board_pinout.h"
#include "display.h"
#include "logger.h"

extern logging::Logger logger;


void Configuration::writeFile() {

    Serial.println("Saving config..");

    StaticJsonDocument<2800> data;
    File configFile = SPIFFS.open("/tracker_conf.json", "w");

    data["wifiAP"]["active"]                    = wifiAP.active;
    data["wifiAP"]["password"]                  = wifiAP.password;

    for (int i = 0; i < beacons.size(); i++) {
        data["beacons"][i]["callsign"]              = beacons[i].callsign;
        data["beacons"][i]["symbol"]                = beacons[i].symbol;
        data["beacons"][i]["overlay"]               = beacons[i].overlay;
        data["beacons"][i]["comment"]               = beacons[i].comment;
        data["beacons"][i]["smartBeaconActive"]     = beacons[i].smartBeaconActive;
        data["beacons"][i]["smartBeaconSetting"]    = beacons[i].smartBeaconSetting;
        data["beacons"][i]["micE"]                  = beacons[i].micE;
        data["beacons"][i]["gpsEcoMode"]            = beacons[i].gpsEcoMode;
    }

    data["display"]["showSymbol"]               = display.showSymbol;
    data["display"]["ecoMode"]                  = display.ecoMode;
    data["display"]["timeout"]                  = display.timeout;
    data["display"]["turn180"]                  = display.turn180;

    data["battery"]["sendVoltage"]              = battery.sendVoltage;
    data["battery"]["voltageAsTelemetry"]       = battery.voltageAsTelemetry;
    data["battery"]["sendVoltageAlways"]        = battery.sendVoltageAlways;
    data["battery"]["monitorVoltage"]           = battery.monitorVoltage;
    data["battery"]["sleepVoltage"]             = battery.sleepVoltage;

    data["winlink"]["password"]                 = winlink.password;

    data["wxsensor"]["active"]                  = wxsensor.active;
    data["wxsensor"]["temperatureCorrection"]   = wxsensor.temperatureCorrection;
    data["wxsensor"]["sendTelemetry"]           = wxsensor.sendTelemetry;

    data["notification"]["ledTx"]               = notification.ledTx;
    data["notification"]["ledTxPin"]            = notification.ledTxPin;
    data["notification"]["ledMessage"]          = notification.ledMessage;
    data["notification"]["ledMessagePin"]       = notification.ledMessagePin;
    data["notification"]["ledFlashlight"]       = notification.ledFlashlight;
    data["notification"]["ledFlashlightPin"]    = notification.ledFlashlightPin;
    data["notification"]["buzzerActive"]        = notification.buzzerActive;
    data["notification"]["buzzerPinTone"]       = notification.buzzerPinTone;
    data["notification"]["buzzerPinVcc"]        = notification.buzzerPinVcc;
    data["notification"]["bootUpBeep"]          = notification.bootUpBeep;
    data["notification"]["txBeep"]              = notification.txBeep;
    data["notification"]["messageRxBeep"]       = notification.messageRxBeep;
    data["notification"]["stationBeep"]         = notification.stationBeep;
    data["notification"]["lowBatteryBeep"]      = notification.lowBatteryBeep;
    data["notification"]["shutDownBeep"]        = notification.shutDownBeep;
    
    for (int i = 0; i < loraTypes.size(); i++) {
        data["lora"][i]["frequency"]                = loraTypes[i].frequency;
        data["lora"][i]["spreadingFactor"]          = loraTypes[i].spreadingFactor;
        data["lora"][i]["signalBandwidth"]          = loraTypes[i].signalBandwidth;
        data["lora"][i]["codingRate4"]              = loraTypes[i].codingRate4;
        data["lora"][i]["power"]                    = loraTypes[i].power;
    }

    data["pttTrigger"]["active"]                = ptt.active;
    data["pttTrigger"]["io_pin"]                = ptt.io_pin;
    data["pttTrigger"]["preDelay"]              = ptt.preDelay;
    data["pttTrigger"]["postDelay"]             = ptt.postDelay;
    data["pttTrigger"]["reverse"]               = ptt.reverse;

    data["bluetooth"]["active"]                 = bluetooth.active;
    data["bluetooth"]["deviceName"]             = bluetooth.deviceName;
    #ifdef HAS_BT_CLASSIC
        data["bluetooth"]["useBLE"]             = bluetooth.useBLE;
    #else
        data["bluetooth"]["useBLE"]             = true; // fixed as BLE        
    #endif
    data["bluetooth"]["useKISS"]                = bluetooth.useKISS;

    data["other"]["simplifiedTrackerMode"]      = simplifiedTrackerMode;
    data["other"]["sendCommentAfterXBeacons"]   = sendCommentAfterXBeacons;
    data["other"]["path"]                       = path;
    data["other"]["nonSmartBeaconRate"]         = nonSmartBeaconRate;
    data["other"]["rememberStationTime"]        = rememberStationTime;
    data["other"]["standingUpdateTime"]         = standingUpdateTime;
    data["other"]["sendAltitude"]               = sendAltitude;
    data["other"]["disableGPS"]                 = disableGPS;
    data["other"]["acceptOwnFrameFromTNC"]      = acceptOwnFrameFromTNC;
    data["other"]["email"]                      = email;


    serializeJson(data, configFile);
    configFile.close();
    Serial.println("Config saved");
}

bool Configuration::readFile() {
    Serial.println("Reading config..");
    File configFile = SPIFFS.open("/tracker_conf.json", "r");

    if (configFile) {
        StaticJsonDocument<2800> data;
        DeserializationError error = deserializeJson(data, configFile);
        if (error) {
            Serial.println("Failed to read file, using default configuration");
        }

        wifiAP.active               = data["wifiAP"]["active"] | true;
        wifiAP.password             = data["wifiAP"]["password"] | "1234567890";

        JsonArray BeaconsArray = data["beacons"];
        for (int i = 0; i < BeaconsArray.size(); i++) {
            Beacon bcn;

            bcn.callsign                = BeaconsArray[i]["callsign"] | "NOCALL-7";
            bcn.callsign.toUpperCase();
            bcn.symbol                  = BeaconsArray[i]["symbol"] | "[";
            bcn.overlay                 = BeaconsArray[i]["overlay"] | "/";
            bcn.comment                 = BeaconsArray[i]["comment"] | "";
            bcn.smartBeaconActive       = BeaconsArray[i]["smartBeaconActive"] | true;
            bcn.smartBeaconSetting      = BeaconsArray[i]["smartBeaconSetting"] | 0;
            bcn.micE                    = BeaconsArray[i]["micE"] | "";
            bcn.gpsEcoMode              = BeaconsArray[i]["gpsEcoMode"] | false;
            
            beacons.push_back(bcn);
        }

        display.showSymbol              = data["display"]["showSymbol"] | true;
        display.ecoMode                 = data["display"]["ecoMode"] | false;
        display.timeout                 = data["display"]["timeout"] | 4;
        display.turn180                 = data["display"]["turn180"] | false;

        battery.sendVoltage             = data["battery"]["sendVoltage"] | false;
        battery.voltageAsTelemetry      = data["battery"]["voltageAsTelemetry"] | false;
        battery.sendVoltageAlways       = data["battery"]["sendVoltageAlways"] | false;
        battery.monitorVoltage          = data["battery"]["monitorVoltage"] | false;
        battery.sleepVoltage            = data["battery"]["sleepVoltage"] | 2.9;

        winlink.password                = data["winlink"]["password"] | "NOPASS";

        wxsensor.active                 = data["wxsensor"]["active"] | false;
        wxsensor.temperatureCorrection  = data["wxsensor"]["temperatureCorrection"] | 0.0;
        wxsensor.sendTelemetry          = data["wxsensor"]["sendTelemetry"] | false;

        notification.ledTx              = data["notification"]["ledTx"] | false;
        notification.ledTxPin           = data["notification"]["ledTxPin"]| 13;
        notification.ledMessage         = data["notification"]["ledMessage"] | false;
        notification.ledMessagePin      = data["notification"]["ledMessagePin"] | 2;
        notification.ledFlashlight      = data["notification"]["ledFlashlight"] | false;
        notification.ledFlashlightPin   = data["notification"]["ledFlashlightPin"] | 14;
        notification.buzzerActive       = data["notification"]["buzzerActive"] | false;
        notification.buzzerPinTone      = data["notification"]["buzzerPinTone"] | 33;
        notification.buzzerPinVcc       = data["notification"]["buzzerPinVcc"] | 25;
        notification.bootUpBeep         = data["notification"]["bootUpBeep"] | false;
        notification.txBeep             = data["notification"]["txBeep"] | false;
        notification.messageRxBeep      = data["notification"]["messageRxBeep"] | false;
        notification.stationBeep        = data["notification"]["stationBeep"] | false;
        notification.lowBatteryBeep     = data["notification"]["lowBatteryBeep"] | false;
        notification.shutDownBeep       = data["notification"]["shutDownBeep"] | false;

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

        ptt.active                      = data["pttTrigger"]["active"] | false;
        ptt.io_pin                      = data["pttTrigger"]["io_pin"] | 4;
        ptt.preDelay                    = data["pttTrigger"]["preDelay"] | 0;
        ptt.postDelay                   = data["pttTrigger"]["postDelay"] | 0;
        ptt.reverse                     = data["pttTrigger"]["reverse"] | false;

        bluetooth.active                = data["bluetooth"]["active"] | false;
        bluetooth.deviceName            = data["bluetooth"]["deviceName"] | "LoRaTracker";
        #ifdef HAS_BT_CLASSIC
            bluetooth.useBLE            = data["bluetooth"]["useBLE"] | false;
            bluetooth.useKISS           = data["bluetooth"]["useKISS"] | false;
        #else
            bluetooth.useBLE            = true;    // fixed as BLE
            bluetooth.useKISS           = data["bluetooth"]["useKISS"] | true;    // true=KISS,  false=TNC2            
        #endif

        simplifiedTrackerMode           = data["other"]["simplifiedTrackerMode"] | false;
        sendCommentAfterXBeacons        = data["other"]["sendCommentAfterXBeacons"] | 10;
        path                            = data["other"]["path"] | "WIDE1-1";
        nonSmartBeaconRate              = data["other"]["nonSmartBeaconRate"] | 15;
        rememberStationTime             = data["other"]["rememberStationTime"] | 30;
        standingUpdateTime              = data["other"]["standingUpdateTime"] | 15;
        sendAltitude                    = data["other"]["sendAltitude"] | true;
        disableGPS                      = data["other"]["disableGPS"] | false;
        acceptOwnFrameFromTNC           = data["other"]["acceptOwnFrameFromTNC"] | false;
        email                           = data["other"]["email"] | "";

        configFile.close();
        Serial.println("Config read successfuly");
        return true;
    } else {
        Serial.println("Config file not found");
        return false;
    }
}

bool Configuration::validateConfigFile(const String& currentBeaconCallsign) {
    if (currentBeaconCallsign.indexOf("NOCALL") != -1) {
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "Config", "Change all your callsigns in WebConfig");
        displayShow("ERROR", "Callsigns = NOCALL!", "---> change it !!!", 2000);
        return true;
    } else {
        return false;
    }
}

bool Configuration::validateMicE(const String& currentBeaconMicE) {
    String miceMessageTypes[] = {"111", "110", "101", "100", "011", "010", "001" , "000"};
    int arraySize = sizeof(miceMessageTypes) / sizeof(miceMessageTypes[0]);
    bool validType = false;
    for (int i = 0; i < arraySize; i++) {
        if (currentBeaconMicE == miceMessageTypes[i]) {
            validType = true;
        }
    }
    return validType;
}

void Configuration::init() {
    wifiAP.active                   = true;
    wifiAP.password                 = "1234567890";

    for (int i = 0; i < 3; i++) {
        Beacon beacon;
        beacon.callsign             = "NOCALL-7";
        beacon.symbol               = "[";
        beacon.overlay              = "/";
        beacon.comment              = "";
        beacon.smartBeaconActive    = true;
        beacon.smartBeaconSetting   = 0;
        beacon.micE                 = "";
        beacon.gpsEcoMode           = false;
        beacons.push_back(beacon);
    }

    display.showSymbol              = true;
    display.ecoMode                 = false;
    display.timeout                 = 4;
    display.turn180                 = false;

    battery.sendVoltage             = false;
    battery.voltageAsTelemetry      = false;
    battery.sendVoltageAlways       = false;
    battery.monitorVoltage          = false;
    battery.sleepVoltage            = 2.9;

    winlink.password                = "NOPASS";

    wxsensor.active                 = false;
    wxsensor.temperatureCorrection  = 0.0;
    wxsensor.sendTelemetry          = false;

    notification.ledTx              = false;
    notification.ledTxPin           = 13;
    notification.ledMessage         = false;
    notification.ledMessagePin      = 2;
    notification.ledFlashlight      = false;
    notification.ledFlashlightPin   = 14;
    notification.buzzerActive       = false;
    notification.buzzerPinTone      = 33;
    notification.buzzerPinVcc       = 25;
    notification.bootUpBeep         = false;
    notification.txBeep             = false;
    notification.messageRxBeep      = false;
    notification.stationBeep        = false;
    notification.lowBatteryBeep     = false;
    notification.shutDownBeep       = false;

    for (int j = 0; j < 3; j++) {
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
        }
        loraType.signalBandwidth    = 125000;
        loraType.power              = 20;
        loraTypes.push_back(loraType);
    }

    ptt.active                      = false;
    ptt.io_pin                      = 4;
    ptt.preDelay                    = 0;
    ptt.postDelay                   = 0;
    ptt.reverse                     = false;

    bluetooth.active                = false;
    bluetooth.deviceName            = "LoRaTracker";
    #ifdef HAS_BT_CLASSIC
        bluetooth.useBLE            = false;
        bluetooth.useKISS           = false;
    #else
        bluetooth.useBLE            = true;    // fixed as BLE
        bluetooth.useKISS           = true;
    #endif
    
    simplifiedTrackerMode           = false;
    sendCommentAfterXBeacons        = 10;
    path                            = "WIDE1-1";
    nonSmartBeaconRate              = 15;
    rememberStationTime             = 30;
    standingUpdateTime              = 15;
    sendAltitude                    = true;
    disableGPS                      = false;
    acceptOwnFrameFromTNC           = false;
    email                           = "";

    Serial.println("New Data Created...");
}


Configuration::Configuration() {
    if (!SPIFFS.begin(false)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }

    bool exists = SPIFFS.exists("/tracker_conf.json");
    if (!exists) {        
        init();
        writeFile();
        ESP.restart();
    }
    readFile();
}