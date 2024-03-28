#ifndef STATION_UTILS_H_
#define STATION_UTILS_H_

#include <Arduino.h>

namespace STATION_Utils {

    String getFirstNearTracker();
    String getSecondNearTracker();
    String getThirdNearTracker();
    String getFourthNearTracker();

    void deleteListenedTrackersbyTime();
    void checkListenedTrackersByTimeAndDelete();
    void orderListenedTrackersByDistance(String callsign, float distance, float course);
    void checkSmartBeaconInterval(int speed);
    void checkStandingUpdateTime();
    void checkSmartBeaconValue();
    void checkSmartBeaconState();
    void sendBeacon(String type);
    void checkTelemetryTx();
    void saveIndex(uint8_t type, uint8_t index);
    void loadIndex(uint8_t type);

}

#endif