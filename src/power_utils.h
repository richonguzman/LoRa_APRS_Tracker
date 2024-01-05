#ifndef POWER_UTILS_H_
#define POWER_UTILS_H_

#include <Arduino.h>
#include "XPowersLib.h"

namespace POWER_Utils {

  double getBatteryVoltage();
  String getBatteryInfoVoltage();
  String getBatteryInfoCurrent();
  bool getBatteryInfoIsConnected();

  void enableChgLed();
  void disableChgLed();

  bool isCharging();
  void handleChargingLed();
  double getBatteryChargeDischargeCurrent();
  bool isBatteryConnected();
  void obtainBatteryInfo();
  void batteryManager();

  void activateMeasurement();

  void activateGPS();
  void deactivateGPS();

  void activateLoRa();
  void deactivateLoRa();

  bool begin(TwoWire &port);
  void setup();

  void lowerCpuFrequency();
  void shutdown();
}

#endif
