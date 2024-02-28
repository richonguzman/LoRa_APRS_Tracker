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
    void saveIndex(int type, int index);
    void loadIndex(int type);

}

#endif