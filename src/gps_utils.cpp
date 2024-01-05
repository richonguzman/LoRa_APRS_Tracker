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
extern bool             disableGPS;
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
    #ifdef TTGO_T_LORA32_V2_1_TNC
    disableGPS = true;
    #else
    disableGPS = Config.disableGPS;
    #endif
    if (disableGPS) {
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
    if (disableGPS) {
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
    if (disableGPS) {
      return;
    }
    if ((millis() > 8000 && gps.charsProcessed() < 10)) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "GPS",
                "No GPS frames detected! Try to reset the GPS Chip with this "
                "firmware: https://github.com/lora-aprs/TTGO-T-Beam_GPS-reset");
      show_display("ERROR", "No GPS frames!", "Reset the GPS Chip", "https://github.com/lora-aprs/TTGO-T-Beam_GPS-reset", 2000);
    }
  }

}