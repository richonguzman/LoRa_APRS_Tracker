#include "configuration.h"
#include "power_utils.h"
#include "logger.h"

#define I2C_SDA 21
#define I2C_SCL 22

extern Configuration    Config;
extern logging::Logger  logger;

// cppcheck-suppress unusedFunction
bool PowerManagement::begin(TwoWire &port) {
#ifdef TTGO_T_Beam_V0_7
  bool result = true;
  return result;
#endif
#if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_LORA_V2_1) || defined(TTGO_T_Beam_V1_0_SX1268)
  bool result = axp.begin(port, AXP192_SLAVE_ADDRESS);
  if (!result) {
    axp.setDCDC1Voltage(3300);
  }
  return result;
#endif
#ifdef TTGO_T_Beam_V1_2
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
#if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_LORA_V2_1) || defined(TTGO_T_Beam_V1_0_SX1268)
  axp.setPowerOutPut(AXP192_LDO2, AXP202_ON);
#endif
#ifdef TTGO_T_Beam_V1_2
  PMU.setALDO2Voltage(3300);
  PMU.enableALDO2();
#endif
}

// cppcheck-suppress unusedFunction
void PowerManagement::deactivateLoRa() {
#if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_LORA_V2_1) || defined(TTGO_T_Beam_V1_0_SX1268)
  axp.setPowerOutPut(AXP192_LDO2, AXP202_OFF);
#endif
#ifdef TTGO_T_Beam_V1_2
  PMU.disableALDO2();
#endif
}

// cppcheck-suppress unusedFunction
void PowerManagement::activateGPS() {
#if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_LORA_V2_1) || defined(TTGO_T_Beam_V1_0_SX1268)
  axp.setPowerOutPut(AXP192_LDO3, AXP202_ON);
#endif
#ifdef TTGO_T_Beam_V1_2
  PMU.setALDO3Voltage(3300);
  PMU.enableALDO3(); 
#endif
}

// cppcheck-suppress unusedFunction
void PowerManagement::deactivateGPS() {
#if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_LORA_V2_1) || defined(TTGO_T_Beam_V1_0_SX1268)
  axp.setPowerOutPut(AXP192_LDO3, AXP202_OFF);
#endif
#ifdef TTGO_T_Beam_V1_2
  PMU.disableALDO3();
#endif
}

// cppcheck-suppress unusedFunction
void PowerManagement::activateOLED() {
#if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_LORA_V2_1) || defined(TTGO_T_Beam_V1_0_SX1268)
  axp.setPowerOutPut(AXP192_DCDC1, AXP202_ON);
#endif
}

// cppcheck-suppress unusedFunction
void PowerManagement::decativateOLED() {
#if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_LORA_V2_1) || defined(TTGO_T_Beam_V1_0_SX1268)
  axp.setPowerOutPut(AXP192_DCDC1, AXP202_OFF);
#endif
}

// cppcheck-suppress unusedFunction
void PowerManagement::disableChgLed() {
#if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_LORA_V2_1) || defined(TTGO_T_Beam_V1_0_SX1268)
  axp.setChgLEDMode(AXP20X_LED_OFF);
#endif
}

// cppcheck-suppress unusedFunction
void PowerManagement::enableChgLed() {
#if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_LORA_V2_1) || defined(TTGO_T_Beam_V1_0_SX1268)
  axp.setChgLEDMode(AXP20X_LED_LOW_LEVEL);
#endif
}

// cppcheck-suppress unusedFunction
void PowerManagement::activateMeasurement() {
#if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_LORA_V2_1) || defined(TTGO_T_Beam_V1_0_SX1268)
  axp.adc1Enable(AXP202_BATT_CUR_ADC1 | AXP202_BATT_VOL_ADC1, true);
#endif
#ifdef TTGO_T_Beam_V1_2
  PMU.enableBattDetection();
  PMU.enableVbusVoltageMeasure();
  PMU.enableBattVoltageMeasure();
  PMU.enableSystemVoltageMeasure();
#endif

}

