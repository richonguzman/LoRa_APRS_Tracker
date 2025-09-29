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
    uint8_t     measuringState      = 0;
#endif

#ifdef HAS_AXP192
    extern XPowersAXP192 PMU;
#endif
#ifdef HAS_AXP2101
    extern XPowersAXP2101 PMU;
#endif

extern      Configuration           Config;
uint32_t    batteryMeasurmentTime   = 0;
int         averageReadings         = 20;

String      batteryVoltage          = "";
bool        batteryConnected      = false;

extern      String                  batteryChargeDischargeCurrent;

float       lora32BatReadingCorr    = 6.5; // % of correction to higher value to reflect the real battery voltage (adjust this to your needs)


namespace BATTERY_Utils {

    String getPercentVoltageBattery(float voltage) {
        int percent = ((voltage - 3.0) / (4.2 - 3.0)) * 100;
        return (percent < 100) ? (((percent < 10) ? "  ": " ") + String(percent)) : "100";
    }

    String getBatteryInfoVoltage() {
        return batteryVoltage;
    }

    float readBatteryVoltage() {
        #if defined(HAS_AXP192) || defined(HAS_AXP2101)
            return (PMU.getBattVoltage() / 1000.0);
        #else
            #ifdef BATTERY_PIN
                int sampleSum = 0;
                analogRead(BATTERY_PIN);    // Dummy Read
                delay(1);
                for (int i = 0; i < averageReadings; i++) {
                    sampleSum += analogRead(BATTERY_PIN);
                    delay(3);
                }
                int adc_value = sampleSum/averageReadings;
                double voltage = (adc_value * 3.3 ) / 4095.0;

                #ifdef LIGHTTRACKER_PLUS_1_0
                    double inputDivider = (1.0 / (560.0 + 100.0)) * 100.0;  // The voltage divider is a 560k + 100k resistor in series, 100k on the low side.
                    return ((voltage / inputDivider) * 1.11029) + 0.14431;
                #endif
                #if defined(TTGO_T_Beam_V0_7) || defined(TTGO_T_LORA32_V2_1_GPS) || defined(TTGO_T_LORA32_V2_1_GPS_915) || defined(TTGO_T_LORA32_V2_1_TNC) || defined(TTGO_T_LORA32_V2_1_TNC_915) || defined(ESP32_DIY_LoRa_GPS) || defined(ESP32_DIY_LoRa_GPS_915) || defined(ESP32_DIY_1W_LoRa_GPS) || defined(ESP32_DIY_1W_LoRa_GPS_915) || defined(ESP32_DIY_1W_LoRa_GPS_LLCC68) || defined(OE5HWN_MeshCom) || defined(TTGO_T_DECK_GPS) || defined(TTGO_T_DECK_PLUS) || defined(ESP32S3_DIY_LoRa_GPS) || defined(ESP32S3_DIY_LoRa_GPS_915) || defined(TROY_LoRa_APRS) || defined(RPC_Electronics_1W_LoRa_GPS)
                    return (2 * (voltage + 0.1)) * (1 + (lora32BatReadingCorr/100)); // (2 x 100k voltage divider) 2 x voltage divider/+0.1 because ESP32 nonlinearity ~100mV ADC offset/extra correction
                #endif
                #if defined(HELTEC_V3_GPS) || defined(HELTEC_V3_TNC) || defined(HELTEC_V3_2_GPS) || defined(HELTEC_V3_2_TNC) || defined(HELTEC_WIRELESS_TRACKER) || defined(HELTEC_WSL_V3_GPS_DISPLAY) || defined(ESP32_C3_DIY_LoRa_GPS) || defined(ESP32_C3_DIY_LoRa_GPS_915) || defined(WEMOS_ESP32_Bat_LoRa_GPS)
                    double inputDivider = (1.0 / (390.0 + 100.0)) * 100.0;  // The voltage divider is a 390k + 100k resistor in series, 100k on the low side. 
                    return (voltage / inputDivider) + 0.285; // Yes, this offset is excessive, but the ADC on the ESP32s3 is quite inaccurate and noisy. Adjust to own measurements.
                #endif
                #if defined(HELTEC_V2_GPS) || defined(HELTEC_V2_GPS_915) || defined(HELTEC_V2_TNC) || defined(F4GOH_1W_LoRa_Tracker) || defined(F4GOH_1W_LoRa_Tracker_LLCC68)
                    double inputDivider = (1.0 / (220.0 + 100.0)) * 100.0;  // The voltage divider is a 220k + 100k resistor in series, 100k on the low side. 
                    return (voltage / inputDivider) + 0.285; // Yes, this offset is excessive, but the ADC on the ESP32 is quite inaccurate and noisy. Adjust to own measurements.
                #endif
            #else
                return 0.0;
            #endif
        #endif
    }

    void obtainBatteryInfo() {
        #if defined(HAS_AXP192) || defined(HAS_AXP2101)
            batteryConnected = PMU.isBatteryConnect();
            if (batteryConnected) {
                batteryVoltage                  = String(readBatteryVoltage(), 2);
                batteryChargeDischargeCurrent   = String(POWER_Utils::getBatteryChargeDischargeCurrent(), 0);
            }
        #else
            batteryVoltage = String(readBatteryVoltage(), 2);
            if (batteryVoltage.toFloat() > 1.5) batteryConnected = true;
        #endif
    }   

    void monitor() {
        #if defined(HAS_AXP192) || defined(HAS_AXP2101)
            if (batteryMeasurmentTime == 0 || (millis() - batteryMeasurmentTime) > 1 * 1000){
                obtainBatteryInfo();
                POWER_Utils::handleChargingLed();
                batteryMeasurmentTime = millis();
            }
        #elif defined(BATTERY_PIN)
            if (batteryMeasurmentTime == 0 || (millis() - batteryMeasurmentTime) > 30 * 1000){ //At least 30 seconds have to pass between measurements
                #ifdef ADC_CTRL
                    switch(measuringState){
                        case 0:     // Initial Measurement
                            POWER_Utils::adc_ctrl_ON();
                            adcCtrlTime = millis();
                            delay(50);
                            obtainBatteryInfo();
                            POWER_Utils::adc_ctrl_OFF();
                            measuringState = 1;
                            break;
                        case 1:     //ADC_CTRL_ON State
                            POWER_Utils::adc_ctrl_ON();
                            adcCtrlTime = millis();
                            measuringState = 2;
                            break;
                        case 2:     // Measurement State
                            if((millis() - adcCtrlTime) > 50){ //At least 50ms have to pass after ADC_Ctrl Mosfet is turned on for voltage to stabilize
                                obtainBatteryInfo();
                                POWER_Utils::adc_ctrl_OFF();
                                measuringState = 1;
                                
                                if (batteryVoltage.toFloat() < (Config.battery.sleepVoltage - 0.1)) {
                                    displayShow("!BATTERY!", "", "LOW BATTERY VOLTAGE!",5000);
                                    POWER_Utils::shutdown();
                                }
                            }
                            break;
                    }
                #else
                    obtainBatteryInfo();
                #endif
                batteryMeasurmentTime = millis();
            }
        #endif
    }
    

}