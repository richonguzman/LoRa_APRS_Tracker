/* Copyright (C) 2025 Ricardo Guzman - CA2RXU
 * 
 * This file is part of LoRa APRS Tracker.
 * 
 * LoRa APRS Tracker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or 
 * (at your option) any later version.
 * 
 * LoRa APRS Tracker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with LoRa APRS Tracker. If not, see <https://www.gnu.org/licenses/>.
 */

#include <Arduino.h>
#include "configuration.h"
#include "battery_utils.h"
#include "board_pinout.h"
#include "power_utils.h"
#include "display.h"


#ifdef ADC_CTRL
    uint32_t    adcCtrlTime         = 0;
    uint8_t     measuring_State     = 0;
#endif

extern      Configuration           Config;
uint32_t    batteryMeasurmentTime   = 0;

//
extern String      batteryVoltage;
//


namespace BATTERY_Utils {

    String getPercentVoltageBattery(float voltage) {
        int percent = ((voltage - 3.0) / (4.2 - 3.0)) * 100;
        return (percent < 100) ? (((percent < 10) ? "  ": " ") + String(percent)) : "100";
    }

    void monitor() {
        #if defined(HAS_AXP192) || defined(HAS_AXP2101)
            POWER_Utils::obtainBatteryInfo();
            POWER_Utils::handleChargingLed();
        #elif defined(BATTERY_PIN)
            if (batteryMeasurmentTime == 0 || (millis() - batteryMeasurmentTime) > 30 * 1000){ //At least 30 seconds have to pass between measurements
                #ifdef ADC_CTRL
                    switch(measuring_State){
                        case 0:     //ADC_CTRL_ON State
                            POWER_Utils::adc_ctrl_ON();
                            adcCtrlTime = millis();
                            measuring_State = 1;
                            break;
                        case 1:     // Measurement State
                            if((millis() - adcCtrlTime) > 50){ //At least 50ms have to pass after ADC_Ctrl Mosfet is turned on for voltage to stabilize
                                POWER_Utils::obtainBatteryInfo(1);
                                POWER_Utils::adc_ctrl_OFF();
                                measuring_State = 0;
                                
                                if (batteryVoltage.toFloat() < (Config.battery.sleepVoltage - 0.1)) {
                                    displayShow("!BATTERY!", "", "LOW BATTERY VOLTAGE!",5000);
                                    POWER_Utils::shutdown();
                                }
                            }
                            break;
                    }
                #else
                    POWER_Utils::obtainBatteryInfo();
                #endif
                batteryMeasurmentTime = millis();
            }
        #endif
    }
    

}