#ifndef BME_UTILS_H_
#define BME_UTILS_H_

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_BME680.h>
#include <Arduino.h>

 
namespace BME_Utils {

    void getWxModuleAddres();
    void setup();
    String generateTempString(float bmeTemp, uint8_t type);
    String generateHumString(float bmeHum, uint8_t type);
    String generatePresString(float bmePress, uint8_t type);
    String readDataSensor(uint8_t type);

}

#endif