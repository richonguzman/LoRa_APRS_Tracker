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
  bool    standingUpdate;
  int     standingUpdateTime;
  bool    sendAltitude;

  Configuration(const String &filePath);
  void validateConfigFile(String currentBeaconCallsign);

private:
  Configuration() {}; // Hide default constructor
  void readFile(fs::FS &fs, const char *fileName) ;
  String _filePath;
};
#endif