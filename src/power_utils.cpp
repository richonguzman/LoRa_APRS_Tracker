#include <SPI.h>
#include "notification_utils.h"
#include "configuration.h"
#include "board_pinout.h"
#include "power_utils.h"
#include "lora_utils.h"
#include "ble_utils.h"
#include "gps_utils.h"
#include "display.h"
#include "logger.h"


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


extern Configuration    Config;
extern logging::Logger  logger;
extern bool             transmitFlag;
extern bool             gpsIsActive;

uint32_t    batteryMeasurmentTime   = 0;

bool        pmuInterrupt;
float       lora32BatReadingCorr    = 6.5; // % of correction to higher value to reflect the real battery voltage (adjust this to your needs)
bool        disableGPS;


namespace POWER_Utils {

    bool   BatteryIsConnected = false;
    String batteryVoltage = "";
    String batteryChargeDischargeCurrent = "";

    double getBatteryVoltage() {
    #if defined(HAS_AXP192) || defined(HAS_AXP2101)
        return (PMU.getBattVoltage() / 1000.0);
    #else
        #ifdef BATTERY_PIN
            #ifdef ADC_CTRL
                #if defined(HELTEC_WIRELESS_TRACKER) || defined(HELTEC_V3_2_GPS) || defined(HELTEC_V3_2_TNC)
                    digitalWrite(ADC_CTRL, HIGH);
                #endif
                #if defined(HELTEC_V3_GPS) || defined(HELTEC_V3_TNC) || defined(HELTEC_V2_GPS) || defined(HELTEC_V2_GPS_915) || defined(HELTEC_V2_TNC) || defined(HELTEC_WSL_V3_GPS_DISPLAY)
                    digitalWrite(ADC_CTRL, LOW);
                #endif
            #endif
                int adc_value = analogRead(BATTERY_PIN);
            #ifdef ADC_CTRL
                #if defined(HELTEC_WIRELESS_TRACKER) || defined(HELTEC_V3_2_GPS) || defined(HELTEC_V3_2_TNC)
                    digitalWrite(ADC_CTRL, LOW);
                #endif
                #if defined(HELTEC_V3_GPS) || defined(HELTEC_V3_TNC) || defined(HELTEC_V2_GPS) || defined(HELTEC_V2_GPS_915) || defined(HELTEC_V2_TNC) || defined(HELTEC_WSL_V3_GPS_DISPLAY)
                    digitalWrite(ADC_CTRL, HIGH);
                #endif
                batteryMeasurmentTime = millis();
            #endif

            double voltage = (adc_value * 3.3 ) / 4095.0;
            
            #ifdef LIGHTTRACKER_PLUS_1_0
                double inputDivider = (1.0 / (560.0 + 100.0)) * 100.0;  // The voltage divider is a 560k + 100k resistor in series, 100k on the low side.
                return (voltage / inputDivider) + 0.1;
            #endif
            #if defined(TTGO_T_Beam_V0_7) || defined(TTGO_T_LORA32_V2_1_GPS) || defined(TTGO_T_LORA32_V2_1_GPS_915) || defined(TTGO_T_LORA32_V2_1_TNC) || defined(TTGO_T_LORA32_V2_1_TNC_915) || defined(ESP32_DIY_LoRa_GPS) || defined(ESP32_DIY_LoRa_GPS_915) || defined(ESP32_DIY_1W_LoRa_GPS) || defined(ESP32_DIY_1W_LoRa_GPS_915) || defined(ESP32_DIY_1W_LoRa_GPS_LLCC68) || defined(OE5HWN_MeshCom) || defined(TTGO_T_DECK_GPS) || defined(TTGO_T_DECK_PLUS) || defined(ESP32S3_DIY_LoRa_GPS) || defined(ESP32S3_DIY_LoRa_GPS_915) || defined(TROY_LoRa_APRS)
                return (2 * (voltage + 0.1)) * (1 + (lora32BatReadingCorr/100)); // (2 x 100k voltage divider) 2 x voltage divider/+0.1 because ESP32 nonlinearity ~100mV ADC offset/extra correction
            #endif
            #if defined(HELTEC_V3_GPS) || defined(HELTEC_V3_TNC) || defined(HELTEC_V3_2_GPS) || defined(HELTEC_V3_2_TNC) || defined(HELTEC_WIRELESS_TRACKER) || defined(HELTEC_WSL_V3_GPS_DISPLAY) || defined(ESP32_C3_DIY_LoRa_GPS) || defined(ESP32_C3_DIY_LoRa_GPS_915) || defined(WEMOS_ESP32_Bat_LoRa_GPS)
                double inputDivider = (1.0 / (390.0 + 100.0)) * 100.0;  // The voltage divider is a 390k + 100k resistor in series, 100k on the low side. 
                return (voltage / inputDivider) + 0.285; // Yes, this offset is excessive, but the ADC on the ESP32s3 is quite inaccurate and noisy. Adjust to own measurements.
            #endif
            #if defined(HELTEC_V2_GPS) || defined(HELTEC_V2_GPS_915) || defined(HELTEC_V2_TNC) || defined(F4GOH_1W_LoRa_Tracker)
                double inputDivider = (1.0 / (220.0 + 100.0)) * 100.0;  // The voltage divider is a 220k + 100k resistor in series, 100k on the low side. 
                return (voltage / inputDivider) + 0.285; // Yes, this offset is excessive, but the ADC on the ESP32 is quite inaccurate and noisy. Adjust to own measurements.
            #endif
        #else
            return 0.0;
        #endif
    #endif    
    }

