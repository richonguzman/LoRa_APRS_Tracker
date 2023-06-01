#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

#include <Arduino.h>
#include <SPIFFS.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <vector>

class Beacon {
public:
  String callsign;
  String symbol;
  String comment;
  bool  smartBeaconState;
  int   slowRate;
  int   slowSpeed;
  int   fastRate;
  int   fastSpeed;
  int   minTxDist;
  int   minDeltaBeacon;
  int   turnMinDeg;
  int   turnSlope;
};

class LoraModule {
public:
  long  frequency;
  int   spreadingFactor;
  long  signalBandwidth;
  int   codingRate4;
  int   power;
};


class Configuration {
public:

  std::vector<Beacon> beacons;  
  LoraModule loramodule;

  bool    displayEcoMode;
  int     displayTimeout;
  String  destination;
  String  path;
  String  overlay;
  int     nonSmartBeaconRate;
  int     listeningTrackerTime;
  int     maxDistanceToTracker;
  bool    defaultStatusAfterBoot;
  String  defaultStatus;
  bool    ecoMode;
  bool    standingUpdate;
  int     standingUpdateTime;
  bool    sendAltitude;


  Configuration(String &filePath) {
    _filePath = filePath;
    if (!SPIFFS.begin(false)) {
      Serial.println("SPIFFS Mount Failed");
      return;
    }
    readFile(SPIFFS, _filePath.c_str());
  }

private:
  String _filePath;

  void readFile(fs::FS &fs, const char *fileName) {
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

    displayEcoMode                = data["other"]["displayEcoMode"].as<bool>();
    displayTimeout                = data["other"]["displayTimeout"].as<int>();
    destination                   = data["other"]["destination"].as<String>();
    path                          = data["other"]["path"].as<String>();
    overlay                       = data["other"]["overlay"].as<String>();
    nonSmartBeaconRate            = data["other"]["nonSmartBeaconRate"].as<int>();
    listeningTrackerTime          = data["other"]["listeningTrackerTime"].as<int>();
    maxDistanceToTracker          = data["other"]["maxDistanceToTracker"].as<int>();
    defaultStatusAfterBoot        = data["other"]["defaultStatusAfterBoot"].as<bool>();
    defaultStatus                 = data["other"]["defaultStatus"].as<String>();
    ecoMode                       = data["other"]["ecoMode "].as<bool>();
    standingUpdateTime            = data["other"]["standingUpdateTime"].as<int>();
    sendAltitude                  = data["other"]["sendAltitude"].as<bool>();

    configFile.close();
  }
};
#endif