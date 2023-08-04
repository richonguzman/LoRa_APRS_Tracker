#ifndef POWER_UTILS_H_
#define POWER_UTILS_H_

#include <Arduino.h>
#if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_LORA_V2_1)
#include <axp20x.h>
#endif
#ifdef TTGO_T_Beam_V1_2
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
#endif

class PowerManagement {
public:
  PowerManagement() : BatteryIsConnected(false), batteryVoltage(""), batteryChargeDischargeCurrent("") {};
  bool begin(TwoWire &port);

  void setup();
  void lowerCpuFrequency();
  void handleChargingLed(); 

  void obtainBatteryInfo();
  String getBatteryInfoVoltage();
  String getBatteryInfoCurrent();
  bool getBatteryInfoIsConnected();
  void batteryManager();
  bool isChargeing();

private:
  

  void activateLoRa();
  void deactivateLoRa();

  void activateGPS();
  void deactivateGPS();

  void activateOLED();
  void decativateOLED();

  void enableChgLed();
  void disableChgLed();

  void activateMeasurement();
  void deactivateMeasurement();

  double getBatteryVoltage();
  double getBatteryChargeDischargeCurrent();

  bool isBatteryConnected();

  #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_LORA_V2_1)
  AXP20X_Class axp;
  #endif
  #ifdef TTGO_T_Beam_V1_2
  XPowersPMU PMU;
  #endif

  bool   BatteryIsConnected;
  String batteryVoltage;
  String batteryChargeDischargeCurrent;
};

#endif
