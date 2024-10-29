#ifndef SMARTBEACON_UTILS_H_
#define SMARTBEACON_UTILS_H_

#include <Arduino.h>

struct SmartBeaconValues {
    int     slowRate;
    int     slowSpeed;
    int     fastRate;
    int     fastSpeed;
    int     minTxDist;
    int     minDeltaBeacon;
    int     turnMinDeg;
    int     turnSlope;
};


namespace SMARTBEACON_Utils {

    void checkSettings(byte index);
    void checkInterval(int speed);
    void checkFixedBeaconTime();
    void checkState();
    
}

#endif