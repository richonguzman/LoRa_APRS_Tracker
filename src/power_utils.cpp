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

#include <esp_log.h>
#include <esp_task_wdt.h>
#include <SPI.h>
#include "notification_utils.h"
#include "configuration.h"
#include "battery_utils.h"
#include "board_pinout.h"
#include "power_utils.h"
#include "lora_utils.h"
#include "ble_utils.h"
#include "gps_utils.h"
#include "display.h"
#if !defined(TTGO_T_Beam_S3_SUPREME_V3) && !defined(HELTEC_WIRELESS_TRACKER)
    #define I2C_SDA 21
    #define I2C_SCL 22
    #define IRQ_PIN 35
#endif

#ifdef TTGO_T_Beam_S3_SUPREME_V3
    #define I2C0_SDA 17
    #define I2C0_SCL 18
    #define I2C1_SDA 42
    #define I2C1_SCL 41
    #define IRQ_PIN  40
#endif

#ifdef HAS_AXP192
    XPowersAXP192 PMU;
#endif
#ifdef HAS_AXP2101
    XPowersAXP2101 PMU;
#endif

extern  Configuration                   Config;
extern  bool                            transmitFlag;
extern  bool                            gpsIsActive;

static const char *TAG = "Power";

bool    pmuInterrupt;
bool    disableGPS;

String  batteryChargeDischargeCurrent    = "";


namespace POWER_Utils {    

    #ifdef VEXT_CTRL
        void vext_ctrl_ON() {
            #if defined(HELTEC_V3_GPS) || defined(HELTEC_V3_TNC) || defined(HELTEC_WIRELESS_TRACKER) || defined(HELTEC_WSL_V3_GPS_DISPLAY)
                digitalWrite(VEXT_CTRL, HIGH);
            #endif
            #if defined(HELTEC_V3_2_GPS) || defined(HELTEC_V3_2_TNC)
                digitalWrite(VEXT_CTRL, LOW);
            #endif
        }

        void vext_ctrl_OFF() {
            #if defined(HELTEC_V3_GPS) || defined(HELTEC_V3_TNC) || defined(HELTEC_WIRELESS_TRACKER) || defined(HELTEC_WSL_V3_GPS_DISPLAY)
                digitalWrite(VEXT_CTRL, LOW);
            #endif
            #if defined(HELTEC_V3_2_GPS) || defined(HELTEC_V3_2_TNC)
                digitalWrite(VEXT_CTRL, HIGH);
            #endif
        }
    #endif


    #ifdef ADC_CTRL
        void adc_ctrl_ON() {
            #if defined(HELTEC_WIRELESS_TRACKER) || defined(HELTEC_V3_2_GPS) || defined(HELTEC_V3_2_TNC)
                digitalWrite(ADC_CTRL, HIGH);
            #endif
            #if defined(HELTEC_V3_GPS) || defined(HELTEC_V3_TNC) || defined(HELTEC_V2_GPS) || defined(HELTEC_V2_GPS_915) || defined(HELTEC_V2_TNC) || defined(HELTEC_WSL_V3_GPS_DISPLAY)
                digitalWrite(ADC_CTRL, LOW);
            #endif
        }

        void adc_ctrl_OFF() {
            #if defined(HELTEC_WIRELESS_TRACKER) || defined(HELTEC_V3_2_GPS) || defined(HELTEC_V3_2_TNC)
                digitalWrite(ADC_CTRL, LOW);
            #endif
            #if defined(HELTEC_V3_GPS) || defined(HELTEC_V3_TNC) || defined(HELTEC_V2_GPS) || defined(HELTEC_V2_GPS_915) || defined(HELTEC_V2_TNC) || defined(HELTEC_WSL_V3_GPS_DISPLAY)
                digitalWrite(ADC_CTRL, HIGH);
            #endif
        }
    #endif

    #if defined(HAS_AXP192) || defined(HAS_AXP2101)
        void activateMeasurement() {
                PMU.disableTSPinMeasure();
                PMU.enableBattDetection();
                PMU.enableVbusVoltageMeasure();
                PMU.enableBattVoltageMeasure();
                PMU.enableSystemVoltageMeasure();
        }

        void enableChgLed() {
            PMU.setChargingLedMode(XPOWERS_CHG_LED_ON);
        }

        void disableChgLed() {
            PMU.setChargingLedMode(XPOWERS_CHG_LED_OFF);
        }

