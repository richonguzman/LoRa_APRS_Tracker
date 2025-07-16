/* Copyright (C) 2025 Ricardo Guzman - CA2RXU
 * 
 * This file is part of LoRa APRS Tracker.
 * 
 * LoRa APRS Tracker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or 
 * (at your option) any later version.
 * 
 * LoRa APRS Tracker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with LoRa APRS Tracker. If not, see <https://www.gnu.org/licenses/>.
 */

#include <TinyGPS++.h>
#include <logger.h>
#ifdef LIGHTTRACKER_PLUS_1_0
#include "Adafruit_SHTC3.h"
#endif
#include "telemetry_utils.h"
#include "configuration.h"
#include "wx_utils.h"
#include "display.h"

#define SEALEVELPRESSURE_HPA (1013.25)
#define CORRECTION_FACTOR (8.2296)      // for meters

extern Configuration    Config;
extern logging::Logger  logger;
extern TinyGPSPlus      gps;

extern uint8_t          wxModuleAddress;

float newHum, newTemp, newPress, newGas;

uint32_t    sensorLastReading   = -60000;
int         wxModuleType        = 0;        // 1=BME280, 2=BMP280, 3=BME680, 4=SHTC3

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
        if (Config.telemetry.active) {
            #ifdef LIGHTTRACKER_PLUS_1_0
                if (!shtc3.begin()) {
                    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BME", " SHTC3 sensor not found");
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

    String formatSensorValueforScreen(float value, int width, const String& errorValue) {
        String valueString = String((int)value);
        if (valueString.length() > width) return errorValue;
        while (valueString.length() < width) {
            valueString = " " + valueString;  // Left-padding
        }
        return valueString;
    }

    String readDataSensor(const uint8_t type) {
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
        
        String sensorTelemetry;
        if (isnan(newTemp) || isnan(newHum) || isnan(newPress) || (!wxModuleFound)) {
            Serial.println("WX Sensor data failed/not found");
            sensorTelemetry = ((type == 1) ? " - C    - %    - hPa" : "");
        } else {
            if (type == 0) {
                sensorTelemetry = TELEMETRY_Utils::generateEncodedTelemetryBytes(newTemp + Config.telemetry.temperatureCorrection, false, 2); // temperature
                if (wxModuleType == 1 || wxModuleType == 3 || wxModuleType == 4) {
                    sensorTelemetry += TELEMETRY_Utils::generateEncodedTelemetryBytes(newHum, false, 0); // humidity
                }
                if (wxModuleType == 1 || wxModuleType == 2 || wxModuleType == 3) {
                    sensorTelemetry += TELEMETRY_Utils::generateEncodedTelemetryBytes(newPress + (gps.altitude.meters()/CORRECTION_FACTOR), false, 3); // pressure
                }
                if (wxModuleType == 3) {
                    sensorTelemetry += TELEMETRY_Utils::generateEncodedTelemetryBytes(newGas, false, 0); // gas
                }
            } else {    // show it in (Oled) Screen
                sensorTelemetry = formatSensorValueforScreen(newTemp + Config.telemetry.temperatureCorrection, 3, "-99");
                sensorTelemetry += "C   ";
                if (wxModuleType == 1 || wxModuleType == 3 || wxModuleType == 4) {
                    sensorTelemetry += formatSensorValueforScreen(newHum, 2, "-9");
                } else {
                    sensorTelemetry += "__";
                }
                sensorTelemetry += "%   ";
                if (wxModuleType == 1 || wxModuleType == 2 || wxModuleType == 3) {
                    sensorTelemetry += formatSensorValueforScreen(newPress + (gps.altitude.meters()/CORRECTION_FACTOR), 4, "-999");
                } else {
                    sensorTelemetry += "____";
                }
                sensorTelemetry += "hPa";
            }
        }
        return sensorTelemetry;
    }

}