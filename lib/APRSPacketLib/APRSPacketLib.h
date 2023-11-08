#ifndef APRSPACKETLIB_H
#define APRSPACKETLIB_H

#include <Arduino.h>

struct APRSPacket {
  String  sender;
  String  tocall;
  String  path;
  String  addressee;
  String  message;
  int     type;
  float   latitude;
  float   longitude;
};

namespace APRSPacketLib {

String generateStatusPacket(String callsign, String tocall, String path, String status);
String generateDigiRepeatedPacket(APRSPacket packet, String callsign);
char *ax25_base91enc(char *s, uint8_t n, uint32_t v);
String encondeGPS(float latitude, float longitude, float course, float speed, String symbol, bool sendAltitude, int altitude, bool sendStandingUpdate, String packetType);
String generateGPSBeaconPacket(String callsign, String tocall, String path, String overlay, String gpsData);
float decodeEncodedLatitude(String receivedPacket);
float decodeEncodedLongitude(String receivedPacket);
float decodeLatitude(String receivedPacket);
float decodeLongitude(String receivedPacket);
APRSPacket processReceivedPacket(String receivedPacket);

}

#endif