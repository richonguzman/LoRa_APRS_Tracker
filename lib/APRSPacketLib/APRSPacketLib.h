#ifndef APRSPACKETLIB_H
#define APRSPACKETLIB_H

#include <Arduino.h>

struct APRSPacket {
  String  sender;
  String  addressee;
  String  message;
  String  type;
  float   latitude;
  float   longitude;
};

namespace APRSPacketLib {

String generateStatusPacket(String callsign, String tocall, String path, String status);
String generateGPSBeaconPacket(String callsign, String tocall, String path, String overlay, String gpsData);
APRSPacket processReceivedPacket(String receivedPacket);

}

#endif