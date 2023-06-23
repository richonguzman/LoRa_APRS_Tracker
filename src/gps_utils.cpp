#include <TinyGPS++.h>
#include "gps_utils.h"
#include "pins_config.h"
#include "station_utils.h"

extern HardwareSerial  neo6m_gps;
extern TinyGPSPlus     gps;

namespace GPS_Utils {

void setup() {
    neo6m_gps.begin(9600, SERIAL_8N1, GPS_TX, GPS_RX);
}

void calculateDistanceCourse(String Callsign, double checkpointLatitude, double checkPointLongitude) {
  double distanceKm = TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(), checkpointLatitude, checkPointLongitude) / 1000.0;
  double courseTo   = TinyGPSPlus::courseTo(gps.location.lat(), gps.location.lng(), checkpointLatitude, checkPointLongitude);
  STATION_Utils::deleteListenedTrackersbyTime();
  STATION_Utils::orderListenedTrackersByDistance(Callsign, distanceKm, courseTo);
}

void decodeEncodedGPS(String packet, String sender) {
  String GPSPacket = packet.substring(packet.indexOf(":!/")+3);
  String encodedLatitude    = GPSPacket.substring(0,4);
  String encodedLongtitude  = GPSPacket.substring(4,8);

  int Y1 = int(encodedLatitude[0]);
  int Y2 = int(encodedLatitude[1]);
  int Y3 = int(encodedLatitude[2]);
  int Y4 = int(encodedLatitude[3]);
  float decodedLatitude = 90.0 - ((((Y1-33) * pow(91,3)) + ((Y2-33) * pow(91,2)) + ((Y3-33) * 91) + Y4-33) / 380926.0);
    
  int X1 = int(encodedLongtitude[0]);
  int X2 = int(encodedLongtitude[1]);
  int X3 = int(encodedLongtitude[2]);
  int X4 = int(encodedLongtitude[3]);
  float decodedLongitude = -180.0 + ((((X1-33) * pow(91,3)) + ((X2-33) * pow(91,2)) + ((X3-33) * 91) + X4-33) / 190463.0);
    
  Serial.print(sender); 
  Serial.print(" GPS : "); 
  Serial.print(decodedLatitude); Serial.print(" N "); 
  Serial.print(decodedLongitude);Serial.println(" E");

  calculateDistanceCourse(sender, decodedLatitude, decodedLongitude);
}

void getReceivedGPS(String packet, String sender) {
  String infoGPS;
  if (packet.indexOf(":!") > 10) {
    infoGPS = packet.substring(packet.indexOf(":!")+2);
  } else if (packet.indexOf(":=") > 10) {
    infoGPS = packet.substring(packet.indexOf(":=")+2);
  }
  String Latitude       = infoGPS.substring(0,8);
  String Longitude      = infoGPS.substring(9,18);

  float convertedLatitude, convertedLongitude;
  String firstLatPart   = Latitude.substring(0,2);
  String secondLatPart  = Latitude.substring(2,4);
  String thirdLatPart   = Latitude.substring(Latitude.indexOf(".")+1,Latitude.indexOf(".")+3);
  String firstLngPart   = Longitude.substring(0,3);
  String secondLngPart  = Longitude.substring(3,5);
  String thirdLngPart   = Longitude.substring(Longitude.indexOf(".")+1,Longitude.indexOf(".")+3);
  convertedLatitude     = firstLatPart.toFloat() + (secondLatPart.toFloat()/60) + (thirdLatPart.toFloat()/(60*100));
  convertedLongitude    = firstLngPart.toFloat() + (secondLngPart.toFloat()/60) + (thirdLngPart.toFloat()/(60*100));
  
  String LatSign = String(Latitude[7]);
  String LngSign = String(Longitude[8]);
  if (LatSign == "S") {
    convertedLatitude = -convertedLatitude;
  } 
  if (LngSign == "W") {
    convertedLongitude = -convertedLongitude;
  } 
  Serial.print(sender); 
  Serial.print(" GPS : "); 
  Serial.print(convertedLatitude); Serial.print(" N "); 
  Serial.print(convertedLongitude);Serial.println(" E");

  calculateDistanceCourse(sender, convertedLatitude, convertedLongitude);
}

}