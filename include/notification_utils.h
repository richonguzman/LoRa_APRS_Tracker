#ifndef NOTIFICATION_UTILS_H_
#define NOTIFICATION_UTILS_H_

#include <Arduino.h>

namespace NOTIFICATION_Utils {

    void playTone(int frequency, uint8_t duration);
    void beaconTxBeep();
    void messageBeep();
    void stationHeardBeep();
    void shutDownBeep();
    void lowBatteryBeep();
    void start();

}

#endif