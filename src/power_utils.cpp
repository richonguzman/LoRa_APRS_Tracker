#include "configuration.h"
#include "power_utils.h"
#include "logger.h"

#define I2C_SDA 21
#define I2C_SCL 22

extern Configuration    Config;
extern logging::Logger  logger;
extern bool             disableGPS;

float lora32BatReadingCorr = 6.5; // % of correction to higher value to reflect the real battery voltage (adjust this to your needs)

// cppcheck-suppress unusedFunction
bool PowerManagement::begin(TwoWire &port) {
  #if defined(TTGO_T_Beam_V0_7) || defined(ESP32_DIY_LoRa_GPS) || defined(TTGO_T_LORA_V2_1_GPS) || defined(TTGO_T_LORA_V2_1_TNC) || defined(ESP32_DIY_1W_LoRa_GPS)
  return true; // Currently there is no init for V0.7 boards(they only measure battery voltage).
  #endif
  #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
  bool result = axp.begin(port, AXP192_SLAVE_ADDRESS);
  if (!result) {
    axp.setDCDC1Voltage(3300);
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
  }
  return result;
  #endif
}

// cppcheck-suppress unusedFunction
void PowerManagement::activateLoRa() {
  #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
  axp.setPowerOutPut(AXP192_LDO2, AXP202_ON);
  #endif
  #if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262)
  PMU.setALDO2Voltage(3300);
  PMU.enableALDO2();
  #endif
}

// cppcheck-suppress unusedFunction
void PowerManagement::deactivateLoRa() {
  #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
  axp.setPowerOutPut(AXP192_LDO2, AXP202_OFF);
  #endif
  #if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262)
  PMU.disableALDO2();
  #endif
}

// cppcheck-suppress unusedFunction
void PowerManagement::activateGPS() {
  #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
  axp.setPowerOutPut(AXP192_LDO3, AXP202_ON);
  #endif
  #if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262)
  PMU.setALDO3Voltage(3300);
  PMU.enableALDO3(); 
    #endif
}

// cppcheck-suppress unusedFunction
void PowerManagement::deactivateGPS() {
  #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
  axp.setPowerOutPut(AXP192_LDO3, AXP202_OFF);
  #endif
  #if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262)
  PMU.disableALDO3();
  #endif
}

// cppcheck-suppress unusedFunction
void PowerManagement::activateOLED() {
  #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
  axp.setPowerOutPut(AXP192_DCDC1, AXP202_ON);
  #endif
}

// cppcheck-suppress unusedFunction
void PowerManagement::decativateOLED() {
  #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
  axp.setPowerOutPut(AXP192_DCDC1, AXP202_OFF);
  #endif
}

// cppcheck-suppress unusedFunction
void PowerManagement::disableChgLed() {
  #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
  axp.setChgLEDMode(AXP20X_LED_OFF);
  #endif
}

// cppcheck-suppress unusedFunction
void PowerManagement::enableChgLed() {
  #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
  axp.setChgLEDMode(AXP20X_LED_LOW_LEVEL);
  #endif
}

// cppcheck-suppress unusedFunction
void PowerManagement::activateMeasurement() {
  #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
  axp.adc1Enable(AXP202_BATT_CUR_ADC1 | AXP202_BATT_VOL_ADC1, true);
  #endif
  #if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262)
  PMU.enableBattDetection();
  PMU.enableVbusVoltageMeasure();
  PMU.enableBattVoltageMeasure();
  PMU.enableSystemVoltageMeasure();
  #endif
}

// cppcheck-suppress unusedFunction
void PowerManagement::deactivateMeasurement() {
  #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
  axp.adc1Enable(AXP202_BATT_CUR_ADC1 | AXP202_BATT_VOL_ADC1, false);
  #endif
}

// cppcheck-suppress unusedFunction
double PowerManagement::getBatteryVoltage() {
  #if defined(TTGO_T_Beam_V0_7) || defined(ESP32_DIY_LoRa_GPS) || defined(TTGO_T_LORA_V2_1_GPS) || defined(TTGO_T_LORA_V2_1_TNC) || defined(ESP32_DIY_1W_LoRa_GPS)
  int adc_value;
  double voltage;
  adc_value = analogRead(35);
  voltage = (adc_value * 3.3 ) / 4095.0;  // the battery voltage is divided by 2 with two 100kOhm resistors and connected to ADC1 Channel 7 -> pin 35
  return (2 * (voltage + 0.1)) * (1 + (lora32BatReadingCorr/100)); // 2 x voltage divider/+0.1 because ESP32 nonlinearity ~100mV ADC offset/extra correction
  #endif
  #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
  return axp.getBattVoltage() / 1000.0;
  #endif
  #if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262)
  return PMU.getBattVoltage() / 1000.0;
  #endif
}

