#ifdef ESP32
#include <esp_bt.h>
#endif
#include <APRS-Decoder.h>
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
bool		 sendStandingUpdate     = false;

int      messagesIterator      = 0;
bool     statusAfterBootState  = true;

std::vector<String> loadedAPRSMessages;

double   currentHeading         = 0;
double   previousHeading        = 0;

double   lastTxLat              = 0.0;
double   lastTxLng              = 0.0;
double   lastTxDistance         = 0.0;
uint32_t txInterval             = 60000L;
uint32_t lastTxTime             = millis();
uint32_t lastTx;


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
  bool gps_time_update = gps.time.isUpdated();
  bool gps_loc_update  = gps.location.isUpdated();
  GPS_Utils::setDateFromData();

  /*if (gps.time.isValid()) {
    setTime(gps.time.hour(), gps.time.minute(), gps.time.second(), gps.date.day(), gps.date.month(), gps.date.year());
  }*/

  currentBeacon = &Config.beacons[myBeaconsIndex];
   
  MSG_Utils::checkReceivedMessage(LoRa_Utils::receivePacket());
  STATION_Utils::checkListenedTrackersByTimeAndDelete();

  int currentSpeed = (int)gps.speed.kmph();

  /*if (gps_loc_update != gps_loc_update_valid) {
    gps_loc_update_valid = gps_loc_update;
    if (gps_loc_update) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Loop", "GPS fix state went to VALID");
    } else {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Loop", "GPS fix state went to INVALID");
    }
  }*/

  //double   currentHeading        = 0;
  //double   previousHeading       = 0;
  //static double   lastTxLat             = 0.0;
  //static double   lastTxLng             = 0.0;
  //static double   lastTxDistance        = 0.0;
  //uint32_t txInterval            = 60000L;
  //static uint32_t lastTxTime            = millis();
  //bool		  sendStandingUpdate 		= false;
  

  lastTx = millis() - lastTxTime;
  if (!send_update && gps_loc_update && currentBeacon->smartBeaconState) {
    GPS_Utils::calculateDistanceTraveled();
    /*uint32_t lastTx = millis() - lastTxTime;
    currentHeading  = gps.course.deg();
    lastTxDistance  = TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(), lastTxLat, lastTxLng);
    if (lastTx >= txInterval) {
      if (lastTxDistance > currentBeacon->minTxDist) {
        send_update = true;
        sendStandingUpdate = false;
      }
    }*/

    if (!send_update) {
      GPS_Utils::calculateHeadingDelta(currentSpeed);
      /*int TurnMinAngle;
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
      }*/
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