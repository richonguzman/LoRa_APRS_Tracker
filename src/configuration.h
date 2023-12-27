#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

#include <Arduino.h>
#include <vector>
#include <FS.h>

class Beacon {
public:
  String callsign;
  String symbol;
  String overlay;
  String micE;
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
  bool    sendTelemetry;
  int     heightCorrection;
};

class Notification {
public:
  bool    ledTx;
  int     ledTxPin;
  bool    ledMessage;
  int     ledMessagePin;
  bool    ledFlashlight;
  int     ledFlashlightPin;
  bool    buzzerActive;
  int     buzzerPinTone;
  int     buzzerPinVcc;
  bool    bootUpBeep;
  bool    txBeep;
  bool    messageRxBeep;
  bool    stationBeep;
  bool    lowBatteryBeep;
};


class Configuration {
public:

  std::vector<Beacon> beacons;  
  LoraModule          loramodule;
  Ptt                 ptt;
  BME                 bme;
  Notification        notification;
  
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
  int     bluetoothType;
  bool    bluetoothActive;
  bool    disableGPS;

  Configuration();
  void validateConfigFile(String currentBeaconCallsign);
  bool validateMicE(String currentBeaconMicE);

private:
  void readFile(fs::FS &fs, const char *fileName) ;
  String _filePath;
};
#endif