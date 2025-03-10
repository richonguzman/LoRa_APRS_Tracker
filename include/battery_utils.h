#ifndef BATTERY_UTILS_H_
#define BATTERY_UTILS_H_

#include <Arduino.h>


namespace BATTERY_Utils {

    String  generateEncodedTelemetry(float voltage);
    String  getPercentVoltageBattery(float voltage);
    void    checkVoltageWithoutGPSFix();
    
}

#endif