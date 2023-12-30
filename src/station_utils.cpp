#include <TinyGPS++.h>
#include <SPIFFS.h>
#include <vector>
#include "APRSPacketLib.h"
#include "station_utils.h"
#include "configuration.h"
#include "power_utils.h"
#include "lora_utils.h"
#include "bme_utils.h"
#include "gps_utils.h"
#include "display.h"
#include "logger.h"

extern Configuration        Config;
extern Beacon               *currentBeacon;
extern logging::Logger      logger;
extern TinyGPSPlus          gps;
extern std::vector<String>  lastHeardStation;
extern std::vector<String>  lastHeardStation_temp;
extern int                  myBeaconsIndex;

extern String               firstNearTracker;
extern String               secondNearTracker;
extern String               thirdNearTracker;
extern String               fourthNearTracker;

extern uint32_t             lastDeleteListenedTracker;
extern uint32_t             lastTx;
extern uint32_t             lastTxTime;

extern uint32_t             telemetryTx;
extern uint32_t             lastTelemetryTx;

extern bool                 sendUpdate;
extern int                  updateCounter;
extern bool                 sendStandingUpdate;

extern uint32_t             txInterval;
extern uint32_t             lastTx;

extern double               currentHeading;
extern double               previousHeading;

extern double               lastTxLat;
extern double               lastTxLng;
extern double               lastTxDistance;

extern bool                 miceActive;


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

  void checkListenedTrackersByTimeAndDelete() {
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
            //Serial.print("Distance Updated for : "); Serial.println(callsign);
            if (distance > secondDistance) {
              firstNearTracker  = secondNearTracker;
              secondNearTracker = newTrackerInfo;
            } else {
              firstNearTracker  = newTrackerInfo;
            }
          }
        } else if (callsign == secondNearTrackerCallsign) {
          if (distance != secondDistance) {
            //Serial.print("Distance Updated for : "); Serial.println(callsign);
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
            //Serial.print("Distance Updated for : "); Serial.println(callsign);
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
            //Serial.print("Distance Updated for : "); Serial.println(callsign);
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
            //Serial.print("Distance Updated for : "); Serial.println(callsign);
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
            //Serial.print("Distance Updated for : "); Serial.println(callsign);
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
            //Serial.print("Distance Updated for : "); Serial.println(callsign);
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
            //Serial.print("Distance Updated for : "); Serial.println(callsign);
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
            //Serial.print("Distance Updated for : "); Serial.println(callsign);
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

  void checkSmartBeaconInterval(int speed) {
    if (currentBeacon->smartBeaconState) {
      if (speed < currentBeacon->slowSpeed) {
        txInterval = currentBeacon->slowRate * 1000;
      } else if (speed > currentBeacon->fastSpeed) {
        txInterval = currentBeacon->fastRate * 1000;
      } else {
        txInterval = min(currentBeacon->slowRate, currentBeacon->fastSpeed * currentBeacon->fastRate / speed) * 1000;
      }
    }
  }

  void checkStandingUpdateTime() {
    if (!sendUpdate && lastTx >= Config.standingUpdateTime*60*1000) {
      sendUpdate = true;
      sendStandingUpdate = true;
    }
  }

  void checkSmartBeaconState() {
    if (!currentBeacon->smartBeaconState) {
      uint32_t lastTxSmartBeacon = millis() - lastTxTime;
      if (lastTxSmartBeacon >= Config.nonSmartBeaconRate*60*1000) {
        sendUpdate = true;
      }
    }
  }

  void sendBeacon(String type) {
    String packet;
    if (Config.bme.sendTelemetry && type == "Wx") {
      if (miceActive) {
        packet = APRSPacketLib::generateMiceGPSBeacon(currentBeacon->micE, currentBeacon->callsign,"_", currentBeacon->overlay, Config.path, gps.location.lat(), gps.location.lng(), gps.course.deg(), gps.speed.knots(), gps.altitude.meters());
      } else {
        packet = APRSPacketLib::generateGPSBeaconPacket(currentBeacon->callsign, "APLRT1", Config.path, "/", APRSPacketLib::encondeGPS(gps.location.lat(),gps.location.lng(), gps.course.deg(), gps.speed.knots(), currentBeacon->symbol, Config.sendAltitude, gps.altitude.feet(), sendStandingUpdate, "Wx"));
      }
      packet += BME_Utils::readDataSensor("APRS");
    } else {
      if (miceActive) {
        packet = APRSPacketLib::generateMiceGPSBeacon(currentBeacon->micE, currentBeacon->callsign, currentBeacon->symbol, currentBeacon->overlay, Config.path, gps.location.lat(), gps.location.lng(), gps.course.deg(), gps.speed.knots(), gps.altitude.meters());
      } else {
        packet = APRSPacketLib::generateGPSBeaconPacket(currentBeacon->callsign, "APLRT1", Config.path, currentBeacon->overlay, APRSPacketLib::encondeGPS(gps.location.lat(),gps.location.lng(), gps.course.deg(), gps.speed.knots(), currentBeacon->symbol, Config.sendAltitude, gps.altitude.feet(), sendStandingUpdate, "GPS"));
      }
    }
    if (currentBeacon->comment != "") {
      updateCounter++;
      if (updateCounter >= Config.sendCommentAfterXBeacons) {
        packet += currentBeacon->comment;
        updateCounter = 0;
      } 
    }
    if (Config.sendBatteryInfo) {
      String batteryVoltage = POWER_Utils::getBatteryInfoVoltage();
      String batteryChargeCurrent = POWER_Utils::getBatteryInfoCurrent();
      #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
      packet += " Bat=" + batteryVoltage + "V (" + batteryChargeCurrent + "mA)";
      #endif
      #if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262)
      packet += " Bat=" + String(batteryVoltage.toFloat()/1000,2) + "V (" + batteryChargeCurrent + "%)";
      #endif
    }
    show_display("<<< TX >>>", "", packet,100);
    LoRa_Utils::sendNewPacket(packet);
    
    if (currentBeacon->smartBeaconState) {
      lastTxLat       = gps.location.lat();
      lastTxLng       = gps.location.lng();
      previousHeading = currentHeading;
      lastTxDistance  = 0.0;
    }
    lastTxTime = millis();
    sendUpdate = false;
  }

  void checkTelemetryTx() {
    if (Config.bme.active && Config.bme.sendTelemetry) {
      lastTx = millis() - lastTxTime;
      telemetryTx = millis() - lastTelemetryTx;
      if (telemetryTx > 10*60*1000 && lastTx > 10*1000) {
        sendBeacon("Wx");
        lastTelemetryTx = millis();
      } 
    }
  }

  void saveCallsingIndex(int index) {
    File fileCallsignIndex = SPIFFS.open("/callsignIndex.txt", "w");
    if(!fileCallsignIndex){
      return;
    } 
    String dataToSave = String(index);

    if (fileCallsignIndex.println(dataToSave)) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "New Callsign Index saved to SPIFFS");
    } 
    fileCallsignIndex.close();
  }

  void loadCallsignIndex() {
    File fileCallsignIndex = SPIFFS.open("/callsignIndex.txt");
    if(!fileCallsignIndex){
      return;
    } else {
      while (fileCallsignIndex.available()) {
        String firstLine = fileCallsignIndex.readStringUntil('\n');
        myBeaconsIndex = firstLine.toInt();
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Callsign Index: %s", firstLine);
      }
      fileCallsignIndex.close();
    }
  }

}