        void handleChargingLed() {
            if (isCharging()) {
                enableChgLed();
            } else {
                disableChgLed();
            }
        }

        String getBatteryInfoCurrent() {
            return batteryChargeDischargeCurrent;
        }

        float getBatteryChargeDischargeCurrent() {
            #ifdef HAS_AXP192
                if (PMU.isCharging()) {
                    return PMU.getBatteryChargeCurrent();
                }
                return -1.0 * PMU.getBattDischargeCurrent();
            #endif
            #ifdef HAS_AXP2101
                return PMU.getBatteryPercent();
            #endif
        }
    #endif

    bool isCharging() {
        #if defined(HAS_AXP192) || defined(HAS_AXP2101)
            return PMU.isCharging();
        #else
            return 0;
        #endif
    }  

    void activateGPS() {
        #ifdef HAS_AXP192
            PMU.setLDO3Voltage(3300);
            PMU.enableLDO3();
        #endif

        #ifdef HAS_AXP2101
            #ifdef TTGO_T_Beam_S3_SUPREME_V3
                PMU.setALDO4Voltage(3300);
                PMU.enableALDO4();
            #else
                PMU.setALDO3Voltage(3300);
                PMU.enableALDO3();
            #endif
        #endif
        #ifdef HELTEC_WIRELESS_TRACKER
            vext_ctrl_ON();
        #endif
        gpsIsActive = true;
    }

    void deactivateGPS() {
        #ifdef HAS_AXP192
            PMU.disableLDO3();
        #endif

        #ifdef HAS_AXP2101
            #ifdef TTGO_T_Beam_S3_SUPREME_V3
                PMU.disableALDO4();
            #else
                PMU.disableALDO3();
            #endif
        #endif
        #ifdef HELTEC_WIRELESS_TRACKER
            vext_ctrl_OFF();
        #endif
        gpsIsActive = false;
    }

    void activateLoRa() {
        #ifdef HAS_AXP192
            PMU.setLDO2Voltage(3300);
            PMU.enableLDO2();
        #endif

        #ifdef HAS_AXP2101
            #ifdef TTGO_T_Beam_S3_SUPREME_V3
                PMU.setALDO3Voltage(3300);
                PMU.enableALDO3();
            #else
                PMU.setALDO2Voltage(3300);
                PMU.enableALDO2();
            #endif
        #endif
    }

    void deactivateLoRa() {
        #ifdef HAS_AXP192
            PMU.disableLDO2();
        #endif

        #ifdef HAS_AXP2101
            #ifdef TTGO_T_Beam_S3_SUPREME_V3
                PMU.disableALDO3();
            #else
                PMU.disableALDO2();
            #endif
        #endif
    }

