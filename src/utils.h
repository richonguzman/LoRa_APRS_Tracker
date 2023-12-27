#ifndef UTILS_H_
#define UTILS_H_

#include <Arduino.h>
#include <TimeLib.h>

namespace Utils {

    char *getMaidenheadLocator(double lat, double lon, int size);
    String createDateString(time_t t);
    String createTimeString(time_t t);
    void checkStatus();
    void checkDisplayEcoMode();
    String getSmartBeaconState();
    void checkFlashlight();

}

#endif