#ifdef ESP32
#include <esp_bt.h>
#endif
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <OneButton.h>
#include <TinyGPS++.h>
#include <logger.h>
#include <WiFi.h>
#include <LoRa.h>
#include <vector>
#include "configuration.h"
#include "station_utils.h"
#include "button_utils.h"
#include "pins_config.h"
#include "power_utils.h"
#include "menu_utils.h"
#include "lora_utils.h"
#include "msg_utils.h"
#include "gps_utils.h"
#include "display.h"
#include "SPIFFS.h"
#include "utils.h"


Configuration                 Config;
PowerManagement               powerManagement;
HardwareSerial                neo6m_gps(1);
TinyGPSPlus                   gps;
NimBLECharacteristic*         pCharacteristic;
OneButton userButton          = OneButton(BUTTON_PIN, true, true);

String    versionDate         = "2023.07.24";

int       myBeaconsIndex      = 0;
int       myBeaconsSize       = Config.beacons.size();
Beacon    *currentBeacon      = &Config.beacons[myBeaconsIndex];

int       menuDisplay         = 100;

int       messagesIterator    = 0;
std::vector<String>           loadedAPRSMessages;

bool      displayEcoMode      = Config.displayEcoMode;
bool      displayState        = true;
uint32_t  displayTime         = millis();

bool      sendUpdate          = true;
int       updateCounter       = Config.sendCommentAfterXBeacons;
bool		  sendStandingUpdate  = false;
bool      statusState         = true;

uint32_t  lastTx              = 0.0;
uint32_t  txInterval          = 60000L;
uint32_t  lastTxTime          = millis();
double    lastTxLat           = 0.0;
double    lastTxLng           = 0.0;
double    lastTxDistance      = 0.0;
double    currentHeading      = 0;
double    previousHeading     = 0;

uint32_t  menuTime            = millis();
bool      symbolAvailable     = true;

logging::Logger               logger;


void setup() {
  Serial.begin(115200);

  powerManagement.setup();
  
  setup_display();
  show_display(" LoRa APRS", "", "     Richonguzman", "     -- CD2RXU --", "", "      " + versionDate, 4000);
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "RichonGuzman -> CD2RXU --> LoRa APRS Tracker/Station");
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Version: %s", versionDate);

  if (Config.ptt.active) {
    pinMode(Config.ptt.io_pin, OUTPUT);
    digitalWrite(Config.ptt.io_pin, Config.ptt.reverse ? HIGH : LOW);
  }

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
  menuDisplay = 0;
}

void loop() {
  currentBeacon = &Config.beacons[myBeaconsIndex];
  if (statusState) {
    Config.validateConfigFile(currentBeacon->callsign);
  }

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
  if (!sendUpdate && gps_loc_update && currentBeacon->smartBeaconState) {
    GPS_Utils::calculateDistanceTraveled();
    if (!sendUpdate) {
      GPS_Utils::calculateHeadingDelta(currentSpeed);
    }
    STATION_Utils::checkStandingUpdateTime();
  }

  STATION_Utils::checkSmartBeaconState();

  if (sendUpdate && gps_loc_update) {
    STATION_Utils::sendBeacon();
  }

  if (gps_time_update) {
    MENU_Utils::showOnScreen();
    STATION_Utils::checkSmartBeaconInterval(currentSpeed);
  }
  GPS_Utils::checkStartUpFrames();
}