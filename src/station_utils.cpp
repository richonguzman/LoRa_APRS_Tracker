#include <TinyGPS++.h>
#include <SPIFFS.h>
#include "APRSPacketLib.h"
#include "station_utils.h"
#include "configuration.h"
#include "power_utils.h"
#include "lora_utils.h"
#include "bme_utils.h"
#include "display.h"
#include "logger.h"

extern Configuration        Config;
extern Beacon               *currentBeacon;
extern logging::Logger      logger;
extern TinyGPSPlus          gps;
extern uint8_t              myBeaconsIndex;
extern uint8_t              loraIndex;

extern uint32_t             lastTx;
extern uint32_t             lastTxTime;

extern bool                 sendUpdate;

extern uint32_t             txInterval;

extern double               currentHeading;
extern double               previousHeading;

extern double               lastTxLat;
extern double               lastTxLng;
extern double               lastTxDistance;

extern bool                 miceActive;
extern bool                 smartBeaconValue;
extern uint8_t              winlinkStatus;
extern bool                 winlinkCommentState;

extern int                  wxModuleType;

bool	    sendStandingUpdate      = false;
uint8_t     updateCounter           = Config.sendCommentAfterXBeacons;
bool        wxRequestStatus         = false;
uint32_t    wxRequestTime           = 0;

uint32_t    lastTelemetryTx         = millis();
uint32_t    telemetryTx             = millis();

String      firstNearTracker;
String      secondNearTracker;
String      thirdNearTracker;
String      fourthNearTracker;

uint32_t    lastDeleteListenedTracker;



namespace STATION_Utils {

    String getFirstNearTracker() {
        return String(firstNearTracker.substring(0, firstNearTracker.indexOf(",")));
    }

    String getSecondNearTracker() {
        return String(secondNearTracker.substring(0, secondNearTracker.indexOf(",")));
    }

    String getThirdNearTracker() {
        return String(thirdNearTracker.substring(0, thirdNearTracker.indexOf(",")));
    }

    String getFourthNearTracker() {
        return String(fourthNearTracker.substring(0, fourthNearTracker.indexOf(",")));
    }

