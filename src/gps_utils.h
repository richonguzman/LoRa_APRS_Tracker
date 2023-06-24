#ifndef GPS_UTILS_H_
#define GPS_UTILS_H_

#include <Arduino.h>

namespace GPS_Utils {

void setup();
void calculateDistanceCourse(String Callsign, double checkpointLatitude, double checkPointLongitude);
void decodeEncodedGPS(String packet, String sender);
void getReceivedGPS(String packet, String sender);
void getData();
void setDateFromData();
void calculateDistanceTraveled();
void calculateHeadingDelta(int speed);
void checkStartUpFrames();
String encondeGPS();

}

#endif