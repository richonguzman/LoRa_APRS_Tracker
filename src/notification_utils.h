#ifndef NOTIFICATION_UTILS_H_
#define NOTIFICATION_UTILS_H_

#include <Arduino.h>

namespace NOTIFICATION_Utils {

    void playTone(int frequency, int duration);
    void beaconTxBeep();
    void messageBeep();
    void stationHeardBeep();
    //void shutDownBeep();
    void lowBatteryBeep();
    void start();
    #ifdef ESP32_BV5DJ_1W_LoRa_GPS
    void startRGB();
    #endif

}

#endif