    void externalPinSetup() {
        #ifdef CROWPANEL_ADVANCE_35
            // CrowPanel: buzzer on IO8, no LEDs or PTT
            Config.notification.buzzerPinTone = 8;
            Config.notification.buzzerPinVcc = -1;  // No VCC pin, buzzer driven directly
            Config.notification.ledTx = false;
            Config.notification.ledTxPin = -1;       // No LED on CrowPanel
            Config.notification.ledMessage = false;
            Config.notification.ledMessagePin = -1;  // IO2 is LoRa NRESET — must not be used as LED!
            Config.notification.ledFlashlight = false;
            Config.notification.ledFlashlightPin = -1;
            Config.ptt.active = false;
            Config.ptt.io_pin = -1;
        #endif
        ESP_LOGI(TAG, "Pin setup: buzzerTone=%d buzzerVcc=%d ledTx=%d ledMsg=%d ptt=%d",
                 Config.notification.buzzerPinTone, Config.notification.buzzerPinVcc,
                 Config.notification.ledTxPin, Config.notification.ledMessagePin,
                 Config.ptt.io_pin);
        #ifdef HAS_I2S
            // T-Deck Plus uses I2S speaker, no GPIO buzzer pins needed
            if (Config.notification.buzzerActive && Config.notification.bootUpBeep) {
                NOTIFICATION_Utils::start();
            }
        #else
            if (Config.notification.buzzerActive && Config.notification.buzzerPinTone >= 0 && Config.notification.buzzerPinTone <= 48) {
                pinMode(Config.notification.buzzerPinTone, OUTPUT);
                if (Config.notification.buzzerPinVcc >= 0 && Config.notification.buzzerPinVcc <= 48) {
                    pinMode(Config.notification.buzzerPinVcc, OUTPUT);
                }
                if (Config.notification.bootUpBeep) NOTIFICATION_Utils::start();
            } else if (Config.notification.buzzerActive) {
                ESP_LOGW(TAG, "Buzzer tone pin invalid (%d), disabling",
                         Config.notification.buzzerPinTone);
                Config.notification.buzzerActive = false;
            }
        #endif

        if (Config.notification.ledTx && Config.notification.ledTxPin >= 0 && Config.notification.ledTxPin <= 48) {
            pinMode(Config.notification.ledTxPin, OUTPUT);
        } else if (Config.notification.ledTx) {
            ESP_LOGW(TAG, "Led Tx Pin invalid (%d), disabling", Config.notification.ledTxPin);
            Config.notification.ledTx = false;
        }

        if (Config.notification.ledMessage && Config.notification.ledMessagePin >= 0 && Config.notification.ledMessagePin <= 48) {
            pinMode(Config.notification.ledMessagePin, OUTPUT);
        } else if (Config.notification.ledMessage) {
            ESP_LOGW(TAG, "Led Message Pin invalid (%d), disabling", Config.notification.ledMessagePin);
            Config.notification.ledMessage = false;
        }

        if (Config.notification.ledFlashlight && Config.notification.ledFlashlightPin >= 0 && Config.notification.ledFlashlightPin <= 48) {
            pinMode(Config.notification.ledFlashlightPin, OUTPUT);
        } else if (Config.notification.ledFlashlight) {
            ESP_LOGW(TAG, "Led Flashlight Pin invalid (%d), disabling", Config.notification.ledFlashlightPin);
            Config.notification.ledFlashlight = false;
        }

        if (Config.ptt.active && Config.ptt.io_pin >= 0 && Config.ptt.io_pin <= 48) {
            pinMode(Config.ptt.io_pin, OUTPUT);
            digitalWrite(Config.ptt.io_pin, Config.ptt.reverse ? HIGH : LOW);
        } else if (Config.ptt.active) {
            ESP_LOGW(TAG, "PTT Pin invalid (%d), disabling", Config.ptt.io_pin);
            Config.ptt.active = false;
        }
    }

