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
#ifdef ESP32_BV5DJ_1W_LoRa_GPS
  #include <Adafruit_NeoPixel.h>
  Adafruit_NeoPixel myLED (LEDNUM, RGB_LED_PIN, NEO_GRB + NEO_KHZ400);
#endif

Configuration                 Config;
HardwareSerial                neo6m_gps(1);
TinyGPSPlus                   gps;
#if !defined(TTGO_T_Beam_S3_SUPREME_V3) && !defined(HELTEC_V3_GPS)
BluetoothSerial               SerialBT;
#endif
#ifdef BUTTON_PIN
OneButton userButton          = OneButton(BUTTON_PIN, true, true);
#endif
#ifdef ESP32_BV5DJ_1W_LoRa_GPS
OneButton userButtonU          = OneButton(BUTTON_UP, true, true);
OneButton userButtonD          = OneButton(BUTTON_DOWN, true, true);
OneButton userButtonL          = OneButton(BUTTON_LEFT, true, true);
OneButton userButtonR          = OneButton(BUTTON_RIGHT, true, true);
#endif

String    versionDate         = "2024.01.26c";

int       myBeaconsIndex      = 0;
int       myBeaconsSize       = Config.beacons.size();
Beacon    *currentBeacon      = &Config.beacons[myBeaconsIndex];

int       menuDisplay         = 100;

int       messagesIterator    = 0;
std::vector<String>           loadedAPRSMessages;

bool      displayEcoMode      = Config.display.ecoMode;
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

uint32_t  bmeLastReading      = -60000;

int       screenBrightness    = Config.display.brightness;
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
  #ifdef BATTERY_PIN
   pinMode(BATTERY_PIN, INPUT);    // This could or should be elsewhere, but this was my point of entry.  (Could be in main. HA5SZI)
  #endif

  setup_display();
  #ifndef ESP32_BV5DJ_1W_LoRa_GPS
  if (Config.notification.buzzerActive && Config.notification.buzzerPinTone >= 0 && Config.notification.buzzerPinVcc >= 0) {
    pinMode(Config.notification.buzzerPinTone, OUTPUT);
    pinMode(Config.notification.buzzerPinVcc, OUTPUT);
    NOTIFICATION_Utils::start();
  }
  if (Config.notification.ledTx && Config.notification.ledTxPin >= 0){
    pinMode(Config.notification.ledTxPin & 127, OUTPUT);
    digitalWrite(Config.notification.ledTxPin & 127, Config.notification.ledTxPin & 128 ? 1 : 0); //add 128 to pin num for HIGH
  }
  if (Config.notification.ledMessage  && Config.notification.ledMessagePin >= 0){
    pinMode(Config.notification.ledMessagePin & 127, OUTPUT);
    digitalWrite(Config.notification.ledMessagePin & 127, Config.notification.ledMessagePin & 128 ? 1 : 0); //add 128 to pin num for HIGH
    
  }
  if (Config.notification.ledFlashlight && Config.notification.ledFlashlightPin >= 0) {
    pinMode(Config.notification.ledFlashlightPin, OUTPUT);
  }
  #endif

  show_display(" LoRa APRS", "", "      (TRACKER)", "", "Richonguzman / CA2RXU", "      " + versionDate, 3000);
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "RichonGuzman (CA2RXU) --> LoRa APRS Tracker/Station");
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Version: %s", versionDate);

  #ifdef ESP32_BV5DJ_1W_LoRa_GPS
    myLED.begin();
    myLED.clear();
    myLED.setBrightness(Config.notification.ws2812brightness);
    NOTIFICATION_Utils::startRGB();
  #endif

#ifndef ESP32_BV5DJ_1W_LoRa_GPS
  if (Config.ptt.active && Config.ptt.io_pin >= 0) {
    pinMode(Config.ptt.io_pin, OUTPUT);
    digitalWrite(Config.ptt.io_pin, Config.ptt.reverse ? HIGH : LOW);
  }
#endif

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
    #if defined(BUTTON_PIN)
    userButton.attachClick(BUTTON_Utils::singlePress);
    userButton.attachLongPressStart(BUTTON_Utils::longPress);
    userButton.attachDoubleClick(BUTTON_Utils::doublePress);
      #ifdef ESP32_BV5DJ_1W_LoRa_GPS
        userButtonU.attachClick(KEYBOARD_Utils::leftArrow); //why changed Up/Left?!
        userButtonD.attachClick(KEYBOARD_Utils::downArrow);
        userButtonL.attachClick(KEYBOARD_Utils::upArrow); //why changed Up/Left?!
        userButtonR.attachClick(KEYBOARD_Utils::rightArrow);
      #endif
    #endif
    KEYBOARD_Utils::setup();
  }

  POWER_Utils::lowerCpuFrequency();
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Smart Beacon is: %s", Utils::getSmartBeaconState());
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Setup Done!");
  menuDisplay = 0;
  #ifdef ESP32_BV5DJ_1W_LoRa_GPS
   myLED.clear();
  #endif
}

void loop() {
  currentBeacon = &Config.beacons[myBeaconsIndex];
  if (statusState) {
    Config.validateConfigFile(currentBeacon->callsign);
    miceActive = Config.validateMicE(currentBeacon->micE);
  }

  POWER_Utils::batteryManager();
  if (!Config.simplifiedTrackerMode) {
    #if defined(BUTTON_PIN)
    userButton.tick();
      #ifdef ESP32_BV5DJ_1W_LoRa_GPS
      userButtonU.tick();
      userButtonD.tick();
      userButtonL.tick();
      userButtonR.tick();
      #endif
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
