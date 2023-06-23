#ifdef ESP32
#include <esp_bt.h>
#endif
//#include <APRS-Decoder.h>
#include <Arduino.h>
#include <LoRa.h>
#include <OneButton.h>
#include <TinyGPS++.h>
#include <WiFi.h>
#include <logger.h>
#include "SPIFFS.h"
#include <vector>
#include "configuration.h"
#include "display.h"
#include "pins_config.h"
#include "power_management.h"
#include "lora_utils.h"
#include "utils.h"
#include "msg_utils.h"
#include "button_utils.h"
#include "gps_utils.h"
#include "station_utils.h"

#define VERSION "2023.06.23"

logging::Logger logger;

Configuration   Config;

int             myBeaconsIndex = 0;
int             myBeaconsSize  = Config.beacons.size();
Beacon          *currentBeacon = &Config.beacons[myBeaconsIndex];
PowerManagement powerManagement;
OneButton       userButton = OneButton(BUTTON_PIN, true, true);
HardwareSerial  neo6m_gps(1);
TinyGPSPlus     gps;

String getSmartBeaconState();

int      menuDisplay           = 0;
bool     displayEcoMode        = Config.displayEcoMode;
uint32_t displayTime           = millis();
bool     displayState          = true;
bool     send_update           = true;
int      messagesIterator      = 0;
bool     statusAfterBootState  = true;

std::vector<String> loadedAPRSMessages;

double   lastTxLat, lastTxLng, lastTxDistance, currentHeading, previousHeading;
bool		  sendStandingUpdate 		= false;
uint32_t  lastTxTime = 0;
bool gps_time_update, gps_loc_update;

void setup() {
  Serial.begin(115200);

  powerManagement.setup();

  delay(500);
  
  setup_display();
  show_display(" LoRa APRS", "", "     Richonguzman", "     -- CD2RXU --", "", "      " VERSION, 4000);
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "RichonGuzman -> CD2RXU --> LoRa APRS Tracker/Station");
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Version: " VERSION);
  
  Config.validateConfigFile(currentBeacon->callsign);

  MSG_Utils::loadNumMessages();
  GPS_Utils::setup();
  LoRa_Utils::setup();

  WiFi.mode(WIFI_OFF);
  btStop();
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "WiFi and BT controller stopped");
  esp_bt_controller_disable();
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "BT controller disabled");

  userButton.attachClick(BUTTON_Utils::singlePress);
  userButton.attachLongPressStart(BUTTON_Utils::longPress);
  userButton.attachDoubleClick(BUTTON_Utils::doublePress);

  powerManagement.lowerCpuFrequency();
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Smart Beacon is: %s", getSmartBeaconState());
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Setup Done!");
}

