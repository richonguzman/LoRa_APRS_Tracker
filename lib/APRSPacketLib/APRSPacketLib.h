#ifndef APRSPACKETLIB_H
#define APRSPACKETLIB_H

#include <Arduino.h>

struct APRSPacket {
    String  header;
    String  sender;
    String  tocall;
    String  path;
    String  addressee;
    String  message;
    int     type;
    String  symbol;
    String  overlay;
    float   latitude;
    float   longitude;
    int     course;
    int     speed;
    int     altitude;
    String  miceType;
    int     rssi;
    float   snr;
    int     freqError;
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

    String  doubleToString(double n, int ndec);
    String  gpsDecimalToDegreesLatitude(double lat);
    String  gpsDecimalToDegreesLongitude(double lon);

    void    encodeMiceAltitude(uint8_t *buf, uint32_t alt_m);
    void    encodeMiceCourseSpeed(uint8_t *buf, uint32_t speed_kt, uint32_t course_deg);
    void    encodeMiceLongitude(uint8_t *buf, gpsLongitudeStruct *lon);
    void    encodeMiceDestinationField(const String& msgType, uint8_t *buf, const gpsLatitudeStruct *lat, const gpsLongitudeStruct *lon);

    gpsLatitudeStruct gpsDecimalToDegreesMiceLatitude(float latitude);
    gpsLongitudeStruct gpsDecimalToDegreesMiceLongitude(float longitude);
    String  generateMiceGPSBeacon(const String& miceMsgType, const String& callsign, const String& symbol, const String& overlay, const String& path, float latitude, float longitude, float course, float speed, int altitude);

    String  generateBasePacket(const String& callsign, const String& tocall, const String& path);
    String  generateStatusPacket(const String& callsign, const String& tocall, const String& path, const String& status);
    String  generateMessagePacket(const String& callsign, const String& tocall, const String& path, const String& addressee, const String& message);

    String  buildDigiPacket(const String& packet, const String& callsign, const String& path, bool thirdParty);
    String  generateDigiRepeatedPacket(const String& packet, const String &callsign, const String& path);

    char    *ax25_base91enc(char *s, uint8_t n, uint32_t v);
    String  encodeGPS(float latitude, float longitude, float course, float speed, const String& symbol, bool sendAltitude, int altitude, bool sendStandingUpdate, const String& packetType);
    String  generateGPSBeaconPacket(const String& callsign, const String& tocall, const String& path, const String& overlay, const String& gpsData);

    float   decodeEncodedLatitude(const String& receivedPacket);
    float   decodeEncodedLongitude(const String& receivedPacket);
    float   decodeLatitude(const String& receivedPacket);
    float   decodeLongitude(const String& receivedPacket);
    int     decodeSpeed(const String& speed);
    int     decodeAltitude(const String& altitude);

    String  decodeMiceMsgType(const String& tocall);
    String  decodeMiceSymbol(const String& informationField);
    String  decodeMiceOverlay(const String& informationField);
    int     decodeMiceSpeed(const String& informationField);
    int     decodeMiceCourse(const String& informationField);
    int     decodeMiceAltitude(const String& informationField);
    float   gpsDegreesToDecimalLatitude(const String& degreesLatitude);
    float   gpsDegreesToDecimalLongitude(const String& degreesLongitude);
    float   decodeMiceLatitude(const String& destinationField);
    float   decodeMiceLongitude(const String& destinationField, const String& informationField);

    APRSPacket processReceivedPacket(const String& receivedPacket, int rssi, float snr, int freqError);

}

#endif