    const String getBatteryInfoVoltage() {
        return batteryVoltage;
    }

    const String getBatteryInfoCurrent() {
        return batteryChargeDischargeCurrent;
    }

    bool getBatteryInfoIsConnected() {
        return BatteryIsConnected;
    }

    void enableChgLed() {
        #if defined(HAS_AXP192) || defined(HAS_AXP2101)
            PMU.setChargingLedMode(XPOWERS_CHG_LED_ON);
        #endif
    }

    void disableChgLed() {
        #if defined(HAS_AXP192) || defined(HAS_AXP2101)
            PMU.setChargingLedMode(XPOWERS_CHG_LED_OFF);
        #endif
        }

    bool isCharging() {
        #if defined(HAS_AXP192) || defined(HAS_AXP2101)
            return PMU.isCharging();
        #else
            return 0;
        #endif
    }

    void handleChargingLed() {
        if (isCharging()) {
            enableChgLed();
        } else {
            disableChgLed();
        }
    }

    double getBatteryChargeDischargeCurrent() {
        #if !defined(HAS_AXP192) && !defined(HAS_AXP2101)
            return 0;
        #endif
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

    bool isBatteryConnected() {
        #if defined(HAS_AXP192) || defined(HAS_AXP2101)
            return PMU.isBatteryConnect();
        #else
            if(getBatteryVoltage() > 1.0) {
                return true;
            } else {
                return false;
            }
        #endif
    }

    void obtainBatteryInfo() {
        static unsigned int rate_limit_check_battery = 0;
        if (!(rate_limit_check_battery++ % 60)) BatteryIsConnected = isBatteryConnected();
        if (BatteryIsConnected) {
            batteryVoltage                  = String(getBatteryVoltage(), 2);
            batteryChargeDischargeCurrent   = String(getBatteryChargeDischargeCurrent(), 0);
        }
    }

    void batteryManager() {
        #ifdef ADC_CTRL
            if (batteryMeasurmentTime == 0 || (millis() - batteryMeasurmentTime) > 30 * 1000) obtainBatteryInfo();
        #else
            obtainBatteryInfo();
        #endif
        #if defined(HAS_AXP192) || defined(HAS_AXP2101)
            handleChargingLed();
        #endif
    }

    void activateMeasurement() {
        #if defined(HAS_AXP192) || defined(HAS_AXP2101)
            PMU.disableTSPinMeasure();
            PMU.enableBattDetection();
            PMU.enableVbusVoltageMeasure();
            PMU.enableBattVoltageMeasure();
            PMU.enableSystemVoltageMeasure();
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
            digitalWrite(VEXT_CTRL, HIGH);
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
            digitalWrite(VEXT_CTRL, LOW);
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
        if (Config.notification.buzzerActive && Config.notification.buzzerPinTone >= 0 && Config.notification.buzzerPinVcc >= 0) {
            pinMode(Config.notification.buzzerPinTone, OUTPUT);
            pinMode(Config.notification.buzzerPinVcc, OUTPUT);
            if (Config.notification.bootUpBeep) NOTIFICATION_Utils::start();
        } else if (Config.notification.buzzerActive && (Config.notification.buzzerPinTone < 0 || Config.notification.buzzerPinVcc < 0)) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "PINOUT", "Buzzer Pins not defined");
            while (1);
        }