void loop() {
  powerManagement.batteryManager();
  userButton.tick();
  utils::checkDisplayEcoMode();

  GPS_Utils::getData();
  gps_time_update = gps.time.isUpdated();
  gps_loc_update  = gps.location.isUpdated();
  GPS_Utils::setDateFromData();

  currentBeacon = &Config.beacons[myBeaconsIndex];
   
  MSG_Utils::checkReceivedMessage(LoRa_Utils::receivePacket());
  STATION_Utils::checkListenedTrackersInterval();

  /*if (gps_loc_update != gps_loc_update_valid) {
    gps_loc_update_valid = gps_loc_update;
    if (gps_loc_update) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Loop", "GPS fix state went to VALID");
    } else {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Loop", "GPS fix state went to INVALID");
    }
  }*/

  //double    currentHeading        = 0;
  //double    previousHeading       = 0;
  //double   lastTxLat             = 0.0;
  //double   lastTxLng             = 0.0;
  //double    lastTxDistance        = 0.0;
  static uint32_t txInterval      = 60000L;
  lastTxTime = millis();
  //bool		  sendStandingUpdate 		= false;
  int       currentSpeed 			    = (int)gps.speed.kmph();

  if (!send_update && gps_loc_update && currentBeacon->smartBeaconState) {
    uint32_t lastTx = millis() - lastTxTime;
    currentHeading  = gps.course.deg();
    lastTxDistance  = TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(), lastTxLat, lastTxLng);
    if (lastTx >= txInterval) {
      if (lastTxDistance > currentBeacon->minTxDist) {
        send_update = true;
        sendStandingUpdate = false;
      }
    }

    if (!send_update) {
      int TurnMinAngle;
      double headingDelta = abs(previousHeading - currentHeading);
      if (lastTx > currentBeacon->minDeltaBeacon * 1000) {
        if (currentSpeed == 0) {
					TurnMinAngle = currentBeacon->turnMinDeg + (currentBeacon->turnSlope/(currentSpeed+1));
				} else {
          TurnMinAngle = currentBeacon->turnMinDeg + (currentBeacon->turnSlope/currentSpeed);
				}
				if (headingDelta > TurnMinAngle && lastTxDistance > currentBeacon->minTxDist) {
          send_update = true;
          sendStandingUpdate = false;
        }
      }
    }
    if (!send_update && lastTx >= Config.standingUpdateTime*60*1000) {
			send_update = true;
			sendStandingUpdate = true;
		}
  }

  if (!currentBeacon->smartBeaconState) {
    uint32_t lastTx = millis() - lastTxTime;
    if (lastTx >= Config.nonSmartBeaconRate*60*1000) {
      send_update = true;
    }
  }

  if (send_update && gps_loc_update) {
    STATION_Utils::sendBeacon();
  }

  if (gps_time_update) {
    switch (menuDisplay) { // Graphic Menu is in here!!!!
      case 1:
        show_display("__MENU_1__", "", "1P -> Read Msg (" + String(MSG_Utils::getNumAPRSMessages()) + ")", "LP -> Delete Msg", "2P -> Menu 2");
        break;
      case 2:
        show_display("__MENU_2__", "", "1P -> Weather Report", "LP -> Listen Trackers", "2P -> Menu 3");
        break;
      case 3:
        show_display("__MENU_3__", "", "1P -> NOTHING YET", "LP -> Display EcoMode", "2P -> (Back) Tracking");
        break;

      case 10:            // Display Received/Saved APRS Messages
        {
          String msgSender      = loadedAPRSMessages[messagesIterator].substring(0,loadedAPRSMessages[messagesIterator].indexOf(","));
          String restOfMessage  = loadedAPRSMessages[messagesIterator].substring(loadedAPRSMessages[messagesIterator].indexOf(",")+1);
          String msgGate        = restOfMessage.substring(0,restOfMessage.indexOf(","));
          String msgText        = restOfMessage.substring(restOfMessage.indexOf(",")+1);
          show_display("MSG_APRS>", msgSender + "-->" + msgGate, msgText, "", "", "               Next>");
        }
        break;

      case 20:            // Display Heared Tracker/Stations
        show_display("LISTENING>", STATION_Utils::getFirstNearTracker(), STATION_Utils::getSecondNearTracker(), STATION_Utils::getThirdNearTracker(), STATION_Utils::getFourthNearTracker(), "<Back");
        break;

      case 0:       ///////////// MAIN MENU //////////////
        String hdopState, firstRowMainMenu, secondRowMainMenu, thirdRowMainMenu, fourthRowMainMenu, fifthRowMainMenu, sixthRowMainMenu;;

        firstRowMainMenu = currentBeacon->callsign;
        if (Config.showSymbolOnDisplay) {
          for (int j=firstRowMainMenu.length();j<9;j++) {
            firstRowMainMenu += " ";
          }
          firstRowMainMenu += currentBeacon->symbol;
        }
        
        secondRowMainMenu = utils::createDateString(now()) + "   " + utils::createTimeString(now());
        
        if (gps.hdop.hdop() > 5) {
          hdopState = "X";
        } else if (gps.hdop.hdop() > 2 && gps.hdop.hdop() < 5) {
          hdopState = "-";
        } else if (gps.hdop.hdop() <= 2) {
          hdopState = "+";
        }
        thirdRowMainMenu = String(gps.location.lat(), 4) + " " + String(gps.location.lng(), 4);
        for(int i = thirdRowMainMenu.length(); i < 18; i++) {
          thirdRowMainMenu += " ";
        }
        if (gps.satellites.value() > 9) { 
          thirdRowMainMenu += String(gps.satellites.value()) + hdopState;
        } else {
          thirdRowMainMenu += " " + String(gps.satellites.value()) + hdopState;
        }
        
        String fourthRowAlt = String(gps.altitude.meters(),0);
        fourthRowAlt.trim();
        for (int a=fourthRowAlt.length();a<4;a++) {
          fourthRowAlt = "0" + fourthRowAlt;
        }
        String fourthRowSpeed = String(gps.speed.kmph(),0);
        fourthRowSpeed.trim();
        for (int b=fourthRowSpeed.length(); b<3;b++) {
          fourthRowSpeed = " " + fourthRowSpeed;
        }
        String fourthRowCourse = String(gps.course.deg(),0);
        if (fourthRowSpeed == "  0") {
          fourthRowCourse = "---";
        } else {
          fourthRowCourse.trim();
          for(int c=fourthRowCourse.length();c<3;c++) {
            fourthRowCourse = "0" + fourthRowCourse;
          }
        }
        fourthRowMainMenu = "A=" + fourthRowAlt + "m  " + fourthRowSpeed + "km/h  " + fourthRowCourse;
        if (MSG_Utils::getNumAPRSMessages() > 0){
          fourthRowMainMenu = "*** MESSAGES: " + String(MSG_Utils::getNumAPRSMessages()) + " ***";
        }
                
        fifthRowMainMenu  = "LAST Rx = " + MSG_Utils::getLastHeardTracker();
            
        if (powerManagement.getBatteryInfoIsConnected()) {
          String batteryVoltage = powerManagement.getBatteryInfoVoltage();
          String batteryChargeCurrent = powerManagement.getBatteryInfoCurrent();
          if (batteryChargeCurrent.toInt() == 0) {
            sixthRowMainMenu = "Battery Charged " + String(batteryVoltage) + "V";
          } else if (batteryChargeCurrent.toInt() > 0) {
            sixthRowMainMenu = "Bat: " + String(batteryVoltage) + "V (charging)";
          } else {
            sixthRowMainMenu = "Battery " + String(batteryVoltage) + "V " + String(batteryChargeCurrent) + "mA";
          }
        } else {
          sixthRowMainMenu = "No Battery Connected" ;
        }
        show_display(String(firstRowMainMenu),
                    String(secondRowMainMenu),
                    String(thirdRowMainMenu),
                    String(fourthRowMainMenu),
                    String(fifthRowMainMenu),
                    String(sixthRowMainMenu));
        break;
    }
    

    if (currentBeacon->smartBeaconState) {
      if (currentSpeed < currentBeacon->slowSpeed) {
        txInterval = currentBeacon->slowRate * 1000;
      } else if (currentSpeed > currentBeacon->fastSpeed) {
        txInterval = currentBeacon->fastRate * 1000;
      } else {
        txInterval = min(currentBeacon->slowRate, currentBeacon->fastSpeed * currentBeacon->fastRate / currentSpeed) * 1000;
      }
    }
  }

  if ((millis() > 8000 && gps.charsProcessed() < 10)) {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "GPS",
               "No GPS frames detected! Try to reset the GPS Chip with this "
               "firmware: https://github.com/lora-aprs/TTGO-T-Beam_GPS-reset");
    show_display("ERROR", "No GPS frames!", "Reset the GPS Chip", "https://github.com/lora-aprs/TTGO-T-Beam_GPS-reset", 2000);
  }
}

/// FUNCTIONS ///
String getSmartBeaconState() {
  if (currentBeacon->smartBeaconState) {
    return "On";
  }
  return "Off";
}