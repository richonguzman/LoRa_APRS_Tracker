#include "power_utils.h"
#include "logger.h"

extern logging::Logger logger;

// cppcheck-suppress unusedFunction
bool PowerManagement::begin(TwoWire &port) {
  bool result = axp.begin(port, AXP192_SLAVE_ADDRESS);
  if (!result) {
    axp.setDCDC1Voltage(3300);
  }
  return result;
}

// cppcheck-suppress unusedFunction
void PowerManagement::activateLoRa() {
  axp.setPowerOutPut(AXP192_LDO2, AXP202_ON);
}

// cppcheck-suppress unusedFunction
void PowerManagement::deactivateLoRa() {
  axp.setPowerOutPut(AXP192_LDO2, AXP202_OFF);
}

// cppcheck-suppress unusedFunction
void PowerManagement::activateGPS() {
  axp.setPowerOutPut(AXP192_LDO3, AXP202_ON);
}

// cppcheck-suppress unusedFunction
void PowerManagement::deactivateGPS() {
  axp.setPowerOutPut(AXP192_LDO3, AXP202_OFF);
}

// cppcheck-suppress unusedFunction
void PowerManagement::activateOLED() {
  axp.setPowerOutPut(AXP192_DCDC1, AXP202_ON);
}

// cppcheck-suppress unusedFunction
void PowerManagement::decativateOLED() {
  axp.setPowerOutPut(AXP192_DCDC1, AXP202_OFF);
}

// cppcheck-suppress unusedFunction
void PowerManagement::disableChgLed() {
  axp.setChgLEDMode(AXP20X_LED_OFF);
}

// cppcheck-suppress unusedFunction
void PowerManagement::enableChgLed() {
  axp.setChgLEDMode(AXP20X_LED_LOW_LEVEL);
}

// cppcheck-suppress unusedFunction
void PowerManagement::activateMeasurement() {
  axp.adc1Enable(AXP202_BATT_CUR_ADC1 | AXP202_BATT_VOL_ADC1, true);
}

// cppcheck-suppress unusedFunction
void PowerManagement::deactivateMeasurement() {
  axp.adc1Enable(AXP202_BATT_CUR_ADC1 | AXP202_BATT_VOL_ADC1, false);
}

// cppcheck-suppress unusedFunction
double PowerManagement::getBatteryVoltage() {
  return axp.getBattVoltage() / 1000.0;
}

// cppcheck-suppress unusedFunction
double PowerManagement::getBatteryChargeDischargeCurrent() {
  if (axp.isChargeing()) {
    return axp.getBattChargeCurrent();
  }
  return -1.0 * axp.getBattDischargeCurrent();
}

bool PowerManagement::isBatteryConnected() {
  return axp.isBatteryConnect();
}

bool PowerManagement::isChargeing() {
  return axp.isChargeing();
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