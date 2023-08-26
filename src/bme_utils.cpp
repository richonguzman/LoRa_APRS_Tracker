#include "bme_utils.h"
#include "configuration.h"
#include "gps_utils.h"
#include "display.h"
#include <logger.h>

#define SEALEVELPRESSURE_HPA (1013.25)
#define HEIGHT_CORRECTION 0             // in meters
#define CORRECTION_FACTOR (8.2296)      // for meters

extern Configuration    Config;
extern logging::Logger  logger;


namespace BME_Utils {

  Adafruit_BME280   bme;

  void setup() {
    if (Config.bme.active) {
      bool status;
      status = bme.begin(0x76);  // Don't forget to join pins for righ direction on BME280!
      if (!status) {
        show_display("ERROR", "", "BME sensor active", "but no sensor found...");
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "BME", " BME280 Active in config but not found! Check Wiring");
        while (1);
      } else {
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BME", " BME280 Module init done!");
      }
    } else {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BME", " BME280 Module not active in 'tracker_conf.json'");
    }
  }

  String generateTempString(float bmeTemp) {
    String strTemp;
    strTemp = String((int)bmeTemp);
    switch (strTemp.length()) {
      case 1:
        return "  " + strTemp;
        break;
      case 2:
        return " " + strTemp;
        break;
      case 3:
        return strTemp;
        break;
      default:
        return "-999";
    }
  }

  String generateHumString(float bmeHum) {
    String strHum;
    strHum = String((int)bmeHum);
    switch (strHum.length()) {
      case 1:
        return " " + strHum;
        break;
      case 2:
        return strHum;
        break;
      case 3:
        if ((int)bmeHum == 100) {
          return "  ";
        } else {
          return "-99";
        }
        break;
      default:
        return "-99";
    }
  }

  String generatePresString(float bmePress) {
    String strPress;
    strPress = String((int)bmePress);
    switch (strPress.length()) {
      case 1:
        return "000" + strPress;
        break;
      case 2:
        return "00" + strPress;
        break;
      case 3:
        return "0" + strPress;
        break;
      case 4:
        return strPress;
        break;
      case 5:
        return strPress;
        break;
      default:
        return "-99999";
    }
  }

  String readDataSensor() {
    String wx, tempStr, humStr, presStr;
    float newTemp   = bme.readTemperature();
    float newHum    = bme.readHumidity();
    float newPress  = (bme.readPressure() / 100.0F);
    
    if (isnan(newTemp) || isnan(newHum) || isnan(newPress)) {
      Serial.println("BME280 Module data failed");
      wx = " - C    - %    - hPa";
      return wx;
    } else {
      tempStr = generateTempString(newTemp);
      humStr  = generateHumString(newHum);
      presStr = generatePresString(newPress + (HEIGHT_CORRECTION/CORRECTION_FACTOR));
      wx = tempStr + "C   " + humStr + "%   " + presStr + "hPa";
      return wx;
    }
  }

}