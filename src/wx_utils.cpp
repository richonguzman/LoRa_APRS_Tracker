#include <TinyGPS++.h>
#include <logger.h>
#ifdef LIGHTTRACKER_PLUS_1_0
#include "Adafruit_SHTC3.h"
#endif
#include "wx_utils.h"
#include "configuration.h"
#include "display.h"

#define SEALEVELPRESSURE_HPA (1013.25)
#define CORRECTION_FACTOR (8.2296)      // for meters

extern Configuration    Config;
extern logging::Logger  logger;
extern TinyGPSPlus      gps;

extern uint8_t          wxModuleAddress;

float newHum, newTemp, newPress, newGas;

uint32_t    sensorLastReading   = -60000;
int         wxModuleType        = 0;

bool        wxModuleFound       = false;


Adafruit_BME280     bme280;
#if defined(HELTEC_V3_GPS) || defined(HELTEC_V3_TNC) || defined(HELTEC_V3_2_GPS) || defined(HELTEC_V3_2_TNC)
Adafruit_BMP280     bmp280(&Wire1);
#else
Adafruit_BMP280     bmp280;
Adafruit_BME680     bme680;
#endif
#ifdef LIGHTTRACKER_PLUS_1_0
Adafruit_SHTC3 shtc3 = Adafruit_SHTC3();
#endif

   

namespace WX_Utils {    

