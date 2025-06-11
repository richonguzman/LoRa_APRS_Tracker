#ifndef WX_UTILS_H_
#define WX_UTILS_H_

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_BME680.h>
#include <Arduino.h>

 
namespace WX_Utils {

    void    setup();
    String  readDataSensor(const uint8_t type);

}

#endif