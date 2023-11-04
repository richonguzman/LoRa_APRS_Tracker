#ifndef APRSPACKETLIB_H
#define APRSPACKETLIB_H

#include <Arduino.h>

namespace APRSPacketLib {

String generateStatusPacket(String callsign, String tocall, String path, String status);
String generateGPSBeaconPacket(String callsign, String tocall, String path, String overlay, String gpsData);

}

#endif