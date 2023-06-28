#include "power_utils.h"
#include "logger.h"

#define I2C_SDA 21
#define I2C_SCL 22

extern logging::Logger logger;

// cppcheck-suppress unusedFunction
bool PowerManagement::begin(TwoWire &port) {
  #ifdef TTGO_T_Beam_V1_0
  bool result = axp.begin(port, AXP192_SLAVE_ADDRESS);
  if (!result) {
    axp.setDCDC1Voltage(3300);
  }
  return result;
  #endif
  #ifdef TTGO_T_Beam_V1_2
  bool result = PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL);
  if (!result) {
    PMU.setDC3Voltage(3000);
    PMU.enableDC3();
    PMU.setDC2Voltage(3000);
    PMU.enableDC2();
    PMU.setDC1Voltage(3000);
    PMU.enableDC1();
  }
  return result;
  #endif
}

// cppcheck-suppress unusedFunction
void PowerManagement::activateLoRa() {
  #ifdef TTGO_T_Beam_V1_0
  axp.setPowerOutPut(AXP192_LDO2, AXP202_ON);
  #endif
  #ifdef TTGO_T_Beam_V1_2
  PMU.setALDO2Voltage(3300);
  PMU.enableALDO2(); 
  #endif
}

// cppcheck-suppress unusedFunction
void PowerManagement::deactivateLoRa() {
  #ifdef TTGO_T_Beam_V1_0
  axp.setPowerOutPut(AXP192_LDO2, AXP202_OFF);
  #endif
  #ifdef TTGO_T_Beam_V1_2
  PMU.disableALDO2();
  #endif
}

// cppcheck-suppress unusedFunction
void PowerManagement::activateGPS() {
  #ifdef TTGO_T_Beam_V1_0
  axp.setPowerOutPut(AXP192_LDO2, AXP202_ON);
  #endif
  #ifdef TTGO_T_Beam_V1_2
  PMU.setALDO3Voltage(3300);
  PMU.enableALDO3(); 
  #endif
}

// cppcheck-suppress unusedFunction
void PowerManagement::deactivateGPS() {
  #ifdef TTGO_T_Beam_V1_0
  axp.setPowerOutPut(AXP192_LDO2, AXP202_OFF);
  #endif
  #ifdef TTGO_T_Beam_V1_2
  PMU.disableALDO3();
  #endif
}

// cppcheck-suppress unusedFunction
void PowerManagement::activateOLED() {
  //axp.setPowerOutPut(AXP192_DCDC1, AXP202_ON);
}

// cppcheck-suppress unusedFunction
void PowerManagement::decativateOLED() {
  //axp.setPowerOutPut(AXP192_DCDC1, AXP202_OFF);
}

// cppcheck-suppress unusedFunction
void PowerManagement::disableChgLed() {
  //axp.setChgLEDMode(AXP20X_LED_OFF);
}

// cppcheck-suppress unusedFunction
void PowerManagement::enableChgLed() {
  //axp.setChgLEDMode(AXP20X_LED_LOW_LEVEL);
}

// cppcheck-suppress unusedFunction
void PowerManagement::activateMeasurement() {
  //axp.adc1Enable(AXP202_BATT_CUR_ADC1 | AXP202_BATT_VOL_ADC1, true);
}

// cppcheck-suppress unusedFunction
void PowerManagement::deactivateMeasurement() {
  //axp.adc1Enable(AXP202_BATT_CUR_ADC1 | AXP202_BATT_VOL_ADC1, false);
}

// cppcheck-suppress unusedFunction
double PowerManagement::getBatteryVoltage() {
  //return axp.getBattVoltage() / 1000.0;
  return 1.0;
}

// cppcheck-suppress unusedFunction
double PowerManagement::getBatteryChargeDischargeCurrent() {
  /*if (axp.isChargeing()) {
    return axp.getBattChargeCurrent();
  }
  return -1.0 * axp.getBattDischargeCurrent();*/
  return 1.0;
}

bool PowerManagement::isBatteryConnected() {
  //return axp.isBatteryConnect();
  return true;
}

bool PowerManagement::isChargeing() {
  //return axp.isChargeing();
  return true;
}

void PowerManagement::setup() {
#ifdef TTGO_T_Beam_V1_0
  Wire.begin(SDA, SCL);
  if (!begin(Wire)) {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "AXP192", "init done!");
  } else {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "AXP192", "init failed!");
  }
  activateLoRa();
  activateOLED();
  activateGPS();
  activateMeasurement();
#endif
#ifdef TTGO_T_Beam_V1_2
  Wire.begin(SDA, SCL);
  if (!begin(Wire)) {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "AXP2101", "init done!");
  } else {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "AXP2101", "init failed!");
  }
  Serial.println("activando LoRa");
  activateLoRa();
  Serial.println("activando OLED");
  activateOLED();
  Serial.println("activando GPS");
  activateGPS();
  Serial.println("activando Measurement");
  activateMeasurement();
  Serial.println("activado");
#endif
}

void PowerManagement::lowerCpuFrequency() {
  #if defined(TTGO_T_Beam_V1_0) 
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
#ifdef TTGO_T_Beam_V1_0
  static unsigned int rate_limit_check_battery = 0;
  if (!(rate_limit_check_battery++ % 60))
    BatteryIsConnected = isBatteryConnected();
  if (BatteryIsConnected) {
    batteryVoltage       = String(getBatteryVoltage(), 2);
    batteryChargeDischargeCurrent = String(getBatteryChargeDischargeCurrent(), 0);
  }
#endif
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
#ifdef TTGO_T_Beam_V1_0
  obtainBatteryInfo();
  handleChargingLed();
#endif
}