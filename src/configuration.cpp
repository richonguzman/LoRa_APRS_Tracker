#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "configuration.h"
#include "display.h"
#include "logger.h"

extern logging::Logger logger;

Configuration::Configuration() {
    _filePath = "/tracker_config.json";;
    if (!SPIFFS.begin(false)) {
      Serial.println("SPIFFS Mount Failed");
      return;
    }
    readFile(SPIFFS, _filePath.c_str());
}

void Configuration::readFile(fs::FS &fs, const char *fileName) {
    StaticJsonDocument<2048> data;
    File configFile = fs.open(fileName, "r");
    DeserializationError error = deserializeJson(data, configFile);
    if (error) {
        Serial.println("Failed to read file, using default configuration");
    }

    JsonArray BeaconsArray = data["beacons"];
    for (int i = 0; i < BeaconsArray.size(); i++) {
        Beacon bcn;

        bcn.callsign          = BeaconsArray[i]["callsign"].as<String>();
        bcn.symbol            = BeaconsArray[i]["symbol"].as<String>();
        bcn.comment           = BeaconsArray[i]["comment"].as<String>();

        bcn.smartBeaconState  = BeaconsArray[i]["smart_beacon"]["active"].as<bool>() | true;
        bcn.slowRate          = BeaconsArray[i]["smart_beacon"]["slowRate"].as<int>() | 120;
        bcn.slowSpeed         = BeaconsArray[i]["smart_beacon"]["slowSpeed"].as<int>() | 10;
        bcn.fastRate          = BeaconsArray[i]["smart_beacon"]["fastRate"].as<int>() | 60;
        bcn.fastSpeed         = BeaconsArray[i]["smart_beacon"]["fastSpeed"].as<int>() | 70;
        bcn.minTxDist         = BeaconsArray[i]["smart_beacon"]["minTxDist"].as<int>() | 100;
        bcn.minDeltaBeacon    = BeaconsArray[i]["smart_beacon"]["minDeltaBeacon"].as<int>() | 12;
        bcn.turnMinDeg        = BeaconsArray[i]["smart_beacon"]["turnMinDeg"].as<int>() | 10;
        bcn.turnSlope         = BeaconsArray[i]["smart_beacon"]["turnSlope"].as<int>() | 80;

        beacons.push_back(bcn);
    }

    loramodule.frequency          = data["lora"]["frequency"].as<long>() | 433775000;
    loramodule.spreadingFactor    = data["lora"]["spreadingFactor"].as<int>() | 12;
    loramodule.signalBandwidth    = data["lora"]["signalBandwidth"].as<long>() | 125000;
    loramodule.codingRate4        = data["lora"]["codingRate4"].as<int>() | 5;
    loramodule.power              = data["lora"]["power"].as<int>() | 20;

    showSymbolOnDisplay           = data["other"]["showSymbolOnDisplay"].as<bool>() | true;
    displayEcoMode                = data["other"]["displayEcoMode"].as<bool>() | false;
    displayTimeout                = data["other"]["displayTimeout"].as<int>() | 4;
    overlay                       = data["other"]["overlay"].as<String>();
    nonSmartBeaconRate            = data["other"]["nonSmartBeaconRate"].as<int>() | 15;
    rememberStationTime           = data["other"]["rememberStationTime"].as<int>()  | 30;
    maxDistanceToTracker          = data["other"]["maxDistanceToTracker"].as<int>() | 30;
    standingUpdateTime            = data["other"]["standingUpdateTime"].as<int>() | 15;
    sendAltitude                  = data["other"]["sendAltitude"].as<bool>() | true;

    configFile.close();
}

void Configuration::validateConfigFile(String currentBeaconCallsign) {
  if (currentBeaconCallsign == "NOCALL-7") {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "Config", "Change your settings in 'data/tracker_config.json' and upload it via 'Upload File System image'");
    show_display("ERROR", "Change your settings", "'tracker_config.json'", "upload it via --> ", "'Upload File System image'");
    while (true) {
        delay(1000);
    }
  }
}