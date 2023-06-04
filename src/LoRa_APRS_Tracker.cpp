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
#include "pins.h"
#include "power_management.h"
#include "lora_utils.h"
#include "utils.h"
#include "messages.h"

#define VERSION "2023.06.04"

logging::Logger logger;

String configurationFilePath = "/tracker_config.json";
Configuration   Config(configurationFilePath);
static int      myBeaconsIndex = 0;
int             myBeaconsSize  = Config.beacons.size();
Beacon          *currentBeacon = &Config.beacons[myBeaconsIndex];
PowerManagement powerManagement;
OneButton       userButton = OneButton(BUTTON_PIN, true, true);
HardwareSerial  neo6m_gps(1);
TinyGPSPlus     gps;

String getSmartBeaconState();

static int      menuDisplay           = 0;
static bool     displayEcoMode        = Config.displayEcoMode;
static uint32_t displayTime           = millis();
static bool     displayState          = true;
static bool     send_update           = true;
static int      messagesIterator      = 0;
static bool     statusAfterBootState  = true;

std::vector<String> loadedAPRSMessages;

void setup_gps() {
  neo6m_gps.begin(9600, SERIAL_8N1, GPS_TX, GPS_RX);
}

static void ButtonSinglePress() {
  if (menuDisplay == 0) {
    if (displayState) {
      send_update = true;
    } else {
      display_toggle(true);
      displayTime = millis();   
      displayState = true;  
    }
  } else if (menuDisplay == 1) {
    messages::loadMessagesFromMemory();
    if (messages::warnNoMessages()) {
      menuDisplay = 1;
    } else {
      menuDisplay = 10;
    }
  } else if (menuDisplay == 2) {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Loop", "%s", "wrl");
    messages::sendMessage("CD2RXU-15","wrl");
  } else if (menuDisplay == 10) {
    messagesIterator++;
    if (messagesIterator == messages::getNumAPRSMessages()) {
      menuDisplay = 1;
      messagesIterator = 0;
    } else {
      menuDisplay = 10;
    }
  } else if (menuDisplay == 20) {
    menuDisplay = 2;
  } else if (menuDisplay == 3) {
    show_display("__INFO____", "", "NOTHING YET ...", 1000);
  }
}

static void ButtonLongPress() {
  if (menuDisplay == 0) {
    if(myBeaconsIndex >= (myBeaconsSize-1)) {
      myBeaconsIndex = 0;
    } else {
      myBeaconsIndex++;
    }
    statusAfterBootState  = true;
    display_toggle(true);
    displayTime = millis();
    show_display("__INFO____", "", "CHANGING CALLSIGN ...", 1000);
  } else if (menuDisplay == 1) {
    messages::deleteFile();
    show_display("__INFO____", "", "ALL MESSAGES DELETED!", 2000);
    messages::loadNumMessages();
  } else if (menuDisplay == 2) {
    menuDisplay = 20;
  } else if (menuDisplay == 3) {
    if (!displayEcoMode) {
      displayEcoMode = true;
      show_display("__DISPLAY_", "", "   ECO MODE -> ON", 1000);
    } else {
      displayEcoMode = false;
      show_display("__DISPLAY_", "", "   ECO MODE -> OFF", 1000);
    }
  }
}

static void ButtonDoublePress() {
  display_toggle(true);
  if (menuDisplay == 0) {
    menuDisplay = 1;
  } else if (menuDisplay == 1) {
    menuDisplay = 2;
    messagesIterator = 0;
  } else if (menuDisplay == 2) {
    menuDisplay = 3;
  } else if (menuDisplay == 3 || menuDisplay == 20) {
    menuDisplay = 0;
    displayTime = millis();
  } 
}

void startingStatus() {
  delay(2000);
  LoRaUtils::sendNewPacket(currentBeacon->callsign + ">" + Config.destination + "," + Config.path + ":>" + Config.defaultStatus);
  statusAfterBootState = false;
}

// cppcheck-suppress unusedFunction
void setup() {
  Serial.begin(115200);

  powerManagement.setup();

  delay(500);
  
  setup_display();
  show_display(" LoRa APRS", "", "     Richonguzman", "     -- CD2RXU --", "", "      " VERSION, 4000);
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "RichonGuzman -> CD2RXU --> LoRa APRS Tracker/Station");
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Version: " VERSION);
  
  Config.validateConfigFile(currentBeacon->callsign);
  messages::loadNumMessages();

  setup_gps();
  LoRaUtils::setup();

  WiFi.mode(WIFI_OFF);
  btStop();
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "WiFi and BT controller stopped");
  esp_bt_controller_disable();
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "BT controller disabled");

  userButton.attachClick(ButtonSinglePress);
  userButton.attachLongPressStart(ButtonLongPress);
  userButton.attachDoubleClick(ButtonDoublePress);

  powerManagement.lowerCpuFrequency();
  delay(500);
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Smart Beacon is: %s", getSmartBeaconState());
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Setup Done!");
}