// cppcheck-suppress unusedFunction
double PowerManagement::getBatteryChargeDischargeCurrent() {
  #if defined(TTGO_T_Beam_V0_7) || defined(ESP32_DIY_LoRa_GPS) || defined(TTGO_T_LORA_V2_1_GPS) || defined(TTGO_T_LORA_V2_1_TNC) || defined(ESP32_DIY_1W_LoRa_GPS)
  return 0;
  #endif
  #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
  if (axp.isChargeing()) {
    return axp.getBattChargeCurrent();
  }
  return -1.0 * axp.getBattDischargeCurrent();
  #endif
  #if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262)
  return PMU.getBatteryPercent();
  #endif
}

bool PowerManagement::isBatteryConnected() {
  #if defined(TTGO_T_Beam_V0_7) || defined(ESP32_DIY_LoRa_GPS) || defined(TTGO_T_LORA_V2_1_GPS) || defined(TTGO_T_LORA_V2_1_TNC) || defined(ESP32_DIY_1W_LoRa_GPS)
  if(getBatteryVoltage() > 1.0) {
    return true;
  } else {
    return false;
  }
  #endif
  #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
  return axp.isBatteryConnect();
  #endif
  #if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262)
  return PMU.isBatteryConnect();
  #endif
}

bool PowerManagement::isChargeing() {
  #if defined(TTGO_T_Beam_V0_7) || defined(ESP32_DIY_LoRa_GPS) || defined(TTGO_T_LORA_V2_1_GPS) || defined(TTGO_T_LORA_V2_1_TNC) || defined(ESP32_DIY_1W_LoRa_GPS)
  return 0;
  #endif
  #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
  return axp.isChargeing();
  #endif
  #if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262)
  return PMU.isCharging();
  #endif
}

void PowerManagement::setup() {
  Wire.end();
  #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
  Wire.begin(SDA, SCL);
  if (!begin(Wire)) {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "AXP192", "init done!");
  } else {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "AXP192", "init failed!");
  }
  activateLoRa();
  activateOLED();
  if (disableGPS) {
    deactivateGPS();
  } else {
    activateGPS();
  }
  activateMeasurement();
  #endif
  #if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262)
  Wire.begin(SDA, SCL);
  if (begin(Wire)) {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "AXP2101", "init done!");
  } else {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "AXP2101", "init failed!");
  }
  activateLoRa();
  activateOLED();
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

void PowerManagement::lowerCpuFrequency() {
  #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268) || defined(TTGO_T_Beam_V1_2) || defined(ESP32_DIY_LoRa_GPS) || defined(TTGO_T_LORA_V2_1_GPS) || defined(TTGO_T_LORA_V2_1_TNC) || defined(ESP32_DIY_1W_LoRa_GPS) || defined(TTGO_T_Beam_V1_2_SX1262)
  if (setCpuFrequencyMhz(80)) {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "CPU frequency set to 80MHz");
  } else {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "Main", "CPU frequency unchanged");
  }
  #endif
}

void PowerManagement::handleChargingLed() {
  if (isChargeing()) {
    enableChgLed();
  } else {
    disableChgLed();
  }
}

void PowerManagement::obtainBatteryInfo() {
  static unsigned int rate_limit_check_battery = 0;
  if (!(rate_limit_check_battery++ % 60))
    BatteryIsConnected = isBatteryConnected();
  if (BatteryIsConnected) {
    #if defined(TTGO_T_Beam_V0_7) || defined(ESP32_DIY_LoRa_GPS) || defined(TTGO_T_LORA_V2_1_GPS) || defined(TTGO_T_LORA_V2_1_TNC) || defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268) || defined(ESP32_DIY_1W_LoRa_GPS)
    batteryVoltage       = String(getBatteryVoltage(), 2);
    #endif
    #if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262)
    batteryVoltage       = String(PMU.getBattVoltage());
    #endif
    batteryChargeDischargeCurrent = String(getBatteryChargeDischargeCurrent(), 0);
  }
}

String PowerManagement::getBatteryInfoVoltage() {
  return batteryVoltage;
}

String PowerManagement::getBatteryInfoCurrent() {
  return batteryChargeDischargeCurrent;
}

bool PowerManagement::getBatteryInfoIsConnected() {
  return BatteryIsConnected;
}

void PowerManagement::batteryManager() {
  obtainBatteryInfo();
  #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268) || defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262)
  handleChargingLed();
  #endif
}

void PowerManagement::shutdown() {
  #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
  axp.shutdown();
  #endif
  #if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262)
  PMU.shutdown();
  #endif
}