        if (Config.notification.ledTx && Config.notification.ledTxPin >= 0) {
            pinMode(Config.notification.ledTxPin, OUTPUT);
        } else if (Config.notification.ledTx && Config.notification.ledTxPin < 0) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "PINOUT", "Led Tx Pin not defined");
            while (1);
        }

        if (Config.notification.ledMessage && Config.notification.ledMessagePin >= 0) {
            pinMode(Config.notification.ledMessagePin, OUTPUT);
        } else if (Config.notification.ledMessage && Config.notification.ledMessagePin < 0) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "PINOUT", "Led Message Pin not defined");
            while (1);
        }

        if (Config.notification.ledFlashlight && Config.notification.ledFlashlightPin >= 0) {
            pinMode(Config.notification.ledFlashlightPin, OUTPUT);
        } else if (Config.notification.ledFlashlight && Config.notification.ledFlashlightPin < 0) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "PINOUT", "Led Flashlight Pin not defined");
            while (1);
        }

        if (Config.ptt.active && Config.ptt.io_pin >= 0) {
            pinMode(Config.ptt.io_pin, OUTPUT);
            digitalWrite(Config.ptt.io_pin, Config.ptt.reverse ? HIGH : LOW);
        } else if (Config.ptt.active && Config.ptt.io_pin < 0) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "PINOUT", "PTT Pin not defined");
            while (1);
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
            if (Config.wifiAP.active) {
                disableGPS = true;
            } else {
                disableGPS = Config.disableGPS;
            }            
        #endif

        #ifdef HAS_AXP192
            Wire.begin(SDA, SCL);
            if (begin(Wire)) {
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "AXP192", "init done!");
            } else {
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "AXP192", "init failed!");
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
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "AXP2101", "init done!");
            } else {
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "AXP2101", "init failed!");
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
            #if defined(HELTEC_V3_GPS) || defined(HELTEC_V3_TNC) || defined(HELTEC_WIRELESS_TRACKER) || defined(HELTEC_WSL_V3_GPS_DISPLAY)
                digitalWrite(VEXT_CTRL, HIGH);   // HWT needs this for GPS and TFT Screen
            #endif
            #if defined(HELTEC_V3_2_GPS) || defined(HELTEC_V3_2_TNC)
                digitalWrite(VEXT_CTRL, LOW);
            #endif
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
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Main", "CPU frequency set to 80MHz");
        } else {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "Main", "CPU frequency unchanged");
        }
    }

    void shutdown() {
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "Main", "SHUTDOWN !!!");
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
                #if defined(HELTEC_V3_GPS) || defined(HELTEC_V3_TNC) || defined(HELTEC_WIRELESS_TRACKER) || defined(HELTEC_WSL_V3_GPS_DISPLAY)
                    digitalWrite(VEXT_CTRL, LOW);
                #endif
                #if defined(HELTEC_V3_2_GPS) || defined(HELTEC_V3_2_TNC)
                    digitalWrite(VEXT_CTRL, HIGH);
                #endif
            #endif

            #ifdef ADC_CTRL
                #if defined(HELTEC_WIRELESS_TRACKER) || defined(HELTEC_V3_2_GPS) || defined(HELTEC_V3_2_TNC)
                    digitalWrite(ADC_CTRL, LOW);
                #endif
                #if defined(HELTEC_V3_GPS) || defined(HELTEC_V3_TNC) || defined(HELTEC_WSL_V3_GPS_DISPLAY)
                    digitalWrite(ADC_CTRL, HIGH);
                #endif
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