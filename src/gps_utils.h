#ifndef GPS_UTILS_H_
#define GPS_UTILS_H_

#include <Arduino.h>

namespace GPS_Utils {

    void    setup();
    void    calculateDistanceCourse(const String& callsign, double checkpointLatitude, double checkPointLongitude);
    void    getData();
    void    setDateFromData();
    void    calculateDistanceTraveled();
    void    calculateHeadingDelta(int speed);
    void    checkStartUpFrames();
    String  getCardinalDirection(float course);

}

#endif