// cppcheck-suppress unusedFunction
void PowerManagement::deactivateMeasurement() {
#if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_LORA_V2_1) || defined(TTGO_T_Beam_V1_0_SX1268)
  axp.adc1Enable(AXP202_BATT_CUR_ADC1 | AXP202_BATT_VOL_ADC1, false);
#endif
}

// cppcheck-suppress unusedFunction
double PowerManagement::getBatteryVoltage() {
#ifdef TTGO_T_Beam_V0_7
  return 0;
#endif
#if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_LORA_V2_1) || defined(TTGO_T_Beam_V1_0_SX1268)
  return axp.getBattVoltage() / 1000.0;
#endif
#ifdef TTGO_T_Beam_V1_2
  return PMU.getBattVoltage() / 1000.0;
#endif
}

// cppcheck-suppress unusedFunction
double PowerManagement::getBatteryChargeDischargeCurrent() {
#ifdef TTGO_T_Beam_V0_7
  return 0;
#endif
#if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_LORA_V2_1) || defined(TTGO_T_Beam_V1_0_SX1268)
  if (axp.isChargeing()) {
    return axp.getBattChargeCurrent();
  }
  return -1.0 * axp.getBattDischargeCurrent();
#endif
#ifdef TTGO_T_Beam_V1_2
  return PMU.getBatteryPercent();
#endif
}

bool PowerManagement::isBatteryConnected() {
#ifdef TTGO_T_Beam_V0_7
  return 0;
#endif
#if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_LORA_V2_1) || defined(TTGO_T_Beam_V1_0_SX1268)
  return axp.isBatteryConnect();
#endif
#ifdef TTGO_T_Beam_V1_2
  return PMU.isBatteryConnect();
#endif
}

bool PowerManagement::isChargeing() {
#ifdef TTGO_T_Beam_V0_7
  return 0;
#endif
#if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_LORA_V2_1) || defined(TTGO_T_Beam_V1_0_SX1268)
  return axp.isChargeing();
#endif
#ifdef TTGO_T_Beam_V1_2
  return PMU.isCharging();
#endif
}

void PowerManagement::setup() {
#if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_LORA_V2_1) || defined(TTGO_T_Beam_V1_0_SX1268)
  Wire.begin(SDA, SCL);
  if (!begin(Wire)) {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "AXP192", "init done!");
  } else {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "AXP192", "init failed!");
  }
  activateLoRa();
  activateOLED();
  if (Config.disableGps) {
    deactivateGPS();
  } else {
    activateGPS();
  }
  activateMeasurement();
#endif
#ifdef TTGO_T_Beam_V1_2
  Wire.begin(SDA, SCL);
  if (begin(Wire)) {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "AXP2101", "init done!");
  } else {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "AXP2101", "init failed!");
  }
  activateLoRa();
  activateOLED();
  if (Config.disableGps) {
    deactivateGPS();
  } else {
    activateGPS();
  }
  activateMeasurement();
  PMU.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);
  PMU.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_500MA);
  PMU.setSysPowerDownVoltage(2600);
#endif
}

void PowerManagement::lowerCpuFrequency() {
#if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_LORA_V2_1)  || defined(TTGO_T_Beam_V1_0_SX1268)
  if (setCpuFrequencyMhz(80)) {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "CPU frequency set to 80MHz");
  } else {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "CPU frequency unchanged");
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
    #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_LORA_V2_1) || defined(TTGO_T_Beam_V1_0_SX1268)
    batteryVoltage       = String(getBatteryVoltage(), 2);
    #endif
    #ifdef TTGO_T_Beam_V1_2
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
#if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_LORA_V2_1)  || defined(TTGO_T_Beam_V1_0_SX1268)
  obtainBatteryInfo();
  handleChargingLed();
#endif
}