    void setup() {
        if (Config.wxsensor.active) {
            #ifdef LIGHTTRACKER_PLUS_1_0
                if (!shtc3.begin()) {
                    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BME", " SHTC3 sensor found");
                    while (1) delay(1);
                }
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BME", " SHTC3 sensor found");
                wxModuleFound = true;
                wxModuleType = 4;
            #else
                if (wxModuleAddress != 0x00) {
                    #if defined(HELTEC_V3_GPS) || defined(HELTEC_V3_TNC) || defined(HELTEC_V3_2_GPS) || defined(HELTEC_V3_2_TNC)
                        if (bme280.begin(wxModuleAddress, &Wire1)) {
                            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BME", " BME280 sensor found");
                            wxModuleType = 1;
                            wxModuleFound = true;
                        } 
                    #else
                        if (bme280.begin(wxModuleAddress)) {
                            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BME", " BME280 sensor found");
                            wxModuleType = 1;
                            wxModuleFound = true;
                        }
                        if (!wxModuleFound) {
                            if (bme680.begin(wxModuleAddress)) {
                                logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BME", " BME680 sensor found");
                                wxModuleType = 3;
                                wxModuleFound = true;
                            }
                        }
                    #endif
                    if (!wxModuleFound) {
                        if (bmp280.begin(wxModuleAddress)) {
                            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BME", " BMP280 sensor found");
                            wxModuleType = 2;
                            wxModuleFound = true;
                        }
                    }
                    if (!wxModuleFound) {
                        displayShow("ERROR", "BME/BMP sensor active", "but no sensor found...", 2000);
                        logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "BME", " BME/BMP sensor Active in config but not found! Check Wiring");
                    } else {
                        switch (wxModuleType) {
                            case 1:
                                bme280.setSampling(Adafruit_BME280::MODE_FORCED,
                                            Adafruit_BME280::SAMPLING_X1,
                                            Adafruit_BME280::SAMPLING_X1,
                                            Adafruit_BME280::SAMPLING_X1,
                                            Adafruit_BME280::FILTER_OFF
                                            );
                                logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BME", " BME280 Module init done!");
                                break;
                            case 2:
                                bmp280.setSampling(Adafruit_BMP280::MODE_FORCED,
                                            Adafruit_BMP280::SAMPLING_X1,
                                            Adafruit_BMP280::SAMPLING_X1,
                                            Adafruit_BMP280::FILTER_OFF
                                            ); 
                                logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BMP", " BMP280 Module init done!");
                                break;
                            case 3:
                                #if !defined(HELTEC_V3_GPS) && !defined(HELTEC_V3_TNC) && !defined(HELTEC_V3_2_GPS) && !defined(HELTEC_V3_2_TNC)
                                    bme680.setTemperatureOversampling(BME680_OS_1X);
                                    bme680.setHumidityOversampling(BME680_OS_1X);
                                    bme680.setPressureOversampling(BME680_OS_1X);
                                    bme680.setIIRFilterSize(BME680_FILTER_SIZE_0);
                                    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BME", " BMP680 Module init done!");
                                #endif
                                break;
                        }
                    }
                }
            #endif
        }
    }

    const String generateTempString(const float sensorTemp, const uint8_t type) {
        String strTemp;
        if (type == 1) {    // OLED
            strTemp = String((int)sensorTemp);
        } else {
            strTemp = String((int)((sensorTemp * 1.8) + 32));
        }
        switch (strTemp.length()) {
            case 1:
                if (type == 1) {
                    return "  " + strTemp;
                } else {
                    return "00" + strTemp;
                }
            case 2:
                if (type == 1) {
                    return " " + strTemp;
                } else {
                    return "0" + strTemp;
                }
            case 3:
                return strTemp;
            default:
                return "-999";
        }
    }

    const String generateHumString(const float sensorHum, const uint8_t type) {
        String strHum = String((int)sensorHum);
        switch (strHum.length()) {
            case 1:
                if (type == 1) {
                    return " " + strHum;
                } else {
                    return "0" + strHum;
                }
            case 2:
                return strHum;
            case 3:
                if ((int)sensorHum == 100) {
                    if (type == 1) {
                        return "  ";
                    } else {
                        return "00";
                    }
                } else {
                    return "-99";
                }
            default:
                return "-99";
        }
    }

    const String generatePresString(const float sensorPress, const uint8_t type) {
        String strPress = String((int)sensorPress);
        String decPress = String(int((sensorPress - int(sensorPress)) * 10));
        switch (strPress.length()) {
            case 1:
                if (type == 1) {
                    return "000" + strPress;
                } else {
                    return "000" + strPress + decPress;
                }
            case 2:
                if (type == 1) {
                    return "00" + strPress;
                } else {
                    return "00" + strPress + decPress;
                }
            case 3:
                if (type == 1) {
                    return "0" + strPress;
                } else {
                    return "0" + strPress + decPress;
                }
            case 4:
                if (type == 1) {
                    return strPress;
                } else {
                    return strPress + decPress;
                }
            case 5:
                return strPress;
            default:
                return "-99999";
        }
    }

    const String readDataSensor(const uint8_t type) {
        uint32_t lastReading = millis() - sensorLastReading;
        if (lastReading > 60 * 1000) {
            #ifdef LIGHTTRACKER_PLUS_1_0
                sensors_event_t humidity, temp;
                shtc3.getEvent(&humidity, &temp);
                newTemp     = temp.temperature;
                newHum      = humidity.relative_humidity;
                newPress    = 0;
            #else
                switch (wxModuleType) {
                    case 1: // BME280
                        bme280.takeForcedMeasurement();
                        newTemp     = bme280.readTemperature();
                        newPress    = (bme280.readPressure() / 100.0F);
                        newHum      = bme280.readHumidity();
                        break;
                    case 2: // BMP280
                        bmp280.takeForcedMeasurement();
                        newTemp     = bmp280.readTemperature();
                        newPress    = (bmp280.readPressure() / 100.0F);
                        newHum      = 0;
                        break;
                    case 3: // BME680
                        #if !defined(HELTEC_V3_GPS) && !defined(HELTEC_V3_TNC) && !defined(HELTEC_V3_2_GPS) && !defined(HELTEC_V3_2_TNC)
                            bme680.performReading();
                            delay(50);
                            if (bme680.endReading()) {
                                newTemp     = bme680.temperature;
                                newPress    = (bme680.pressure / 100.0F);
                                newHum      = bme680.humidity;
                                newGas      = bme680.gas_resistance / 1000.0; // in Kilo ohms
                            }
                        #endif
                        break;
                }
            #endif
            sensorLastReading = millis();
        }
        
        String wx;
        if (isnan(newTemp) || isnan(newHum) || isnan(newPress)) {
            Serial.println("WX Sensor data failed");
            wx = ((type == 1) ? " - C    - %    - hPa" : ".../...g...t...");
            return wx;
        } else {
            String tempStr = generateTempString(newTemp + Config.wxsensor.temperatureCorrection, type);
            String humStr;
            #ifdef LIGHTTRACKER_PLUS_1_0
                humStr  = generateHumString(newHum,type);
            #else
                if (wxModuleType == 1 || wxModuleType == 3) {
                    humStr  = generateHumString(newHum,type);
                } else if (wxModuleType == 2) {
                    humStr  = "..";
                }
            #endif
            String presStr = generatePresString(newPress + (gps.altitude.meters()/CORRECTION_FACTOR), type);
            #ifdef LIGHTTRACKER_PLUS_1_0
                if (type == 1) {
                    wx = tempStr;
                    wx += "C      ";
                    wx += humStr;
                    wx += "%   ";
                } else {
                    wx = ".../...g...t";
                    wx += tempStr;
                    wx += "h";
                    wx += humStr;
                }
            #else
                if (type == 1) {
                    if (wxModuleType == 1 || wxModuleType == 3) {
                        wx = tempStr;
                        wx += "C   ";
                        wx += humStr;
                        wx += "%   ";
                        wx += presStr;
                        wx += "hPa";
                    } else if (wxModuleType == 2) {
                        wx = "T: ";
                        wx += tempStr;
                        wx += "C P: ";
                        wx += presStr;
                        wx += "hPa";
                    }
                } else {
                    wx = ".../...g...t";
                    wx += tempStr;
                    wx += "h";
                    wx += humStr;
                    wx += "b";
                    wx += presStr;
                    if (wxModuleType == 3) {
                        wx += "Gas: ";
                        wx += String(newGas);
                        wx += "Kohms";
                    }
                }
            #endif
            return wx;
        }
    }

}