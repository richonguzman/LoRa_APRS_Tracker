#include <BluetoothSerial.h>
#include <OneButton.h>
#include <TinyGPS++.h>
#include <Arduino.h>
#include <logger.h>
#include <WiFi.h>
#include <vector>
#include "APRSPacketLib.h"
#include "notification_utils.h"
#include "bluetooth_utils.h"
#include "keyboard_utils.h"
#include "configuration.h"
#include "station_utils.h"
#include "button_utils.h"
#include "pins_config.h"
#include "power_utils.h"
#include "menu_utils.h"
#include "lora_utils.h"
#include "msg_utils.h"
#include "gps_utils.h"
#include "bme_utils.h"
#include "ble_utils.h"
#include "display.h"
#include "SPIFFS.h"
#include "utils.h"

Configuration                 Config;
HardwareSerial                neo6m_gps(1);
TinyGPSPlus                   gps;
#if !defined(TTGO_T_Beam_S3_SUPREME_V3) && !defined(HELTEC_V3_GPS)
BluetoothSerial               SerialBT;
#endif
#if defined(TTGO_T_Beam_V0_7) || defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_0_SX1268) || defined(ESP32_DIY_1W_LoRa_GPS) || defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3)
OneButton userButton          = OneButton(BUTTON_PIN, true, true);
#endif

String    versionDate         = "2024.01.05";

int       myBeaconsIndex      = 0;
int       myBeaconsSize       = Config.beacons.size();
Beacon    *currentBeacon      = &Config.beacons[myBeaconsIndex];

int       menuDisplay         = 100;

int       messagesIterator    = 0;
std::vector<String>           loadedAPRSMessages;

bool      displayEcoMode      = Config.displayEcoMode;
bool      displayState        = true;
uint32_t  displayTime         = millis();
uint32_t  refreshDisplayTime  = millis();

bool      sendUpdate          = true;
int       updateCounter       = Config.sendCommentAfterXBeacons;
bool	    sendStandingUpdate  = false;
bool      statusState         = true;
uint32_t  statusTime          = millis();
bool      bluetoothConnected  = false;
bool      bluetoothActive     = Config.bluetoothActive;
bool      sendBleToLoRa       = false;
String    BLEToLoRaPacket     = "";

bool      messageLed          = false;
uint32_t  messageLedTime      = millis();
int       lowBatteryPercent   = 21;

uint32_t  lastTelemetryTx     = millis();
uint32_t  telemetryTx         = millis();

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

int       screenBrightness    = 1;
bool      keyboardConnected   = false;
bool      keyDetected         = false;
uint32_t  keyboardTime        = millis();
String    messageCallsign     = "";
String    messageText         = "";

bool      flashlight          = false;
bool      digirepeaterActive  = false;
bool      sosActive           = false;
bool      disableGPS;

bool      miceActive          = false;

APRSPacket                    lastReceivedPacket;

logging::Logger               logger;

void setup() {
  Serial.begin(115200);

  #ifndef DEBUG
  logger.setDebugLevel(logging::LoggerLevel::LOGGER_LEVEL_INFO);
  #endif

  POWER_Utils::setup();

  setup_display();
  if (Config.notification.buzzerActive) {
    pinMode(Config.notification.buzzerPinTone, OUTPUT);
    pinMode(Config.notification.buzzerPinVcc, OUTPUT);
    NOTIFICATION_Utils::start();
  }
  if (Config.notification.ledTx){
    pinMode(Config.notification.ledTxPin, OUTPUT);
  }
  if (Config.notification.ledMessage){
    pinMode(Config.notification.ledMessagePin, OUTPUT);
  }
  if (Config.notification.ledFlashlight) {
    pinMode(Config.notification.ledFlashlightPin, OUTPUT);
  }

  show_display(" LoRa APRS", "", "      (TRACKER)", "", "Richonguzman / CA2RXU", "      " + versionDate, 4000);
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "RichonGuzman (CA2RXU) --> LoRa APRS Tracker/Station");
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Version: %s", versionDate);

  if (Config.ptt.active) {
    pinMode(Config.ptt.io_pin, OUTPUT);
    digitalWrite(Config.ptt.io_pin, Config.ptt.reverse ? HIGH : LOW);
  }

  MSG_Utils::loadNumMessages();
  GPS_Utils::setup();
  LoRa_Utils::setup();
  BME_Utils::setup();
  STATION_Utils::loadCallsignIndex();

  WiFi.mode(WIFI_OFF);
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "WiFi controller stopped");
  if (Config.bluetoothType==0) {
    BLE_Utils::setup();
  } else {
    #if !defined(TTGO_T_Beam_S3_SUPREME_V3) && !defined(HELTEC_V3_GPS)
    BLUETOOTH_Utils::setup();
    #endif
  }

  if (!Config.simplifiedTrackerMode) {
    #if defined(TTGO_T_Beam_V0_7) || defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_0_SX1268) || defined(ESP32_DIY_1W_LoRa_GPS) || defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3)
    userButton.attachClick(BUTTON_Utils::singlePress);
    userButton.attachLongPressStart(BUTTON_Utils::longPress);
    userButton.attachDoubleClick(BUTTON_Utils::doublePress);
    #endif
    KEYBOARD_Utils::setup();
  }

  POWER_Utils::lowerCpuFrequency();
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Smart Beacon is: %s", Utils::getSmartBeaconState());
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Setup Done!");
  menuDisplay = 0;
}

void loop() {
  currentBeacon = &Config.beacons[myBeaconsIndex];
  if (statusState) {
    Config.validateConfigFile(currentBeacon->callsign);
    miceActive = Config.validateMicE(currentBeacon->micE);
  }

  POWER_Utils::batteryManager();
  if (!Config.simplifiedTrackerMode) {
    #if defined(TTGO_T_Beam_V0_7) || defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_0_SX1268) || defined(ESP32_DIY_1W_LoRa_GPS) || defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3)
    userButton.tick();
    #endif
  }
  Utils::checkDisplayEcoMode();

  if (keyboardConnected) {
    KEYBOARD_Utils::read();
  }

  GPS_Utils::getData();
  bool gps_time_update = gps.time.isUpdated();
  bool gps_loc_update  = gps.location.isUpdated();
  GPS_Utils::setDateFromData();

  MSG_Utils::checkReceivedMessage(LoRa_Utils::receivePacket());
  MSG_Utils::ledNotification();
  Utils::checkFlashlight();
  STATION_Utils::checkListenedTrackersByTimeAndDelete();
  if (Config.bluetoothType==0) {
    BLE_Utils::sendToLoRa();
  } else {
    #if !defined(TTGO_T_Beam_S3_SUPREME_V3) && !defined(HELTEC_V3_GPS)
    BLUETOOTH_Utils::sendToLoRa();
    #endif
  }

  int currentSpeed = (int) gps.speed.kmph();

  if (gps_loc_update) {
    Utils::checkStatus();
    STATION_Utils::checkTelemetryTx();
  }
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
    STATION_Utils::sendBeacon("GPS");
  }
  if (gps_time_update) {
    STATION_Utils::checkSmartBeaconInterval(currentSpeed);
  }
  
  if (millis() - refreshDisplayTime >= 1000 || gps_time_update) {
    GPS_Utils::checkStartUpFrames();
    MENU_Utils::showOnScreen();
    refreshDisplayTime = millis();
  }
}