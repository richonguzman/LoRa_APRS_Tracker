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

float newHum, newTemp, newPress, newGas;

bool        bmeSensorFound      = false;
uint32_t    bmeLastReading      = -60000;

namespace BME_Utils {

    #ifdef BME280Sensor
    Adafruit_BME280   bme;
    #endif
    #ifdef BMP280Sensor
    Adafruit_BMP280   bme;
    #endif
    #ifdef BME680Sensor
    Adafruit_BME680 bme; 
    #endif

    void setup() {
        if (Config.bme.active) {
            bool status;
            #ifdef HELTEC_V3_GPS
            status = bme.begin(0x76, &Wire1);
            #else
            status = bme.begin(0x76);
            #endif
            if (!status) {
                show_display("ERROR", "", "BME/BMP sensor active", "but no sensor found...", "", 2000);
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "BME", " BME/BMP sensor Active in config but not found! Check Wiring");
            } else {
                #ifdef BME280Sensor
                bme.setSampling(Adafruit_BME280::MODE_FORCED,
                                Adafruit_BME280::SAMPLING_X1,
                                Adafruit_BME280::SAMPLING_X1,
                                Adafruit_BME280::SAMPLING_X1,
                                Adafruit_BME280::FILTER_OFF
                                );
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BME", " BME280 Module init done!");
                #endif
                #ifdef BMP280Sensor
                bme.setSampling(Adafruit_BMP280::MODE_FORCED,
                                Adafruit_BMP280::SAMPLING_X1,
                                Adafruit_BMP280::SAMPLING_X1,
                                Adafruit_BMP280::FILTER_OFF
                                ); 
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BMP", " BMP280 Module init done!");
                #endif
                #ifdef BME680Sensor
                bme.setTemperatureOversampling(BME680_OS_1X);
                bme.setHumidityOversampling(BME680_OS_1X);
                bme.setPressureOversampling(BME680_OS_1X);
                bme.setIIRFilterSize(BME680_FILTER_SIZE_0);
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BME", " BMP680 Module init done!");
                #endif
                bmeSensorFound = true;
            }
        } else {
            #ifdef BME280Sensor
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BME", " BME280 Module not active in 'tracker_conf.json'");
            #endif
            #ifdef BMP280Sensor
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BMP", " BMP280 Module not active in 'tracker_conf.json'");
            #endif
            #ifdef BME680Sensor
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BMP", " BMP680 Module not active in 'tracker_conf.json'");
            #endif
        }
    }

    String generateTempString(float bmeTemp, String type) {
        String strTemp;
        if (type=="OLED") {
            strTemp = String((int)bmeTemp);
        } else {
            strTemp = String((int)((bmeTemp * 1.8) + 32));
        }
        switch (strTemp.length()) {
            case 1:
                if (type=="OLED") {
                    return "  " + strTemp;
                } else {
                    return "00" + strTemp;
                }
                break;
            case 2:
                if (type=="OLED") {
                    return " " + strTemp;
                } else {
                    return "0" + strTemp;
                }
                break;
            case 3:
                return strTemp;
                break;
            default:
                return "-999";
        }
    }

    String generateHumString(float bmeHum, String type) {
        String strHum;
        strHum = String((int)bmeHum);
        switch (strHum.length()) {
            case 1:
                if (type=="OLED") {
                    return " " + strHum;
                } else {
                    return "0" + strHum;
                }
                break;
            case 2:
                return strHum;
                break;
            case 3:
                if ((int)bmeHum == 100) {
                    if (type=="OLED") {
                        return "  ";
                    } else {
                        return "00";
                    }
                } else {
                    return "-99";
                }
                break;
            default:
                return "-99";
        }
    }

    String generatePresString(float bmePress, String type) {
        String strPress;
        strPress = String((int)bmePress);
        switch (strPress.length()) {
            case 1:
                if (type=="OLED") {
                    return "000" + strPress;
                } else {
                    return "000" + strPress + "0";
                }
                break;
            case 2:
                if (type=="OLED") {
                    return "00" + strPress;
                } else {
                    return "00" + strPress + "0";
                }
                break;
            case 3:
                if (type=="OLED") {
                    return "0" + strPress;
                } else {
                    return "0" + strPress + "0";
                }
                break;
            case 4:
                if (type=="OLED") {
                    return strPress;
                } else {
                    return strPress + "0";
                }
                break;
            case 5:
                return strPress;
                break;
            default:
                return "-99999";
        }
    }

    String readDataSensor(String type) {
        String wx, tempStr, humStr, presStr;

        uint32_t lastReading = millis() - bmeLastReading;
        if (lastReading > 60 * 1000) {
            #if defined(BME280Sensor) || defined(BMP280Sensor)
            bme.takeForcedMeasurement();
            newTemp   = bme.readTemperature();
            newPress  = (bme.readPressure() / 100.0F);
            #ifdef BME280Sensor
            newHum = bme.readHumidity();
            #endif
            #ifdef BMP280Sensor
            newHum = 0;
            #endif
            #endif

            #ifdef BME680Sensor
            bme.performReading();
            delay(50);
            if (bme.endReading()) {
                newTemp     = bme.temperature;
                newPress    = (bme.pressure / 100.0F);
                newHum      = bme.humidity;
                newGas      = bme.gas_resistance / 1000.0; // in Kilo ohms
            }
            #endif
            bmeLastReading = millis();
        }
        
        if (isnan(newTemp) || isnan(newHum) || isnan(newPress)) {
            Serial.println("BME/BMP Module data failed");
            if (type == "OLED") {
                wx = " - C    - %    - hPa";
            } else {
                wx = ".../...g...t...r...p...P...h..b.....";
            }
            return wx;
        } else {
            tempStr = generateTempString(newTemp, type);
            #if defined(BME280Sensor) || defined(BME680Sensor)
            humStr  = generateHumString(newHum,type);
            #endif
            #ifdef BMP280Sensor
            humStr  = "..";
            #endif
            presStr = generatePresString(newPress + (Config.bme.heightCorrection/CORRECTION_FACTOR), type);
            if (type == "OLED") {
                #if defined(BME280Sensor) || defined(BME680Sensor)
                wx = tempStr + "C   " + humStr + "%   " + presStr + "hPa";
                #endif
                #ifdef BMP280Sensor
                wx = "T: " + tempStr + "C " + "P: " + presStr + "hPa";
                #endif
            } else {
                wx = ".../...g...t" + tempStr + "r...p...P...h" + humStr + "b" + presStr;
                #ifdef BME680Sensor
                wx += "Gas: " + String(newGas) + "Kohms";
                #endif
            }
            return wx;
        }
    }

}