// cppcheck-suppress unusedFunction
void loop() {
  currentBeacon = &Config.beacons[myBeaconsIndex];

  userButton.tick();

  uint32_t lastDisplayTime = millis() - displayTime;
  if (displayEcoMode) {
    if (menuDisplay == 0 && lastDisplayTime >= Config.displayTimeout*1000) {
      display_toggle(false);
      displayState = false;
    }
  }

  powerManagement.obtainBatteryInfo();
  powerManagement.handleChargingLed();

  while (neo6m_gps.available() > 0) {
    gps.encode(neo6m_gps.read());
  }

  bool gps_time_update = gps.time.isUpdated();
  bool gps_loc_update  = gps.location.isUpdated();

  String loraPacket = "";
  int packetSize = LoRa.parsePacket();  // Listening for LoRa Packets
  if (packetSize) {
    while (LoRa.available()) {
      int inChar = LoRa.read();
      loraPacket += (char)inChar;
    }
    messages::checkReceivedMessage(loraPacket);
  }

  messages::checkListenedTrackersByTimeAndDelete();

  /*if (gps_loc_update != gps_loc_update_valid) {
    gps_loc_update_valid = gps_loc_update;
    if (gps_loc_update) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Loop", "GPS fix state went to VALID");
    } else {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Loop", "GPS fix state went to INVALID");
    }
  }*/

  static double       currentHeading          = 0;
  static double       previousHeading         = 0;

  if (gps.time.isValid()) {
    setTime(gps.time.hour(), gps.time.minute(), gps.time.second(), gps.date.day(), gps.date.month(), gps.date.year());
  }

  static double   lastTxLat             = 0.0;
  static double   lastTxLng             = 0.0;
  static double   lastTxDistance        = 0.0;
  static uint32_t txInterval            = 60000L;
  static uint32_t lastTxTime            = millis();
  static bool		  sendStandingUpdate 		= false;
  int 			      currentSpeed 			    = (int)gps.speed.kmph();

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
    APRSMessage msg;
    msg.setSource(currentBeacon->callsign);
    msg.setDestination(Config.destination);
    msg.setPath(Config.path);
    

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

    LoRaUtils::sendNewPacket(data);

    if (currentBeacon->smartBeaconState) {
      lastTxLat       = gps.location.lat();
      lastTxLng       = gps.location.lng();
      previousHeading = currentHeading;
      lastTxDistance  = 0.0;
    }
    lastTxTime = millis();
    send_update = false;

    if (Config.defaultStatusAfterBoot && statusAfterBootState) {
      startingStatus();
    }
  }

  if (gps_time_update) {
    switch (menuDisplay) { // Graphic Menu is in here!!!!
      case 1:
        show_display("__MENU_1__", "", "1P -> Read Msg (" + String(messages::getNumAPRSMessages()) + ")", "LP -> Delete Msg", "2P -> Menu 2");
        break;
      case 2:
        show_display("__MENU_2__", "", "1P -> Weather Report", "LP -> Listen Trackers", "2P -> Menu 3");
        break;
      case 3:
        show_display("__MENU_3__", "", "1P -> Nothing Yet", "LP -> Display EcoMode", "2P -> (Back) Tracking");
        break;

      case 10:            // Display Received/Saved APRS Messages
        {
          String msgSender      = loadedAPRSMessages[messagesIterator].substring(0, loadedAPRSMessages[messagesIterator].indexOf(","));
          String restOfMessage  = loadedAPRSMessages[messagesIterator].substring(loadedAPRSMessages[messagesIterator].indexOf(",")+1);
          String msgGate        = restOfMessage.substring(0,restOfMessage.indexOf(","));
          String msgText        = restOfMessage.substring(restOfMessage.indexOf(",")+1);
          show_display("MSG_APRS>", msgSender + "-->" + msgGate, msgText, "", "", "               Next>");
        }
        break;

      case 20:            // Display Heared Tracker/Stations
        show_display("LISTENING>", messages::getFirstNearTracker(), messages::getSecondNearTracker(), messages::getThirdNearTracker(), messages::getFourthNearTracker(), "<Back");
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
        if (messages::getNumAPRSMessages() > 0){
          fourthRowMainMenu = "*** MESSAGES: " + String(messages::getNumAPRSMessages()) + " ***";
        }
                
        fifthRowMainMenu  = "LAST Rx = " + messages::getLastHeardTracker();
            
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

  if ((millis() > 5000 && gps.charsProcessed() < 10)) {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "GPS",
               "No GPS frames detected! Try to reset the GPS Chip with this "
               "firmware: https://github.com/lora-aprs/TTGO-T-Beam_GPS-reset");
    show_display("No GPS frames detected!", "Try to reset the GPS Chip", "https://github.com/lora-aprs/TTGO-T-Beam_GPS-reset", 2000);
  }
}

/// FUNCTIONS ///
String getSmartBeaconState() {
  if (currentBeacon->smartBeaconState) {
    return "On";
  }
  return "Off";
}