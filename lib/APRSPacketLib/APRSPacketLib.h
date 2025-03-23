#ifndef APRSPACKETLIB_H
#define APRSPACKETLIB_H

#include <Arduino.h>

struct APRSPacket {
    String  header;     //  (if packet is Third-Packet)
    String  sender;     //  Source Address      = sender
    String  tocall;     //  Destination Address = tocall
    String  path;
    String  addressee;
    String  payload;
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

    String  generateStatusPacket(const String& callsign, const String& tocall, const String& path, const String& status);
    String  generateMessagePacket(const String& callsign, const String& tocall, const String& path, const String& addressee, const String& message);

    String  generateDigipeatedPacket(const String& packet, const String &callsign, const String& path);

    String  encodeGPSIntoBase91(float latitude, float longitude, float course, float speed, const String& symbol, bool sendAltitude = false, int altitude = 0, bool sendStandingUpdate = false, const String& packetType = "GPS");
    String  generateBase91GPSBeaconPacket(const String& callsign, const String& tocall, const String& path, const String& overlay, const String& gpsData);

    String  generateMiceGPSBeaconPacket(const String& miceMsgType, const String& callsign, const String& symbol, const String& overlay, const String& path, float latitude, float longitude, float course, float speed, int altitude);

    APRSPacket processReceivedPacket(const String& receivedPacket, int rssi, float snr, int freqError);

}

#endif