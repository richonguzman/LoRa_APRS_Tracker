#ifndef BME_UTILS_H_
#define BME_UTILS_H_

#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

namespace BME_Utils {

void setup();
String generateTempString(float bmeTemp, String type);
String generateHumString(float bmeHum, String type);
String generatePresString(float bmePress, String type);
String readDataSensor(String type);

}

#endif