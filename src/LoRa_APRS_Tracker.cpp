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
#include "menu_utils.h"

#define VERSION "2023.06.24"

logging::Logger             logger;

Configuration               Config;
int myBeaconsIndex          = 0;
int myBeaconsSize           = Config.beacons.size();
Beacon *currentBeacon       = &Config.beacons[myBeaconsIndex];
PowerManagement             powerManagement;
OneButton userButton        = OneButton(BUTTON_PIN, true, true);
HardwareSerial              neo6m_gps(1);
TinyGPSPlus                 gps;

int      menuDisplay        = 0;
bool     displayEcoMode     = Config.displayEcoMode;
uint32_t displayTime        = millis();
bool     displayState       = true;

bool     send_update        = true;
bool		 sendStandingUpdate = false;

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
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Smart Beacon is: %s", utils::getSmartBeaconState());
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Setup Done!");
}

void loop() {
  currentBeacon = &Config.beacons[myBeaconsIndex];

  powerManagement.batteryManager();
  userButton.tick();
  utils::checkDisplayEcoMode();

  GPS_Utils::getData();
  bool gps_time_update = gps.time.isUpdated();
  bool gps_loc_update  = gps.location.isUpdated();
  GPS_Utils::setDateFromData();
  MSG_Utils::checkReceivedMessage(LoRa_Utils::receivePacket());
  STATION_Utils::checkListenedTrackersByTimeAndDelete();

  int currentSpeed = (int)gps.speed.kmph();

  lastTx = millis() - lastTxTime;
  if (!send_update && gps_loc_update && currentBeacon->smartBeaconState) {
    GPS_Utils::calculateDistanceTraveled();
    if (!send_update) {
      GPS_Utils::calculateHeadingDelta(currentSpeed);
    }
    if (!send_update && lastTx >= Config.standingUpdateTime*60*1000) {
			send_update = true;
			sendStandingUpdate = true;
		}
  }

  STATION_Utils::checkSmartBeaconState();

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
    MENU_Utils::showOnScreen();
    STATION_Utils::checkSmartBeaconInterval(currentSpeed);
  }
  GPS_Utils::checkStartUpFrames();
}