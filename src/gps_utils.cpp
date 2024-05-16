#include <TinyGPS++.h>
#include "TimeLib.h"
#include "configuration.h"
#include "station_utils.h"
#include "boards_pinout.h"
#include "gps_utils.h"
#include "display.h"
#include "logger.h"

#include "APRSPacketLib.h"

#ifdef HIGH_GPS_BAUDRATE
    #define GPS_BAUD  115200
#else
    #define GPS_BAUD  9600
#endif

extern Configuration    Config;
extern HardwareSerial   neo6m_gps;      // cambiar a gpsSerial
extern TinyGPSPlus      gps;
extern Beacon           *currentBeacon;
extern logging::Logger  logger;
extern bool             disableGPS;
extern bool             sendUpdate;
extern bool		        sendStandingUpdate;

extern uint32_t         lastTxTime;
extern uint32_t         txInterval;
extern double           lastTxLat;
extern double           lastTxLng;
extern double           lastTxDistance;
extern uint32_t         lastTx;

double      currentHeading          = 0;
double      previousHeading         = 0;

namespace GPS_Utils {

    void setup() {
        #ifdef TTGO_T_LORA32_V2_1_TNC
            disableGPS = true;
        #else
            disableGPS = Config.disableGPS;
        #endif
        if (disableGPS) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "Main", "GPS disabled");
            return;
        }
        neo6m_gps.begin(GPS_BAUD, SERIAL_8N1, GPS_TX, GPS_RX);
    }

    void calculateDistanceCourse(const String& callsign, double checkpointLatitude, double checkPointLongitude) {
        double distanceKm = TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(), checkpointLatitude, checkPointLongitude) / 1000.0;
        double courseTo   = TinyGPSPlus::courseTo(gps.location.lat(), gps.location.lng(), checkpointLatitude, checkPointLongitude);
        STATION_Utils::deleteListenedTrackersbyTime();
        STATION_Utils::orderListenedTrackersByDistance(callsign, distanceKm, courseTo);
    }

    void getData() {
        if (disableGPS) return;
        while (neo6m_gps.available() > 0) {
            gps.encode(neo6m_gps.read());
        }
    }

    void setDateFromData() {
        if (gps.time.isValid()) setTime(gps.time.hour(), gps.time.minute(), gps.time.second(), gps.date.day(), gps.date.month(), gps.date.year());
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
        uint8_t TurnMinAngle;
        double headingDelta = abs(previousHeading - currentHeading);
        if (lastTx > currentBeacon->minDeltaBeacon * 1000) {
            if (speed == 0) {
                TurnMinAngle = currentBeacon->turnMinDeg + (currentBeacon->turnSlope/(speed + 1));
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
        if (disableGPS) return;
        if ((millis() > 10000 && gps.charsProcessed() < 10)) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "GPS",
                        "No GPS frames detected! Try to reset the GPS Chip with this "
                        "firmware: https://github.com/richonguzman/TTGO_T_BEAM_GPS_RESET");
            show_display("ERROR", "No GPS frames!", "Reset the GPS Chip", 2000);
        }
    }

}