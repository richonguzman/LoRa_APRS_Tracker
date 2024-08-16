#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "configuration.h"
#include "display.h"
#include "logger.h"

extern logging::Logger logger;

Configuration::Configuration() {
    _filePath = "/tracker_config.json";
    if (!SPIFFS.begin(false)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }
    readFile(SPIFFS, _filePath.c_str());
}

void Configuration::readFile(fs::FS &fs, const char *fileName) {
    StaticJsonDocument<2800> data;
    File configFile = fs.open(fileName, "r");
    DeserializationError error = deserializeJson(data, configFile);
    if (error) {
        Serial.println("Failed to read file, using default configuration");
    }

    JsonArray BeaconsArray = data["beacons"];
    for (int i = 0; i < BeaconsArray.size(); i++) {
        Beacon bcn;

        bcn.callsign            = BeaconsArray[i]["callsign"] | "NOCALL-7";
        bcn.callsign.toUpperCase();
        bcn.gpsEcoMode          = BeaconsArray[i]["gpsEcoMode"] | false;
        bcn.symbol              = BeaconsArray[i]["symbol"] | ">";
        bcn.overlay             = BeaconsArray[i]["overlay"] | "/";
        bcn.micE                = BeaconsArray[i]["micE"] | "";
        bcn.comment             = BeaconsArray[i]["comment"] | "";
        bcn.smartBeaconState    = BeaconsArray[i]["smartBeacon"]["active"] | true;
        bcn.slowRate            = BeaconsArray[i]["smartBeacon"]["slowRate"] | 120;
        bcn.slowSpeed           = BeaconsArray[i]["smartBeacon"]["slowSpeed"] | 10;
        bcn.fastRate            = BeaconsArray[i]["smartBeacon"]["fastRate"] | 60;
        bcn.fastSpeed           = BeaconsArray[i]["smartBeacon"]["fastSpeed"]| 70;
        bcn.minTxDist           = BeaconsArray[i]["smartBeacon"]["minTxDist"] | 100;
        bcn.minDeltaBeacon      = BeaconsArray[i]["smartBeacon"]["minDeltaBeacon"] | 12;
        bcn.turnMinDeg          = BeaconsArray[i]["smartBeacon"]["turnMinDeg"] | 10;
        bcn.turnSlope           = BeaconsArray[i]["smartBeacon"]["turnSlope"] | 80;
        
        beacons.push_back(bcn);
    }

    display.showSymbol            = data["display"]["showSymbol"] | true;
    display.ecoMode               = data["display"]["ecoMode"] | false;
    display.timeout               = data["display"]["timeout"] | 4;
    display.turn180               = data["display"]["turn180"] | false;

    winlink.password              = data["winlink"]["password"] | "NOPASS";

    bme.active                    = data["bme"]["active"] | false;
    bme.temperatureCorrection     = data["bme"]["temperatureCorrection"] | 0.0;
    bme.sendTelemetry             = data["bme"]["sendTelemetry"] | false;

    notification.ledTx            = data["notification"]["ledTx"] | false;
    notification.ledTxPin         = data["notification"]["ledTxPin"]| 13;
    notification.ledMessage       = data["notification"]["ledMessage"] | false;
    notification.ledMessagePin    = data["notification"]["ledMessagePin"] | 2;
    notification.ledFlashlight    = data["notification"]["ledFlashlight"] | false;
    notification.ledFlashlightPin = data["notification"]["ledFlashlightPin"] | 14;
    notification.buzzerActive     = data["notification"]["buzzerActive"] | false;
    notification.buzzerPinTone    = data["notification"]["buzzerPinTone"] | 33;
    notification.buzzerPinVcc     = data["notification"]["buzzerPinVcc"] | 25;
    notification.bootUpBeep       = data["notification"]["bootUpBeep"] | false;
    notification.txBeep           = data["notification"]["txBeep"] | false;
    notification.messageRxBeep    = data["notification"]["messageRxBeep"] | false;
    notification.stationBeep      = data["notification"]["stationBeep"] | false;
    notification.lowBatteryBeep   = data["notification"]["lowBatteryBeep"] | false;
    notification.shutDownBeep     = data["notification"]["shutDownBeep"] | false;

    JsonArray LoraTypesArray = data["lora"];
    for (int j = 0; j < LoraTypesArray.size(); j++) {
        LoraType loraType;

        loraType.frequency           = LoraTypesArray[j]["frequency"] | 433775000;
        loraType.spreadingFactor     = LoraTypesArray[j]["spreadingFactor"] | 12;
        loraType.signalBandwidth     = LoraTypesArray[j]["signalBandwidth"] | 125000;
        loraType.codingRate4         = LoraTypesArray[j]["codingRate4"] | 5;
        loraType.power               = LoraTypesArray[j]["power"] | 20;
        loraTypes.push_back(loraType);
    }

    ptt.active                    = data["pttTrigger"]["active"] | false;
    ptt.io_pin                    = data["pttTrigger"]["io_pin"] | 4;
    ptt.preDelay                  = data["pttTrigger"]["preDelay"] | 0;
    ptt.postDelay                 = data["pttTrigger"]["postDelay"] | 0;
    ptt.reverse                   = data["pttTrigger"]["reverse"] | false;

    simplifiedTrackerMode         = data["other"]["simplifiedTrackerMode"] | false;
    sendCommentAfterXBeacons      = data["other"]["sendCommentAfterXBeacons"] | 10;
    path                          = data["other"]["path"] | "WIDE1-1";
    nonSmartBeaconRate            = data["other"]["nonSmartBeaconRate"] | 15;
    rememberStationTime           = data["other"]["rememberStationTime"] | 30;
    maxDistanceToTracker          = data["other"]["maxDistanceToTracker"] | 30;
    standingUpdateTime            = data["other"]["standingUpdateTime"] | 15;
    sendAltitude                  = data["other"]["sendAltitude"] | true;
    sendBatteryInfo               = data["other"]["sendBatteryInfo"] | false;
    bluetoothType                 = data["other"]["bluetoothType"] | 1;
    bluetoothActive               = data["other"]["bluetoothActive"] | true;
    disableGPS                    = data["other"]["disableGPS"] | false;

    configFile.close();
}

bool Configuration::validateConfigFile(const String& currentBeaconCallsign) {
    if (currentBeaconCallsign.indexOf("NOCALL") != -1) {
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "Config", "Change all your callsigns in 'data/tracker_config.json' and upload it via 'Upload File System image'");
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