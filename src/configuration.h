#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

#include <Arduino.h>
#include <FS.h>
#include <vector>

class Beacon {
public:
  String callsign;
  String symbol;
  String overlay;
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

class Ptt {
public:
  bool  active;
  int   io_pin;
  int   preDelay;
  int   postDelay;
  bool  reverse;
};

class BME {
public:
  bool    active;
};

class Configuration {
public:

  std::vector<Beacon> beacons;  
  LoraModule          loramodule;
  Ptt                 ptt;
  BME                 bme;
  
  bool    simplifiedTrackerMode;
  bool    showSymbolOnScreen;
  int     sendCommentAfterXBeacons;
  bool    displayEcoMode;
  int     displayTimeout;
  String  path;
  int     nonSmartBeaconRate;
  int     rememberStationTime;
  int     maxDistanceToTracker;
  int     standingUpdateTime;
  bool    sendAltitude;
  bool    sendBatteryInfo;
  bool    bluetooth;
  bool    disableGps;

  Configuration();
  void validateConfigFile(String currentBeaconCallsign);

private:
  void readFile(fs::FS &fs, const char *fileName) ;
  String _filePath;
};
#endif