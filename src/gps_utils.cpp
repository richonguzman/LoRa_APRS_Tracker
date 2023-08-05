#include <TinyGPS++.h>
#include "TimeLib.h"
#include "configuration.h"
#include "station_utils.h"
#include "pins_config.h"
#include "gps_utils.h"
#include "display.h"
#include "logger.h"
#include "utils.h"

extern Configuration    Config;
extern HardwareSerial   neo6m_gps;
extern TinyGPSPlus      gps;
extern Beacon           *currentBeacon;
extern logging::Logger  logger;

extern bool             sendUpdate;
extern bool		          sendStandingUpdate;

extern double           currentHeading;
extern double           previousHeading;

extern uint32_t         lastTxTime;
extern uint32_t         txInterval;
extern double           lastTxLat;
extern double           lastTxLng;
extern double           lastTxDistance;
extern uint32_t         lastTx;

namespace GPS_Utils {

  void setup() {
    if (Config.disableGps) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "GPS disabled");
      return;
    }
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

  void getData() {
    if (Config.disableGps) {
      return;
    }
    while (neo6m_gps.available() > 0) {
      gps.encode(neo6m_gps.read());
    }
  }

  void setDateFromData() {
    if (gps.time.isValid()) {
      setTime(gps.time.hour(), gps.time.minute(), gps.time.second(), gps.date.day(), gps.date.month(), gps.date.year());
    }
  }

  void calculateDistanceTraveled() {
    currentHeading  = gps.course.deg();
    lastTxDistance  = TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(), lastTxLat, lastTxLng);
    if (lastTx >= txInterval) {
      if (lastTxDistance > currentBeacon->minTxDist) {
        sendUpdate = true;
        sendStandingUpdate = false;
      }
    }
  }

  void calculateHeadingDelta(int speed) {
    int TurnMinAngle;
    double headingDelta = abs(previousHeading - currentHeading);
    if (lastTx > currentBeacon->minDeltaBeacon * 1000) {
      if (speed == 0) {
        TurnMinAngle = currentBeacon->turnMinDeg + (currentBeacon->turnSlope/(speed+1));
      } else {
        TurnMinAngle = currentBeacon->turnMinDeg + (currentBeacon->turnSlope/speed);
      }
      if (headingDelta > TurnMinAngle && lastTxDistance > currentBeacon->minTxDist) {
        sendUpdate = true;
        sendStandingUpdate = false;
      }
    }
  }

  void checkStartUpFrames() {
    if (Config.disableGps) {
      return;
    }
    if ((millis() > 8000 && gps.charsProcessed() < 10)) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "GPS",
                "No GPS frames detected! Try to reset the GPS Chip with this "
                "firmware: https://github.com/lora-aprs/TTGO-T-Beam_GPS-reset");
      show_display("ERROR", "No GPS frames!", "Reset the GPS Chip", "https://github.com/lora-aprs/TTGO-T-Beam_GPS-reset", 2000);
    }
  }

  String encondeGPS() {
    String encodedData;
    float Tlat, Tlon;
    float Tspeed=0, Tcourse=0;
    Tlat    = gps.location.lat();
    Tlon    = gps.location.lng();
    Tcourse = gps.course.deg();
    Tspeed  = gps.speed.knots();

    uint32_t aprs_lat, aprs_lon;
    aprs_lat = 900000000 - Tlat * 10000000;
    aprs_lat = aprs_lat / 26 - aprs_lat / 2710 + aprs_lat / 15384615;
    aprs_lon = 900000000 + Tlon * 10000000 / 2;
    aprs_lon = aprs_lon / 26 - aprs_lon / 2710 + aprs_lon / 15384615;

    String Ns, Ew, helper;
    if(Tlat < 0) { Ns = "S"; } else { Ns = "N"; }
    if(Tlat < 0) { Tlat= -Tlat; }

    if(Tlon < 0) { Ew = "W"; } else { Ew = "E"; }
    if(Tlon < 0) { Tlon= -Tlon; }

    char helper_base91[] = {"0000\0"};
    int i;
    utils::ax25_base91enc(helper_base91, 4, aprs_lat);
    for (i=0; i<4; i++) {
      encodedData += helper_base91[i];
    }
    utils::ax25_base91enc(helper_base91, 4, aprs_lon);
    for (i=0; i<4; i++) {
      encodedData += helper_base91[i];
    }
      
    encodedData += currentBeacon->symbol;

    if (Config.sendAltitude) {      // Send Altitude or... (APRS calculates Speed also)
      int Alt1, Alt2;
      int Talt;
      Talt = gps.altitude.feet();
      if(Talt>0) {
        double ALT=log(Talt)/log(1.002);
        Alt1= int(ALT/91);
        Alt2=(int)ALT%91;
      } else {
        Alt1=0;
        Alt2=0;
      }
      if (sendStandingUpdate) {
        encodedData += " ";
      } else {
        encodedData +=char(Alt1+33);
      }
      encodedData +=char(Alt2+33);
      encodedData +=char(0x30+33);
    } else {                      // ... just send Course and Speed
      utils::ax25_base91enc(helper_base91, 1, (uint32_t) Tcourse/4 );
      if (sendStandingUpdate) {
        encodedData += " ";
      } else {
        encodedData += helper_base91[0];
      }
      utils::ax25_base91enc(helper_base91, 1, (uint32_t) (log1p(Tspeed)/0.07696));
      encodedData += helper_base91[0];
      encodedData += "\x47";
    }
    return encodedData;
  }

}