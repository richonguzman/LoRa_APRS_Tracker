#ifndef WX_UTILS_H_
#define WX_UTILS_H_

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_BME680.h>
#include <Arduino.h>

 
namespace WX_Utils {

    void  setup();
    const String generateTempString(const float sensorTemp, const uint8_t type);
    const String generateHumString(const float sensorHum, const uint8_t type);
    const String generatePresString(const float sensorPress, const uint8_t type);
    const String readDataSensor(const uint8_t type);

}

#endif