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

        bcn.smartBeaconState  = BeaconsArray[i]["smart_beacon"]["active"].as<bool>();
        bcn.slowRate          = BeaconsArray[i]["smart_beacon"]["slowRate"].as<int>();
        bcn.slowSpeed         = BeaconsArray[i]["smart_beacon"]["slowSpeed"].as<int>();
        bcn.fastRate          = BeaconsArray[i]["smart_beacon"]["fastRate"].as<int>();
        bcn.fastSpeed         = BeaconsArray[i]["smart_beacon"]["fastSpeed"].as<int>();
        bcn.minTxDist         = BeaconsArray[i]["smart_beacon"]["minTxDist"].as<int>();
        bcn.minDeltaBeacon    = BeaconsArray[i]["smart_beacon"]["minDeltaBeacon"].as<int>();
        bcn.turnMinDeg        = BeaconsArray[i]["smart_beacon"]["turnMinDeg"].as<int>();
        bcn.turnSlope         = BeaconsArray[i]["smart_beacon"]["turnSlope"].as<int>();

        beacons.push_back(bcn);
    }

    loramodule.frequency          = data["lora"]["frequency"].as<long>();
    loramodule.spreadingFactor    = data["lora"]["spreadingFactor"].as<int>();
    loramodule.signalBandwidth    = data["lora"]["signalBandwidth"].as<long>();
    loramodule.codingRate4        = data["lora"]["codingRate4"].as<int>();
    loramodule.power              = data["lora"]["power"].as<int>();

    showSymbolOnDisplay           = data["other"]["showSymbolOnDisplay"].as<bool>();
    sendCommentAfterXBeacons      = data["other"]["sendCommentAfterXBeacons"].as<int>();
    displayEcoMode                = data["other"]["displayEcoMode"].as<bool>();
    displayTimeout                = data["other"]["displayTimeout"].as<int>();
    overlay                       = data["other"]["overlay"].as<String>();
    path                          = data["other"]["path"].as<String>();
    nonSmartBeaconRate            = data["other"]["nonSmartBeaconRate"].as<int>();
    rememberStationTime           = data["other"]["rememberStationTime"].as<int>();
    maxDistanceToTracker          = data["other"]["maxDistanceToTracker"].as<int>();
    standingUpdateTime            = data["other"]["standingUpdateTime"].as<int>();
    sendAltitude                  = data["other"]["sendAltitude"].as<bool>();

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