#ifndef BME_UTILS_H_
#define BME_UTILS_H_

#include <Adafruit_Sensor.h>
#include <Arduino.h>

#define BME280Sensor        // its set by default but you should comment it with "//"
//#define BMP280Sensor      // and delete "//" from the one you want to use.
//#define BME680Sensor

#ifdef BME280Sensor
#include <Adafruit_BME280.h>
#endif
#ifdef BMP280Sensor
#include <Adafruit_BMP280.h>
#endif
#ifdef BME680Sensor
#include <Adafruit_BME680.h>
#endif

namespace BME_Utils {

    void setup();
    String generateTempString(float bmeTemp, String type);
    String generateHumString(float bmeHum, String type);
    String generatePresString(float bmePress, String type);
    String readDataSensor(String type);

}

#endif