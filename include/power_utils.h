#ifndef POWER_UTILS_H_
#define POWER_UTILS_H_

#include <Arduino.h>
#include "board_pinout.h"
#if defined(HAS_AXP2101) || defined(HAS_AXP192)
    #include "XPowersLib.h"
#else
    #include <Wire.h>
#endif

namespace POWER_Utils {

    double  getBatteryVoltage();
    const String getBatteryInfoVoltage();
    const String getBatteryInfoCurrent();
    bool    getBatteryInfoIsConnected();

    void    enableChgLed();
    void    disableChgLed();

    bool    isCharging();
    void    handleChargingLed();
    double  getBatteryChargeDischargeCurrent();
    bool    isBatteryConnected();
    void    obtainBatteryInfo();
    void    batteryManager();

    void    activateMeasurement();

    void    activateGPS();
    void    deactivateGPS();

    void    activateLoRa();
    void    deactivateLoRa();

    void    externalPinSetup();

    bool    begin(TwoWire &port);
    void    setup();

    void    lowerCpuFrequency();
    void    shutdown();
  
}

#endif
