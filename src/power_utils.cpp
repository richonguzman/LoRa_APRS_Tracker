 #include "configuration.h"
#include "power_utils.h"
#include "pins_config.h"
#include "logger.h"

#ifndef TTGO_T_Beam_S3_SUPREME_V3
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

#if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
XPowersAXP192 PMU;
#endif
#if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3)
XPowersAXP2101 PMU;
#endif


extern Configuration    Config;
extern logging::Logger  logger;
extern bool             disableGPS;

bool    pmuInterrupt;
float   lora32BatReadingCorr = 6.5; // % of correction to higher value to reflect the real battery voltage (adjust this to your needs)

namespace POWER_Utils {

  bool   BatteryIsConnected = false;
  String batteryVoltage = "";
  String batteryChargeDischargeCurrent = "";

  double getBatteryVoltage() {
    #if defined(TTGO_T_Beam_V0_7) || defined(ESP32_DIY_LoRa_GPS) || defined(TTGO_T_LORA32_V2_1_GPS) || defined(TTGO_T_LORA32_V2_1_TNC) || defined(ESP32_DIY_1W_LoRa_GPS)
    int adc_value;
    double voltage;
    adc_value = analogRead(35);
    voltage = (adc_value * 3.3 ) / 4095.0;  // the battery voltage is divided by 2 with two 100kOhm resistors and connected to ADC1 Channel 7 -> pin 35
    return (2 * (voltage + 0.1)) * (1 + (lora32BatReadingCorr/100)); // 2 x voltage divider/+0.1 because ESP32 nonlinearity ~100mV ADC offset/extra correction
    #endif
    #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268) || defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3)
    return PMU.getBattVoltage() / 1000.0;
    #endif
    #if defined(HELTEC_V3_GPS)
    return 0; // Add measurement later
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
    #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268) || defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3)
    PMU.setChargingLedMode(XPOWERS_CHG_LED_ON);
    #endif
  }

  void disableChgLed() {
    #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268) || defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3)
    PMU.setChargingLedMode(XPOWERS_CHG_LED_OFF);
    #endif
  }  

  bool isCharging() {
    #if defined(TTGO_T_Beam_V0_7) || defined(ESP32_DIY_LoRa_GPS) || defined(TTGO_T_LORA32_V2_1_GPS) || defined(TTGO_T_LORA32_V2_1_TNC) || defined(ESP32_DIY_1W_LoRa_GPS) || defined(HELTEC_V3_GPS)
    return 0;
    #endif
    #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268) || defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3)
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
    #if defined(TTGO_T_Beam_V0_7) || defined(ESP32_DIY_LoRa_GPS) || defined(TTGO_T_LORA32_V2_1_GPS) || defined(TTGO_T_LORA32_V2_1_TNC) || defined(ESP32_DIY_1W_LoRa_GPS) || defined(HELTEC_V3_GPS)
    return 0;
    #endif
    #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
    if (PMU.isCharging()) {
      return PMU.getBatteryChargeCurrent();
    }
    return -1.0 * PMU.getBattDischargeCurrent();
    #endif
    #if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3)
    return PMU.getBatteryPercent();
    #endif
  }

  bool isBatteryConnected() {
    #if defined(TTGO_T_Beam_V0_7) || defined(ESP32_DIY_LoRa_GPS) || defined(TTGO_T_LORA32_V2_1_GPS) || defined(TTGO_T_LORA32_V2_1_TNC) || defined(ESP32_DIY_1W_LoRa_GPS) || defined(HELTEC_V3_GPS)
    if(getBatteryVoltage() > 1.0) {
      return true;
    } else {
      return false;
    }
    #endif
    #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268) || defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3)
    return PMU.isBatteryConnect();
    #endif
  }

  void obtainBatteryInfo() {
    static unsigned int rate_limit_check_battery = 0;
    if (!(rate_limit_check_battery++ % 60))
      BatteryIsConnected = isBatteryConnected();
    if (BatteryIsConnected) {
      #if defined(TTGO_T_Beam_V0_7) || defined(ESP32_DIY_LoRa_GPS) || defined(TTGO_T_LORA32_V2_1_GPS) || defined(TTGO_T_LORA32_V2_1_TNC) || defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268) || defined(ESP32_DIY_1W_LoRa_GPS)
      batteryVoltage       = String(getBatteryVoltage(), 2);
      #endif
      #if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3)
      batteryVoltage       = String(PMU.getBattVoltage());
      #endif
      batteryChargeDischargeCurrent = String(getBatteryChargeDischargeCurrent(), 0);
    }
  }

  void batteryManager() {
    obtainBatteryInfo();
    #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268) || defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3)
    handleChargingLed();
    #endif
  }

  void activateMeasurement() {
    #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268) || defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3)
    PMU.disableTSPinMeasure();
    PMU.enableBattDetection();
    PMU.enableVbusVoltageMeasure();
    PMU.enableBattVoltageMeasure();
    PMU.enableSystemVoltageMeasure();
    #endif
  }

  void activateGPS() {
    #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
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
    #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
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
    #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
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
    #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
    PMU.disableLDO2();
    #endif
    #if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262)
    PMU.disableALDO2();
    #endif
    #if defined(TTGO_T_Beam_S3_SUPREME_V3)
    PMU.disableALDO3();
    #endif
  }

  bool begin(TwoWire &port) {
    #if defined(TTGO_T_Beam_V0_7) || defined(ESP32_DIY_LoRa_GPS) || defined(TTGO_T_LORA32_V2_1_GPS) || defined(TTGO_T_LORA32_V2_1_TNC) || defined(ESP32_DIY_1W_LoRa_GPS) || defined(HELTEC_V3_GPS)
    return true; // nor powerManagment chip for this boards (only a few measure battery voltage).
    #endif
    #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
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
    Wire.end();
    #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
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
    
    #if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3)
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
  }

  void lowerCpuFrequency() {
    #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268) || defined(TTGO_T_Beam_V1_2) || defined(ESP32_DIY_LoRa_GPS) || defined(TTGO_T_LORA32_V2_1_GPS) || defined(TTGO_T_LORA32_V2_1_TNC) || defined(ESP32_DIY_1W_LoRa_GPS) || defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3)
    if (setCpuFrequencyMhz(80)) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "CPU frequency set to 80MHz");
    } else {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "Main", "CPU frequency unchanged");
    }
    #endif
  }

  void shutdown() {
    #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268) || defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3)
    PMU.shutdown();
    #endif
  }

}