#ifndef NOTIFICATION_UTILS_H_
#define NOTIFICATION_UTILS_H_

#include <Arduino.h>

namespace Notification_Utils {

void playTone(int frequency, int duration);
void beaconTxBeep();
void messageBeep();
void stationHeardBeep();
void shutDownBeep();
void start();

}

#endif