    bool begin(TwoWire &port) {
        #if !defined(HAS_AXP192) && !defined(HAS_AXP2101)
            return true; // no powerManagment chip for this boards (only a few measure battery voltage).
        #endif

        #ifdef HAS_AXP192
            bool result = PMU.begin(Wire, AXP192_SLAVE_ADDRESS, I2C_SDA, I2C_SCL);
            if (result) {
                PMU.disableDC2();
                PMU.disableLDO2();
                PMU.disableLDO3();
                PMU.setDC1Voltage(3300);
                PMU.enableDC1();
                PMU.setProtectedChannel(XPOWERS_DCDC3);
                PMU.disableIRQ(XPOWERS_AXP192_ALL_IRQ);
            }
            return result;
        #endif

        #ifdef HAS_AXP2101
            #ifdef TTGO_T_Beam_S3_SUPREME_V3
                bool result = PMU.begin(Wire1, AXP2101_SLAVE_ADDRESS, I2C1_SDA, I2C1_SCL);
            #else
                bool result = PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL);
            #endif
            if (result) {
                PMU.disableDC2();
                PMU.disableDC3();
                PMU.disableDC4();
                PMU.disableDC5();
                #ifndef TTGO_T_Beam_S3_SUPREME_V3
                    PMU.disableALDO1();
                    PMU.disableALDO4();
                #endif
                PMU.disableBLDO1();
                PMU.disableBLDO2();
                PMU.disableDLDO1();
                PMU.disableDLDO2();
                PMU.setDC1Voltage(3300);
                PMU.enableDC1();
                #ifdef TTGO_T_Beam_S3_SUPREME_V3
                    PMU.setALDO1Voltage(3300);
                #endif
                PMU.setButtonBatteryChargeVoltage(3300);
                PMU.enableButtonBatteryCharge();
                PMU.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
            }
            return result;
        #endif
    }

    void setup() {
        #ifdef HAS_NO_GPS
            disableGPS = true;
        #else
            disableGPS = Config.disableGPS;
        #endif

        #ifdef HAS_AXP192
            Wire.begin(SDA, SCL);
            if (begin(Wire)) {
                ESP_LOGI(TAG, "init done!");
            } else {
                ESP_LOGE(TAG, "init failed!");
            }
            activateLoRa();
            if (disableGPS) {
                deactivateGPS();
            } else {
                activateGPS();
            }
            activateMeasurement();
            PMU.setChargerTerminationCurr(XPOWERS_AXP192_CHG_ITERM_LESS_10_PERCENT);
            PMU.setChargeTargetVoltage(XPOWERS_AXP192_CHG_VOL_4V2);
            PMU.setChargerConstantCurr(XPOWERS_AXP192_CHG_CUR_780MA);
            PMU.setSysPowerDownVoltage(2600);
        #endif

        #ifdef HAS_AXP2101
            bool beginStatus = false;
            #ifdef TTGO_T_Beam_S3_SUPREME_V3
                Wire1.begin(I2C1_SDA, I2C1_SCL);
                Wire.begin(I2C0_SDA, I2C0_SCL);
                if (begin(Wire1)) beginStatus = true;
            #else
                Wire.begin(SDA, SCL);
                if (begin(Wire)) beginStatus = true;
            #endif
            if (beginStatus) {
                ESP_LOGI(TAG, "init done!");
            } else {
                ESP_LOGE(TAG, "init failed!");
            }
            activateLoRa();
            if (disableGPS) {
                deactivateGPS();
            } else {
                activateGPS();
            }
            activateMeasurement();
            PMU.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_200MA);
            PMU.setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA);
            PMU.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);
            PMU.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_800MA);
            PMU.setSysPowerDownVoltage(2600);
        #endif

        #ifdef BATTERY_PIN
            pinMode(BATTERY_PIN, INPUT);
        #endif

        #ifdef VEXT_CTRL
            pinMode(VEXT_CTRL,OUTPUT);
            vext_ctrl_ON();
        #endif
        
        #ifdef ADC_CTRL
            pinMode(ADC_CTRL, OUTPUT);
        #endif

        #ifdef HELTEC_WIRELESS_TRACKER
            Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);
        #endif

        #if defined(HELTEC_V3_GPS) || defined(HELTEC_V3_TNC) || defined(HELTEC_V3_2_GPS) || defined(HELTEC_V3_2_TNC) || defined(HELTEC_WSL_V3_GPS_DISPLAY)
            Wire1.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);
        #endif

        #if defined(TTGO_T_DECK_GPS) || defined(TTGO_T_DECK_PLUS)
            pinMode(BOARD_POWERON, OUTPUT);
            digitalWrite(BOARD_POWERON, HIGH);

            pinMode(BOARD_SDCARD_CS, OUTPUT);
            pinMode(RADIO_CS_PIN, OUTPUT);
            pinMode(TFT_CS, OUTPUT);

            digitalWrite(BOARD_SDCARD_CS, HIGH);
            digitalWrite(RADIO_CS_PIN, HIGH);
            digitalWrite(TFT_CS, HIGH);

            delay(500);
            Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);
        #endif
    }

    void lowerCpuFrequency() {
        if (setCpuFrequencyMhz(80)) {
            ESP_LOGI(TAG, "CPU frequency lowered to %d MHz", getCpuFrequencyMhz());
        } else {
            ESP_LOGW(TAG, "CPU frequency unchanged");
        }
    }

    void shutdown() {
        // Disable watchdog before shutdown
        esp_task_wdt_delete(NULL);
        ESP_LOGI(TAG, "Watchdog disabled for shutdown");

        delay(3000);
        ESP_LOGW(TAG, "SHUTDOWN !!!");
        #if defined(HAS_AXP192) || defined(HAS_AXP2101)
            if (Config.notification.shutDownBeep) NOTIFICATION_Utils::shutDownBeep();
            displayToggle(false);
            PMU.shutdown();
        #else
            if (Config.bluetooth.active && Config.bluetooth.useBLE) {
                BLE_Utils::stop();
            } /*else {
                // turn off BT classic ???
            }*/

            #ifdef VEXT_CTRL
                vext_ctrl_OFF();
            #endif

            #ifdef ADC_CTRL
                adc_ctrl_OFF();
            #endif

            #if defined(TTGO_T_DECK_GPS) || defined(TTGO_T_DECK_PLUS)
                digitalWrite(BOARD_POWERON, LOW);
            #endif

            LoRa_Utils::sleepRadio();

            long DEEP_SLEEP_TIME_SEC = 1296000; // 15 days
            esp_sleep_enable_timer_wakeup(1000000ULL * DEEP_SLEEP_TIME_SEC);
            delay(500);
            esp_deep_sleep_start();
        #endif
    }

}