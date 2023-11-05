#include <TinyGPS++.h>
#include "TimeLib.h"
#include "configuration.h"
#include "station_utils.h"
#include "pins_config.h"
#include "gps_utils.h"
#include "display.h"
#include "logger.h"
#include "utils.h"

#include "APRSPacketLib.h"

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

  String encondeGPS(String type) {
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
    //utils::ax25_base91enc(helper_base91, 4, aprs_lat);
    APRSPacketLib::ax25_base91enc(helper_base91, 4, aprs_lat);
    for (i=0; i<4; i++) {
      encodedData += helper_base91[i];
    }
    //utils::ax25_base91enc(helper_base91, 4, aprs_lon);
    APRSPacketLib::ax25_base91enc(helper_base91, 4, aprs_lon);
    for (i=0; i<4; i++) {
      encodedData += helper_base91[i];
    }
    if (type=="Wx") {
      encodedData += "_";
    } else {
      encodedData += currentBeacon->symbol;
    }

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
      //utils::ax25_base91enc(helper_base91, 1, (uint32_t) Tcourse/4 );
      APRSPacketLib::ax25_base91enc(helper_base91, 1, (uint32_t) Tcourse/4 );
      if (sendStandingUpdate) {
        encodedData += " ";
      } else {
        encodedData += helper_base91[0];
      }
      //utils::ax25_base91enc(helper_base91, 1, (uint32_t) (log1p(Tspeed)/0.07696));
      APRSPacketLib::ax25_base91enc(helper_base91, 1, (uint32_t) (log1p(Tspeed)/0.07696));
      encodedData += helper_base91[0];
      encodedData += "\x47";
    }
    return encodedData;
  }

}