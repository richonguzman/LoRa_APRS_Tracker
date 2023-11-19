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
  String  symbol;
  float   latitude;
  float   longitude;
  int     course;
  int     speed;
  int     altitude;
};

namespace APRSPacketLib {

  String generateBasePacket(String callsign, String tocall, String path);
  String generateStatusPacket(String callsign, String tocall, String path, String status);
  String generateMessagePacket(String callsign, String tocall, String path, String addressee, String message);
  String generateDigiRepeatedPacket(APRSPacket packet, String callsign);
  char *ax25_base91enc(char *s, uint8_t n, uint32_t v);
  String encondeGPS(float latitude, float longitude, float course, float speed, String symbol, bool sendAltitude, int altitude, bool sendStandingUpdate, String packetType);
  String generateGPSBeaconPacket(String callsign, String tocall, String path, String overlay, String gpsData);
  float decodeEncodedLatitude(String receivedPacket);
  float decodeEncodedLongitude(String receivedPacket);
  float decodeLatitude(String receivedPacket);
  float decodeLongitude(String receivedPacket);
  int decodeSpeed(String speed);
  int decodeAltitude(String altitude);
  APRSPacket processReceivedPacket(String receivedPacket);

}

#endif