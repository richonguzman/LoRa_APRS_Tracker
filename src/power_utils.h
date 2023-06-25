#ifndef POWER_UTILS_H_
#define POWER_UTILS_H_

#include <Arduino.h>
#include <axp20x.h>

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

private:
  bool isChargeing();

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

  AXP20X_Class axp;

  bool   BatteryIsConnected;
  String batteryVoltage;
  String batteryChargeDischargeCurrent;
};

#endif
