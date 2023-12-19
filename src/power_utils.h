#ifndef POWER_UTILS_H_
#define POWER_UTILS_H_

#include <Arduino.h>
#if defined(TTGO_T_Beam_V0_7) || defined(ESP32_DIY_LoRa_GPS) || defined(TTGO_T_LORA_V2_1_GPS) || defined(TTGO_T_LORA_V2_1_TNC) || defined(ESP32_DIY_1W_LoRa_GPS)
// The V0.7 boards have no power managment components connected to TwoWire. 
// Battery charging is controlled by a TP5400 IC indepemdetly from the ESP32.
// Wire.h must be included to maitain software compatibility with V1.0 and 1.2 boards.
#include <Wire.h>
#endif
#if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
#include <axp20x.h>
#endif
#if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262)
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
  void shutdown();
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

  #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
  AXP20X_Class axp;
  #endif
  #if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262)
  XPowersPMU PMU;
  #endif

  bool   BatteryIsConnected;
  String batteryVoltage;
  String batteryChargeDischargeCurrent;
};

#endif
