#ifndef POWER_MANAGEMENT_H_
#define POWER_MANAGEMENT_H_

#include <Arduino.h>
#include <axp20x.h>

class PowerManagement {
public:
  PowerManagement();
  bool begin(TwoWire &port);

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

  bool isBatteryConnect();
  bool isChargeing();

private:
  AXP20X_Class axp;
};

#endif
