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
  String  miceType;
};

struct gpsLatitudeStruct {
	uint8_t degrees;
	uint8_t minutes;
	uint8_t minuteHundredths;
	uint8_t north;
};

struct gpsLongitudeStruct {
	uint8_t degrees;
	uint8_t minutes;
	uint8_t minuteHundredths;
	uint8_t east;
};

namespace APRSPacketLib {

  String double2string(double n, int ndec);
  String processLatitudeAPRS(double lat);
  String processLongitudeAPRS(double lon);

  void miceAltiduteEncoding(uint8_t *buf, uint32_t alt_m);
  void miceCourseSpeedEncoding(uint8_t *buf, uint32_t speed_kt, uint32_t course_deg);
  void miceLongitudeEncoding(uint8_t *buf, gpsLongitudeStruct *lon);
  void miceDestinationFieldEncoding(String msgType, uint8_t *buf, const gpsLatitudeStruct *lat, gpsLongitudeStruct *lon);

  gpsLatitudeStruct gpsDecimalToDegreesMiceLatitude(float latitude);
  gpsLongitudeStruct gpsDecimalToDegreesMiceLongitude(float longitude);

  String generateMiceGPSBeacon(String miceMsgType, String callsign, String symbol, String overlay, String path, float latitude, float longitude, float course, float speed, int altitude);

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