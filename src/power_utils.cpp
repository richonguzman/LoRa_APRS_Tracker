#include <SPI.h>
#include "configuration.h"
#include "power_utils.h"
#include "notification_utils.h"
#include "boards_pinout.h"
#include "ble_utils.h"
#include "logger.h"

#ifndef TTGO_T_Beam_S3_SUPREME_V3
    #ifndef HELTEC_WIRELESS_TRACKER
        #define I2C_SDA 21
        #define I2C_SCL 22
        #define IRQ_PIN 35
    #endif
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
extern bool             disableGPS;

uint32_t    batteryMeasurmentTime   = 0;

bool        pmuInterrupt;
float       lora32BatReadingCorr    = 6.5; // % of correction to higher value to reflect the real battery voltage (adjust this to your needs)

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
                #ifdef HELTEC_WIRELESS_TRACKER
                    digitalWrite(ADC_CTRL, HIGH);
                #endif
                #ifdef HELTEC_V3_GPS
                    digitalWrite(ADC_CTRL, LOW);
                #endif
            #endif
                int adc_value = analogRead(BATTERY_PIN);
            #ifdef ADC_CTRL
                #ifdef HELTEC_WIRELESS_TRACKER
                    digitalWrite(ADC_CTRL, LOW);
                #endif
                #ifdef HELTEC_V3_GPS
                    digitalWrite(ADC_CTRL, HIGH);
                #endif
                batteryMeasurmentTime = millis();
            #endif
                double voltage = (adc_value * 3.3 ) / 4095.0;
            #if defined(TTGO_T_Beam_V0_7) || defined(TTGO_T_LORA32_V2_1_GPS) || defined(TTGO_T_LORA32_V2_1_TNC) || defined(ESP32_DIY_LoRa_GPS) || defined(ESP32_DIY_1W_LoRa_GPS) || defined(OE5HWN_MeshCom) || defined(TTGO_T_DECK_GPS)
                return (2 * (voltage + 0.1)) * (1 + (lora32BatReadingCorr/100)); // (2 x 100k voltage divider) 2 x voltage divider/+0.1 because ESP32 nonlinearity ~100mV ADC offset/extra correction
            #endif
            #if defined(HELTEC_V3_GPS) || defined(HELTEC_WIRELESS_TRACKER) || defined(ESP32_C3_DIY_LoRa_GPS)
                double inputDivider = (1.0 / (390.0 + 100.0)) * 100.0;  // The voltage divider is a 390k + 100k resistor in series, 100k on the low side. 
                return (voltage / inputDivider) + 0.285; // Yes, this offset is excessive, but the ADC on the ESP32s3 is quite inaccurate and noisy. Adjust to own measurements.
            #endif
        #else
            return 0.0;
        #endif
    #endif    
    }

    String getBatteryInfoVoltage() {
        return batteryVoltage;
    }

    String getBatteryInfoCurrent() {
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
        #if defined(TTGO_T_Beam_V0_7) || defined(ESP32_DIY_LoRa_GPS) || defined(TTGO_T_LORA32_V2_1_GPS) || defined(TTGO_T_LORA32_V2_1_TNC) || defined(ESP32_DIY_1W_LoRa_GPS) || defined(HELTEC_V3_GPS) || defined(OE5HWN_MeshCom) || defined(ESP32_C3_DIY_LoRa_GPS) || defined(HELTEC_WIRELESS_TRACKER) || defined(TTGO_T_DECK_GPS)
            return 0;
        #endif
        #if defined(HAS_AXP192) || defined(HAS_AXP2101)
            return PMU.isCharging();
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
        #if defined(TTGO_T_Beam_V0_7) || defined(ESP32_DIY_LoRa_GPS) || defined(TTGO_T_LORA32_V2_1_GPS) || defined(TTGO_T_LORA32_V2_1_TNC) || defined(ESP32_DIY_1W_LoRa_GPS) || defined(HELTEC_V3_GPS) || defined(OE5HWN_MeshCom) || defined(ESP32_C3_DIY_LoRa_GPS) || defined(HELTEC_WIRELESS_TRACKER) || defined(TTGO_T_DECK_GPS)
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
        #if defined(TTGO_T_Beam_V0_7) || defined(ESP32_DIY_LoRa_GPS) || defined(TTGO_T_LORA32_V2_1_GPS) || defined(TTGO_T_LORA32_V2_1_TNC) || defined(ESP32_DIY_1W_LoRa_GPS) || defined(HELTEC_V3_GPS) || defined(OE5HWN_MeshCom) || defined(ESP32_C3_DIY_LoRa_GPS) || defined(HELTEC_WIRELESS_TRACKER) || defined(TTGO_T_DECK_GPS)
            if(getBatteryVoltage() > 1.0) {
                return true;
            } else {
                return false;
            }
        #endif
        #if defined(HAS_AXP192) || defined(HAS_AXP2101)
            return PMU.isBatteryConnect();
        #endif
    }

    void obtainBatteryInfo() {
        static unsigned int rate_limit_check_battery = 0;
        if (!(rate_limit_check_battery++ % 60))
            BatteryIsConnected = isBatteryConnected();
        if (BatteryIsConnected) {
            #if defined(TTGO_T_Beam_V0_7) || defined(ESP32_DIY_LoRa_GPS) || defined(TTGO_T_LORA32_V2_1_GPS) || defined(TTGO_T_LORA32_V2_1_TNC) || defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268) || defined(ESP32_DIY_1W_LoRa_GPS) || defined(HELTEC_V3_GPS) || defined(HELTEC_WIRELESS_TRACKER) || defined(OE5HWN_MeshCom)  || defined(TTGO_T_DECK_GPS)
                batteryVoltage       = String(getBatteryVoltage(), 2);
            #endif
            #ifdef HAS_AXP2101
                batteryVoltage       = String(PMU.getBattVoltage());
            #endif
            batteryChargeDischargeCurrent = String(getBatteryChargeDischargeCurrent(), 0);
        }
    }

    void batteryManager() {
        #ifdef ADC_CTRL
            if(batteryMeasurmentTime == 0 || (millis() - batteryMeasurmentTime) > 30 * 1000) obtainBatteryInfo();
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
        #if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262)
            PMU.setALDO3Voltage(3300);
            PMU.enableALDO3(); 
        #endif
        #if defined(TTGO_T_Beam_S3_SUPREME_V3)
            PMU.setALDO4Voltage(3300);
            PMU.enableALDO4();
        #endif
    }

    void deactivateGPS() {
        #ifdef HAS_AXP192
            PMU.disableLDO3();
        #endif
        #if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262)
            PMU.disableALDO3();
        #endif
        #if defined(TTGO_T_Beam_S3_SUPREME_V3)
            PMU.disableALDO4();
        #endif
    }

    void activateLoRa() {
        #ifdef HAS_AXP192
            PMU.setLDO2Voltage(3300);
            PMU.enableLDO2();
        #endif
        #if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262)
            PMU.setALDO2Voltage(3300);
            PMU.enableALDO2();
        #endif
        #if defined(TTGO_T_Beam_S3_SUPREME_V3)
            PMU.setALDO3Voltage(3300);
            PMU.enableALDO3();
        #endif
    }

    void deactivateLoRa() {
        #ifdef HAS_AXP192
            PMU.disableLDO2();
        #endif
        #if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262)
            PMU.disableALDO2();
        #endif
        #if defined(TTGO_T_Beam_S3_SUPREME_V3)
            PMU.disableALDO3();
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
        #if defined(TTGO_T_Beam_V0_7) || defined(ESP32_DIY_LoRa_GPS) || defined(TTGO_T_LORA32_V2_1_GPS) || defined(TTGO_T_LORA32_V2_1_TNC) || defined(ESP32_DIY_1W_LoRa_GPS) || defined(HELTEC_V3_GPS) || defined(OE5HWN_MeshCom) || defined(ESP32_C3_DIY_LoRa_GPS) || defined(HELTEC_WIRELESS_TRACKER) || defined(TTGO_T_DECK_GPS)
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

        #if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262)
            bool result = PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL);
            if (result) {
                PMU.disableDC2();
                PMU.disableDC3();
                PMU.disableDC4();
                PMU.disableDC5();
                PMU.disableALDO1();
                PMU.disableALDO4();
                PMU.disableBLDO1();
                PMU.disableBLDO2();
                PMU.disableDLDO1();
                PMU.disableDLDO2();
                PMU.setDC1Voltage(3300);
                PMU.enableDC1();
                PMU.setButtonBatteryChargeVoltage(3300);
                PMU.enableButtonBatteryCharge();
                PMU.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
            }
            return result;
        #endif

        #if defined(TTGO_T_Beam_S3_SUPREME_V3)
            bool result = PMU.begin(Wire1, AXP2101_SLAVE_ADDRESS, I2C1_SDA, I2C1_SCL);
            if (result) {
                PMU.disableDC2();
                PMU.disableDC3();
                PMU.disableDC4();
                PMU.disableDC5();
                PMU.disableBLDO1();
                PMU.disableBLDO2();
                PMU.disableDLDO1();
                PMU.disableDLDO2();
                PMU.setDC1Voltage(3300);
                PMU.enableDC1();
                PMU.setALDO1Voltage(3300);
                PMU.setButtonBatteryChargeVoltage(3300);
                PMU.enableButtonBatteryCharge();
                PMU.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
            }
            return result;
        #endif
    }

    void setup() {
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

        #if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262)
            Wire.begin(SDA, SCL);
            if (begin(Wire)) {
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "AXP2101", "init done!");
            } else {
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "AXP2101", "init failed!");
            }
        #endif

        #if defined(TTGO_T_Beam_S3_SUPREME_V3)
            Wire1.begin(I2C1_SDA, I2C1_SCL);
            Wire.begin(I2C0_SDA, I2C0_SCL);
            if (begin(Wire1)) {
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "AXP2101", "init done!");
            } else {
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "AXP2101", "init failed!");
            }
        #endif
    
        #ifdef HAS_AXP2101
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
            pinMode(VEXT_CTRL,OUTPUT); // this is for GPS and TFT screen on Wireless_Tracker and only for Oled in Heltec V3
            digitalWrite(VEXT_CTRL, HIGH);
        #endif
        
        #ifdef ADC_CTRL
            pinMode(ADC_CTRL, OUTPUT);
        #endif

        #ifdef HELTEC_WIRELESS_TRACKER
            Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);
        #endif

        #ifdef HELTEC_V3_GPS
            Wire1.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);
        #endif

        #if defined(TTGO_T_DECK_GPS)
            pinMode(BOARD_POWERON, OUTPUT);
            digitalWrite(BOARD_POWERON, HIGH);

            pinMode(BOARD_SDCARD_CS, OUTPUT);
            pinMode(RADIO_CS_PIN, OUTPUT);
            pinMode(TFT_CS, OUTPUT);

            digitalWrite(BOARD_SDCARD_CS, HIGH);
            digitalWrite(RADIO_CS_PIN, HIGH);
            digitalWrite(TFT_CS, HIGH);
            

            pinMode(TrackBallCenter, INPUT_PULLUP);
            pinMode(TrackBallUp, INPUT_PULLUP);
            pinMode(TrackBallDown, INPUT_PULLUP);
            pinMode(TrackBallLeft, INPUT_PULLUP);
            pinMode(TrackBallRight, INPUT_PULLUP);

            delay(500);
            Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);
        #endif
    }

    void lowerCpuFrequency() {
        // later for all boards!
        #if defined(HAS_AXP192) || defined(HAS_AXP2101) || defined(ESP32_DIY_LoRa_GPS) || defined(TTGO_T_LORA32_V2_1_GPS) || defined(TTGO_T_LORA32_V2_1_TNC) || defined(ESP32_DIY_1W_LoRa_GPS) || defined(HELTEC_V3_GPS) || defined(OE5HWN_MeshCom) || defined(ESP32_C3_DIY_LoRa_GPS) || defined(HELTEC_WIRELESS_TRACKER) || defined(TTGO_T_DECK_GPS)
            if (setCpuFrequencyMhz(80)) {
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Main", "CPU frequency set to 80MHz");
            } else {
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "Main", "CPU frequency unchanged");
            }
        #endif
    }

    void shutdown() {
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "Main", "SHUTDOWN !!!");
        #if defined(HAS_AXP192) || defined(HAS_AXP2101)
            if (Config.notification.shutDownBeep) NOTIFICATION_Utils::shutDownBeep();
            PMU.shutdown();
        #else

            if (Config.bluetoothType == 0) {
                BLE_Utils::stop();
            } else {
                // turn off BT classic ???
            }

            #ifdef VEXT_CTRL
                digitalWrite(VEXT_CTRL, LOW);
            #endif

            #ifdef ADC_CTRL
                #ifdef HELTEC_WIRELESS_TRACKER
                    digitalWrite(ADC_CTRL, LOW);
                #endif
                #ifdef HELTEC_V3_GPS
                    digitalWrite(ADC_CTRL, HIGH);
                #endif
            #endif


            #ifdef HELTEC_WIRELESS_TRACKER
                /*Serial.flush();           // not working yet
                SPI.endTransaction();           
                SPI.end();
                pinMode(RADIO_DIO1_PIN, ANALOG);
                pinMode(RADIO_RST_PIN, ANALOG);
                pinMode(RADIO_BUSY_PIN, ANALOG);
                pinMode(RADIO_SCLK_PIN, ANALOG);
                pinMode(RADIO_MISO_PIN, ANALOG);
                pinMode(RADIO_MOSI_PIN, ANALOG);

                pinMode(RADIO_CS_PIN, OUTPUT);
                digitalWrite(RADIO_CS_PIN, HIGH);
                gpio_hold_en((gpio_num_t)RADIO_CS_PIN);*/
            #endif


            long DEEP_SLEEP_TIME_SEC = 1296000; // 30 days
            esp_sleep_enable_timer_wakeup(1000000ULL * DEEP_SLEEP_TIME_SEC);
            esp_deep_sleep_start();
        #endif
    }

}