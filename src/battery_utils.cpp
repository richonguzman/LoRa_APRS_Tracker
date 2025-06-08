#include <Arduino.h>
#include "battery_utils.h"
#include "board_pinout.h"
#include "power_utils.h"
#include "display.h"


uint32_t    lastNoGPSCheckTime  = 0;


namespace BATTERY_Utils {

    String getPercentVoltageBattery(float voltage) {
        int percent = ((voltage - 3.0) / (4.2 - 3.0)) * 100;
        return (percent < 100) ? (((percent < 10) ? "  ": " ") + String(percent)) : "100";
    }

    void checkVoltageWithoutGPSFix() {
        #ifdef BATTERY_PIN
            if (lastNoGPSCheckTime == 0 || millis() - lastNoGPSCheckTime > 15 * 60 * 1000) {
                String batteryVoltage = POWER_Utils::getBatteryInfoVoltage();
                if (batteryVoltage.toFloat() < 3.0) {
                    displayShow("!BATTERY!", "", "LOW BATTERY VOLTAGE!",5000);
                    POWER_Utils::shutdown();
                }
                lastNoGPSCheckTime = millis();
            }
        #endif
    }

}