    void deleteListenedTrackersbyTime() {
        String firstNearTrackermillis, secondNearTrackermillis, thirdNearTrackermillis, fourthNearTrackermillis;
        uint32_t firstTrackermillis, secondTrackermillis, thirdTrackermillis, fourthTrackermillis;
        if (firstNearTracker != "") {
            firstNearTrackermillis = firstNearTracker.substring(firstNearTracker.indexOf(",") + 1);
            firstTrackermillis = firstNearTrackermillis.toInt();
            if ((millis() - firstTrackermillis) > Config.rememberStationTime*60*1000) {
                firstNearTracker = "";
            }
        }
        if (secondNearTracker != "") {
            secondNearTrackermillis = secondNearTracker.substring(secondNearTracker.indexOf(",") + 1);
            secondTrackermillis = secondNearTrackermillis.toInt();
            if ((millis() - secondTrackermillis) > Config.rememberStationTime*60*1000) {
                secondNearTracker = "";
            }
        }
        if (thirdNearTracker != "") {
            thirdNearTrackermillis = thirdNearTracker.substring(thirdNearTracker.indexOf(",") + 1);
            thirdTrackermillis = thirdNearTrackermillis.toInt();
            if ((millis() - thirdTrackermillis) > Config.rememberStationTime*60*1000) {
                thirdNearTracker = "";
            }
        }
        if (fourthNearTracker != "") {
            fourthNearTrackermillis = fourthNearTracker.substring(fourthNearTracker.indexOf(",") + 1);
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

    void orderListenedTrackersByDistance(const String& callsign, float distance, float course) {
        String firstNearTrackerDistance, secondNearTrackerDistance, thirdNearTrackerDistance, fourthNearTrackerDistance, newTrackerInfo, firstNearTrackerCallsign, secondNearTrackerCallsign,thirdNearTrackerCallsign, fourthNearTrackerCallsign;
        newTrackerInfo = callsign + "> " + String(distance,2) + "km " + String(int(course)) + "," + String(millis());
        float firstDistance   = 0.0;
        float secondDistance  = 0.0;
        float thirdDistance   = 0.0;
        float fourthDistance  = 0.0;
        if (firstNearTracker != "") {
            firstNearTrackerCallsign = firstNearTracker.substring(0, firstNearTracker.indexOf(">"));
            firstNearTrackerDistance = firstNearTracker.substring(firstNearTracker.indexOf(">") + 1, firstNearTracker.indexOf("km"));
            firstDistance = firstNearTrackerDistance.toFloat();
        }
        if (secondNearTracker != "") {
            secondNearTrackerCallsign = secondNearTracker.substring(0, secondNearTracker.indexOf(">"));
            secondNearTrackerDistance = secondNearTracker.substring(secondNearTracker.indexOf(">") + 1, secondNearTracker.indexOf("km"));
            secondDistance = secondNearTrackerDistance.toFloat();
        }
        if (thirdNearTracker != "") {
            thirdNearTrackerCallsign = thirdNearTracker.substring(0, thirdNearTracker.indexOf(">"));
            thirdNearTrackerDistance = thirdNearTracker.substring(thirdNearTracker.indexOf(">") + 1, thirdNearTracker.indexOf("km"));
            thirdDistance = thirdNearTrackerDistance.toFloat();
        }
        if (fourthNearTracker != "") {
            fourthNearTrackerCallsign = fourthNearTracker.substring(0, fourthNearTracker.indexOf(">"));
            fourthNearTrackerDistance = fourthNearTracker.substring(fourthNearTracker.indexOf(">") + 1, fourthNearTracker.indexOf("km"));
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
                        if (distance > secondDistance) {
                            firstNearTracker  = secondNearTracker;
                            secondNearTracker = newTrackerInfo;
                        } else {
                            firstNearTracker  = newTrackerInfo;
                        }
                    }
                } else if (callsign == secondNearTrackerCallsign) {
                    if (distance != secondDistance) {
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
        if (smartBeaconValue) {
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
        if (!sendUpdate && lastTx >= Config.standingUpdateTime * 60 * 1000) {
            sendUpdate = true;
            sendStandingUpdate = true;
        }
    }

    void checkSmartBeaconValue() {
        if (wxRequestStatus && (millis() - wxRequestTime) > 20000) {
            wxRequestStatus = false;
        }
        if(winlinkStatus == 0 && !wxRequestStatus) {
            smartBeaconValue = currentBeacon->smartBeaconState;
        } else {
            smartBeaconValue = false;
        }
    }

    void checkSmartBeaconState() {
        if (!smartBeaconValue) {
            uint32_t lastTxSmartBeacon = millis() - lastTxTime;
            if (lastTxSmartBeacon >= Config.nonSmartBeaconRate * 60 * 1000) {
                sendUpdate = true;
            }
        }
    }

    void sendBeacon(uint8_t type) {
        String packet, comment;
        int sendCommentAfterXBeacons;
        if (Config.bme.sendTelemetry && type == 1) { // WX
            if (miceActive) {
                packet = APRSPacketLib::generateMiceGPSBeacon(currentBeacon->micE, currentBeacon->callsign,"_", currentBeacon->overlay, Config.path, gps.location.lat(), gps.location.lng(), gps.course.deg(), gps.speed.knots(), gps.altitude.meters());
            } else {
                packet = APRSPacketLib::generateGPSBeaconPacket(currentBeacon->callsign, "APLRT1", Config.path, "/", APRSPacketLib::encodeGPS(gps.location.lat(),gps.location.lng(), gps.course.deg(), gps.speed.knots(), currentBeacon->symbol, Config.sendAltitude, gps.altitude.feet(), sendStandingUpdate, "Wx"));
            }
            if (wxModuleType != 0) {
                packet += BME_Utils::readDataSensor(0);
            } else {
                packet += ".../...g...t...r...p...P...h..b.....";
            }            
        } else {
            if (miceActive) {
                packet = APRSPacketLib::generateMiceGPSBeacon(currentBeacon->micE, currentBeacon->callsign, currentBeacon->symbol, currentBeacon->overlay, Config.path, gps.location.lat(), gps.location.lng(), gps.course.deg(), gps.speed.knots(), gps.altitude.meters());
            } else {
                packet = APRSPacketLib::generateGPSBeaconPacket(currentBeacon->callsign, "APLRT1", Config.path, currentBeacon->overlay, APRSPacketLib::encodeGPS(gps.location.lat(),gps.location.lng(), gps.course.deg(), gps.speed.knots(), currentBeacon->symbol, Config.sendAltitude, gps.altitude.feet(), sendStandingUpdate, "GPS"));
            }
        }
        if (winlinkCommentState) {
            comment = " winlink";
            sendCommentAfterXBeacons = 1;
        } else {
            comment = currentBeacon->comment;
            sendCommentAfterXBeacons = Config.sendCommentAfterXBeacons;
        }
        if (Config.sendBatteryInfo) {
            String batteryVoltage = POWER_Utils::getBatteryInfoVoltage();
            String batteryChargeCurrent = POWER_Utils::getBatteryInfoCurrent();
            #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
                comment += " Bat=" + batteryVoltage + "V (" + batteryChargeCurrent + "mA)";
            #endif
            #if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262)
                comment += " Bat=" + String(batteryVoltage.toFloat()/1000,2) + "V (" + batteryChargeCurrent + "%)";
            #endif
            #if defined(HELTEC_V3_GPS) || defined(HELTEC_WIRELESS_TRACKER)
                comment += " Bat=" + String(batteryVoltage.toFloat(),2) + "V";
            #endif
        }
        if (comment != "") {
            updateCounter++;
            if (updateCounter >= sendCommentAfterXBeacons) {
                packet += comment;
                updateCounter = 0;
            } 
        }        
        #ifdef HAS_TFT
            cleanTFT();
        #endif
        show_display("<<< TX >>>", "", packet,100);
        LoRa_Utils::sendNewPacket(packet);
        
        if (smartBeaconValue) {
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
            if (telemetryTx > 10 * 60 * 1000 && lastTx > 10 * 1000) {
                sendBeacon(1);
                lastTelemetryTx = millis();
            }
        }
    }

    void saveIndex(uint8_t type, uint8_t index) {
        String filePath;
        if (type == 0) {
            filePath = "/callsignIndex.txt";
        } else {
            filePath = "/freqIndex.txt";
        }
        File fileIndex = SPIFFS.open(filePath, "w");
        if(!fileIndex) {
            return;
        }
        String dataToSave = String(index);
        if (fileIndex.println(dataToSave)) {
            if (type == 0) {
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Main", "New Callsign Index saved to SPIFFS");
            } else {
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Main", "New Frequency Index saved to SPIFFS");
            }
        } 
        fileIndex.close();
    }

    void loadIndex(uint8_t type) {
        String filePath;
        if (type == 0) {
            filePath = "/callsignIndex.txt";
        } else {
            filePath = "/freqIndex.txt";
        }
        File fileIndex = SPIFFS.open(filePath);
        if(!fileIndex) {
            return;
        } else {
            while (fileIndex.available()) {
                String firstLine = fileIndex.readStringUntil('\n');
                if (type == 0) {
                    myBeaconsIndex = firstLine.toInt();
                    logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Main", "Callsign Index: %s", firstLine);
                } else {
                    loraIndex = firstLine.toInt();
                    logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "LoRa", "LoRa Freq Index: %s", firstLine);
                }
            }
            fileIndex.close();
        }
    }

}