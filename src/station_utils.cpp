#include <APRS-Decoder.h>
#include <logger.h>
#include "station_utils.h"
#include "configuration.h"
#include "msg_utils.h"
#include <vector>
#include "utils.h"
#include "lora_utils.h"
#include <TinyGPS++.h>
#include "display.h"

extern Configuration        Config;
extern Beacon               *currentBeacon;
extern logging::Logger      logger;
extern TinyGPSPlus          gps;
extern std::vector<String>  lastHeardStation;
extern std::vector<String>  lastHeardStation_temp;
extern String               fourthLine;

extern String     firstNearTracker;
extern String     secondNearTracker;
extern String     thirdNearTracker;
extern String     fourthNearTracker;
extern uint32_t   lastDeleteListenedTracker;
extern bool       send_update;
extern bool       gps_loc_update;
extern bool       statusAfterBootState;
extern uint32_t   lastTxTime;
extern double     currentHeading;
extern double     lastTxDistance;
extern double     lastTxLat;
extern double     lastTxLng;
extern double     previousHeading;
extern bool		    sendStandingUpdate;



namespace STATION_Utils {

String getFirstNearTracker() {
  return String(firstNearTracker.substring(0,firstNearTracker.indexOf(",")));
}

String getSecondNearTracker() {
  return String(secondNearTracker.substring(0,secondNearTracker.indexOf(",")));
}

String getThirdNearTracker() {
  return String(thirdNearTracker.substring(0,thirdNearTracker.indexOf(",")));
}

String getFourthNearTracker() {
  return String(fourthNearTracker.substring(0,fourthNearTracker.indexOf(",")));
}


void deleteListenedTrackersbyTime() {
  String firstNearTrackermillis, secondNearTrackermillis, thirdNearTrackermillis, fourthNearTrackermillis;
  uint32_t firstTrackermillis, secondTrackermillis, thirdTrackermillis, fourthTrackermillis;
  if (firstNearTracker != "") {
    firstNearTrackermillis = firstNearTracker.substring(firstNearTracker.indexOf(",")+1);
    firstTrackermillis = firstNearTrackermillis.toInt();
    if ((millis() - firstTrackermillis) > Config.rememberStationTime*60*1000) {
      firstNearTracker = "";
    }
  }
  if (secondNearTracker != "") {
    secondNearTrackermillis = secondNearTracker.substring(secondNearTracker.indexOf(",")+1);
    secondTrackermillis = secondNearTrackermillis.toInt();
    if ((millis() - secondTrackermillis) > Config.rememberStationTime*60*1000) {
      secondNearTracker = "";
    }
  }
  if (thirdNearTracker != "") {
    thirdNearTrackermillis = thirdNearTracker.substring(thirdNearTracker.indexOf(",")+1);
    thirdTrackermillis = thirdNearTrackermillis.toInt();
    if ((millis() - thirdTrackermillis) > Config.rememberStationTime*60*1000) {
      thirdNearTracker = "";
    }
  }
  if (fourthNearTracker != "") {
    fourthNearTrackermillis = fourthNearTracker.substring(fourthNearTracker.indexOf(",")+1);
    fourthTrackermillis = fourthNearTrackermillis.toInt();
    if ((millis() - fourthTrackermillis) > Config.rememberStationTime*60*1000) {
      fourthNearTracker = "";
    }
  }

  if (thirdNearTracker == "") {
    thirdNearTracker = fourthNearTracker;
    fourthNearTracker = "";
  } 
  if (secondNearTracker == "") {
    secondNearTracker = thirdNearTracker;
    thirdNearTracker = fourthNearTracker;
    fourthNearTracker = "";
  }
  if  (firstNearTracker == "") {
    firstNearTracker = secondNearTracker;
    secondNearTracker = thirdNearTracker;
    thirdNearTracker = fourthNearTracker;
    fourthNearTracker = "";
  }
  lastDeleteListenedTracker = millis();
}

void checkListenedTrackersInterval() {
  if (millis() - lastDeleteListenedTracker > Config.rememberStationTime*60*1000) {
    deleteListenedTrackersbyTime();
  }
}

void orderListenedTrackersByDistance(String callsign, float distance, float course) {
  String firstNearTrackerDistance, secondNearTrackerDistance, thirdNearTrackerDistance, fourthNearTrackerDistance, newTrackerInfo, firstNearTrackerCallsign, secondNearTrackerCallsign,thirdNearTrackerCallsign, fourthNearTrackerCallsign;
  newTrackerInfo = callsign + "> " + String(distance,2) + "km " + String(int(course)) + "," + String(millis());
  float firstDistance   = 0.0;
  float secondDistance  = 0.0;
  float thirdDistance   = 0.0;
  float fourthDistance  = 0.0;
  if (firstNearTracker != "") {
    firstNearTrackerCallsign = firstNearTracker.substring(0,firstNearTracker.indexOf(">"));
    firstNearTrackerDistance = firstNearTracker.substring(firstNearTracker.indexOf(">")+1,firstNearTracker.indexOf("km"));
    firstDistance = firstNearTrackerDistance.toFloat();
  }
  if (secondNearTracker != "") {
    secondNearTrackerCallsign = secondNearTracker.substring(0,secondNearTracker.indexOf(">"));
    secondNearTrackerDistance = secondNearTracker.substring(secondNearTracker.indexOf(">")+1,secondNearTracker.indexOf("km"));
    secondDistance = secondNearTrackerDistance.toFloat();
  }
  if (thirdNearTracker != "") {
    thirdNearTrackerCallsign = thirdNearTracker.substring(0,thirdNearTracker.indexOf(">"));
    thirdNearTrackerDistance = thirdNearTracker.substring(thirdNearTracker.indexOf(">")+1,thirdNearTracker.indexOf("km"));
    thirdDistance = thirdNearTrackerDistance.toFloat();
  }
  if (fourthNearTracker != "") {
    fourthNearTrackerCallsign = fourthNearTracker.substring(0,fourthNearTracker.indexOf(">"));
    fourthNearTrackerDistance = fourthNearTracker.substring(fourthNearTracker.indexOf(">")+1,fourthNearTracker.indexOf("km"));
    fourthDistance = fourthNearTrackerDistance.toFloat();
  } 

  if (firstNearTracker == "" && secondNearTracker == "" && thirdNearTracker == "" && fourthNearTracker == "") {
    firstNearTracker = newTrackerInfo;
  } else if (firstNearTracker != "" && secondNearTracker == "" && thirdNearTracker == "" && fourthNearTracker == "") {
    if (callsign != firstNearTrackerCallsign) {
      if (distance < firstDistance) {
        secondNearTracker = firstNearTracker;
        firstNearTracker  = newTrackerInfo;
      } else {
        secondNearTracker = newTrackerInfo;
      }
    } else { 
      if (distance != firstDistance) {
        firstNearTracker  = newTrackerInfo;
      }
    }
  } else if (firstNearTracker != "" && secondNearTracker != "" && thirdNearTracker == "" && fourthNearTracker == "") {
    if (callsign != firstNearTrackerCallsign && callsign != secondNearTrackerCallsign) {
      if (distance < firstDistance) {
        thirdNearTracker  = secondNearTracker;
        secondNearTracker = firstNearTracker;
        firstNearTracker  = newTrackerInfo;
      } else if (distance < secondDistance && distance >= firstDistance) {
        thirdNearTracker  = secondNearTracker;
        secondNearTracker = newTrackerInfo;
      } else if (distance >= secondDistance) {
        thirdNearTracker  = newTrackerInfo;
      }
    } else {  
      if (callsign == firstNearTrackerCallsign) {
        if (distance != firstDistance) {
          Serial.print("Distance Updated for : "); Serial.println(callsign);
          if (distance > secondDistance) {
            firstNearTracker  = secondNearTracker;
            secondNearTracker = newTrackerInfo;
          } else {
            firstNearTracker  = newTrackerInfo;
          }
        }
      } else if (callsign == secondNearTrackerCallsign) {
        if (distance != secondDistance) {
          Serial.print("Distance Updated for : "); Serial.println(callsign);
          if (distance < firstDistance) {
            secondNearTracker = firstNearTracker;
            firstNearTracker  = newTrackerInfo;
          } else {
            secondNearTracker = newTrackerInfo;
          }
        }
      }     
    }
  } else if (firstNearTracker != "" && secondNearTracker != "" && thirdNearTracker != "" && fourthNearTracker == "") {
    if (callsign != firstNearTrackerCallsign && callsign != secondNearTrackerCallsign && callsign != thirdNearTrackerCallsign) {
      if (distance < firstDistance) {
        fourthNearTracker = thirdNearTracker;
        thirdNearTracker  = secondNearTracker;
        secondNearTracker = firstNearTracker;
        firstNearTracker  = newTrackerInfo;
      } else if (distance >= firstDistance && distance < secondDistance) {
        fourthNearTracker = thirdNearTracker;
        thirdNearTracker  = secondNearTracker;
        secondNearTracker = newTrackerInfo;
      } else if (distance >= secondDistance && distance < thirdDistance) {
        fourthNearTracker = thirdNearTracker;
        thirdNearTracker  = newTrackerInfo;
      } else if (distance >= thirdDistance) {
        fourthNearTracker = newTrackerInfo;
      }
    } else {  
      if (callsign == firstNearTrackerCallsign) {
        if (distance != firstDistance) {
          Serial.print("Distance Updated for : "); Serial.println(callsign);
          if (distance > thirdDistance) {
            firstNearTracker  = secondNearTracker;
            secondNearTracker = thirdNearTracker;
            thirdNearTracker  = newTrackerInfo;
          } else if (distance <= thirdDistance && distance > secondDistance) {
            firstNearTracker  = secondNearTracker;
            secondNearTracker = newTrackerInfo;
          } else if (distance <= secondDistance) {
            firstNearTracker  = newTrackerInfo;
          }
        }
      } else if (callsign == secondNearTrackerCallsign) {
        if (distance != secondDistance) {
          Serial.print("Distance Updated for : "); Serial.println(callsign);
          if (distance > thirdDistance) {
            secondNearTracker = thirdNearTracker;
            thirdNearTracker  = newTrackerInfo;
          } else if (distance <= thirdDistance && distance > firstDistance) {
            secondNearTracker = newTrackerInfo;
          } else if (distance <= firstDistance) {
            secondNearTracker = firstNearTracker;
            firstNearTracker  = newTrackerInfo;
          }
        }
      } else if (callsign == thirdNearTrackerCallsign) {
        if (distance != thirdDistance) {
          Serial.print("Distance Updated for : "); Serial.println(callsign);
          if (distance <= firstDistance) {
            thirdNearTracker  = secondNearTracker;
            secondNearTracker = firstNearTracker;
            firstNearTracker  = newTrackerInfo;
          } else if (distance > firstDistance && distance <= secondDistance) {
            thirdNearTracker  = secondNearTracker;
            secondNearTracker = newTrackerInfo;
          } else if (distance > secondDistance) {
            thirdNearTracker  = newTrackerInfo;
          }
        }
      }  
    }
  } else if (firstNearTracker != "" && secondNearTracker != "" && thirdNearTracker != "" && fourthNearTracker != "") {
    if (callsign != firstNearTrackerCallsign && callsign != secondNearTrackerCallsign && callsign != thirdNearTrackerCallsign && callsign != fourthNearTrackerCallsign) {
      if (distance < firstDistance) {
        fourthNearTracker = thirdNearTracker;
        thirdNearTracker  = secondNearTracker;
        secondNearTracker = firstNearTracker;
        firstNearTracker  = newTrackerInfo;
      } else if (distance < secondDistance && distance >= firstDistance) {
        fourthNearTracker = thirdNearTracker;
        thirdNearTracker  = secondNearTracker;
        secondNearTracker = newTrackerInfo;
      } else if (distance < thirdDistance && distance >= secondDistance) {

        fourthNearTracker = thirdNearTracker;
        thirdNearTracker  = newTrackerInfo;
      } else if (distance < fourthDistance && distance >= thirdDistance) {
        fourthNearTracker = newTrackerInfo;
      }
    } else {
      if (callsign == firstNearTrackerCallsign) {
        if (distance != firstDistance) {
          Serial.print("Distance Updated for : "); Serial.println(callsign);
          if (distance > fourthDistance) {
            firstNearTracker  = secondNearTracker;
            secondNearTracker = thirdNearTracker;
            thirdNearTracker  = fourthNearTracker;
            fourthNearTracker = newTrackerInfo;
          } else if (distance > thirdDistance && distance <= fourthDistance) {
            firstNearTracker  = secondNearTracker;
            secondNearTracker = thirdNearTracker;
            thirdNearTracker  = newTrackerInfo;
          } else if (distance > secondDistance && distance <= thirdDistance) {
            firstNearTracker  = secondNearTracker;
            secondNearTracker = newTrackerInfo;
          } else if (distance <= secondDistance) {
            firstNearTracker  = newTrackerInfo;
          }
        }
      } else if (callsign == secondNearTrackerCallsign) {
        if (distance != secondDistance) {
          Serial.print("Distance Updated for : "); Serial.println(callsign);
          if (distance > fourthDistance) {
            secondNearTracker = thirdNearTracker;
            thirdNearTracker  = fourthNearTracker;
            fourthNearTracker = newTrackerInfo;
          } else if (distance > thirdDistance && distance <= fourthDistance) {
            secondNearTracker = thirdNearTracker;
            thirdNearTracker  = newTrackerInfo;
          } else if (distance > firstDistance && distance <= thirdDistance) {
            secondNearTracker = newTrackerInfo;
          } else if (distance <= firstDistance) {
            secondNearTracker = firstNearTracker;
            firstNearTracker  = newTrackerInfo;
          }
        }
      } else if (callsign == thirdNearTrackerCallsign) {
        if (distance != thirdDistance) {
          Serial.print("Distance Updated for : "); Serial.println(callsign);
          if (distance > fourthDistance) {
            thirdNearTracker  = fourthNearTracker;
            fourthNearTracker = newTrackerInfo;
          } else if (distance > secondDistance && distance <= fourthDistance) {
            thirdNearTracker  = newTrackerInfo;
          } else if (distance > firstDistance && distance <= secondDistance) {
            thirdNearTracker  = secondNearTracker;
            secondNearTracker = newTrackerInfo;
          } else if (distance <= firstDistance) {
            thirdNearTracker  = secondNearTracker;
            secondNearTracker = firstNearTracker;
            firstNearTracker  = newTrackerInfo;
          }
        }
      } else if (callsign == fourthNearTrackerCallsign) {
        if (distance != fourthDistance) {
          Serial.print("Distance Updated for : "); Serial.println(callsign);
          if (distance > thirdDistance) {
            fourthNearTracker = newTrackerInfo;
          } else if (distance > secondDistance && distance <= thirdDistance) {
            fourthNearTracker = thirdNearTracker;
            thirdNearTracker  = newTrackerInfo;
          } else if (distance > firstDistance && distance <= secondDistance) {
            fourthNearTracker = thirdNearTracker;
            thirdNearTracker  = secondNearTracker;
            secondNearTracker = newTrackerInfo;
          } else if (distance <= firstDistance) {
            fourthNearTracker = thirdNearTracker;
            thirdNearTracker  = secondNearTracker;
            secondNearTracker = firstNearTracker;
            firstNearTracker  = newTrackerInfo;
          }
        }
      }       
    }
  }
}

void sendBeacon() {
  APRSMessage msg;
  msg.setSource(currentBeacon->callsign);
  msg.setDestination("APLRT1");
  msg.setPath("WIDE1-1");
    
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

  String infoField = "!";
  infoField += Config.overlay;

  char helper_base91[] = {"0000\0"};
  int i;
  utils::ax25_base91enc(helper_base91, 4, aprs_lat);
  for (i=0; i<4; i++) {
    infoField += helper_base91[i];
  }
  utils::ax25_base91enc(helper_base91, 4, aprs_lon);
  for (i=0; i<4; i++) {
    infoField += helper_base91[i];
  }
    
  infoField += currentBeacon->symbol;

  if (Config.sendAltitude) {      // Send Altitude or... (APRS calculates Speed also)
    int Alt1, Alt2;
    int Talt;
    Talt = gps.altitude.feet();
    if(Talt>0){
      double ALT=log(Talt)/log(1.002);
      Alt1= int(ALT/91);
      Alt2=(int)ALT%91;
    }else{
      Alt1=0;
      Alt2=0;
    }
    if (sendStandingUpdate) {
      infoField += " ";
    } else {
      infoField +=char(Alt1+33);
    }
    infoField +=char(Alt2+33);
    infoField +=char(0x30+33);
  } else {                      // ... just send Course and Speed
    utils::ax25_base91enc(helper_base91, 1, (uint32_t) Tcourse/4 );
    if (sendStandingUpdate) {
      infoField += " ";
    } else {
      infoField += helper_base91[0];
    }
    utils::ax25_base91enc(helper_base91, 1, (uint32_t) (log1p(Tspeed)/0.07696));
    infoField += helper_base91[0];
    infoField += "\x47";
  }

  if (currentBeacon->comment != "") {
    infoField += currentBeacon->comment;
  }

  msg.getBody()->setData(infoField);
  String data = msg.encode();
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Loop", "%s", data.c_str());
  show_display("<<< TX >>>", "", data);

  LoRa_Utils::sendNewPacket(data);

  if (currentBeacon->smartBeaconState) {
    lastTxLat       = gps.location.lat();
    lastTxLng       = gps.location.lng();
    previousHeading = currentHeading;
    lastTxDistance  = 0.0;
  }
  lastTxTime = millis();
  send_update = false;

  if (statusAfterBootState) {
    utils::startingStatus();
  }
}

}