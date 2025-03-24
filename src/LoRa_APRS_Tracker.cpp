/*___________________________________________________________________

██╗      ██████╗ ██████╗  █████╗      █████╗ ██████╗ ██████╗ ███████╗
██║     ██╔═══██╗██╔══██╗██╔══██╗    ██╔══██╗██╔══██╗██╔══██╗██╔════╝
██║     ██║   ██║██████╔╝███████║    ███████║██████╔╝██████╔╝███████╗
██║     ██║   ██║██╔══██╗██╔══██║    ██╔══██║██╔═══╝ ██╔══██╗╚════██║
███████╗╚██████╔╝██║  ██║██║  ██║    ██║  ██║██║     ██║  ██║███████║
╚══════╝ ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝    ╚═╝  ╚═╝╚═╝     ╚═╝  ╚═╝╚══════╝

      ████████╗██████╗  █████╗  ██████╗██╗  ██╗███████╗██████╗
      ╚══██╔══╝██╔══██╗██╔══██╗██╔════╝██║ ██╔╝██╔════╝██╔══██╗
         ██║   ██████╔╝███████║██║     █████╔╝ █████╗  ██████╔╝
         ██║   ██╔══██╗██╔══██║██║     ██╔═██╗ ██╔══╝  ██╔══██╗
         ██║   ██║  ██║██║  ██║╚██████╗██║  ██╗███████╗██║  ██║
         ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝

                       Ricardo Guzman - CA2RXU 
          https://github.com/richonguzman/LoRa_APRS_Tracker
             (donations : http://paypal.me/richonguzman)
____________________________________________________________________*/

#include <BluetoothSerial.h>
#include <APRSPacketLib.h>
#include <TinyGPS++.h>
#include <Arduino.h>
#include <logger.h>
#include <WiFi.h>
#include "smartbeacon_utils.h"
#include "bluetooth_utils.h"
#include "keyboard_utils.h"
#include "joystick_utils.h"
#include "configuration.h"
#include "battery_utils.h"
#include "station_utils.h"
#include "board_pinout.h"
#include "button_utils.h"
#include "power_utils.h"
#include "sleep_utils.h"
#include "menu_utils.h"
#include "lora_utils.h"
#include "wifi_utils.h"
#include "msg_utils.h"
#include "gps_utils.h"
#include "web_utils.h"
#include "ble_utils.h"
#include "wx_utils.h"
#include "display.h"
#include "utils.h"
#ifdef HAS_TOUCHSCREEN
#include "touch_utils.h"
#endif

Configuration                       Config;
HardwareSerial                      gpsSerial(1);
TinyGPSPlus                         gps;
#ifdef HAS_BT_CLASSIC
    BluetoothSerial                 SerialBT;
#endif

String      versionDate             = "2025.03.24";

uint8_t     myBeaconsIndex          = 0;
int         myBeaconsSize           = Config.beacons.size();
Beacon      *currentBeacon          = &Config.beacons[myBeaconsIndex];
uint8_t     loraIndex               = 0;
int         loraIndexSize           = Config.loraTypes.size();
LoraType    *currentLoRaType        = &Config.loraTypes[loraIndex];

int         menuDisplay             = 100;

bool        statusState             = true;
bool        displayEcoMode          = Config.display.ecoMode;
bool        displayState            = true;
uint32_t    displayTime             = millis();
uint32_t    refreshDisplayTime      = millis();

bool        sendUpdate              = true;

bool        bluetoothActive         = Config.bluetooth.active;
bool        bluetoothConnected      = false;

uint32_t    lastTx                  = 0.0;
uint32_t    txInterval              = 60000L;
uint32_t    lastTxTime              = 0;
double      lastTxLat               = 0.0;
double      lastTxLng               = 0.0;
double      lastTxDistance          = 0.0;

uint32_t    menuTime                = millis();

bool        flashlight              = false;
bool        digipeaterActive        = false;
bool        sosActive               = false;

bool        miceActive              = false;

bool        smartBeaconActive       = true;

int         ackRequestNumber;

uint32_t    lastGPSTime             = 0;

APRSPacket                          lastReceivedPacket;

logging::Logger                     logger;
//#define DEBUG

extern bool gpsIsActive;

void setup() {
    Serial.begin(115200);

    #ifndef DEBUG
        logger.setDebugLevel(logging::LoggerLevel::LOGGER_LEVEL_INFO);
    #endif

    POWER_Utils::setup();
    displaySetup();
    POWER_Utils::externalPinSetup();

    STATION_Utils::loadIndex(0);    // callsign Index
    STATION_Utils::loadIndex(1);    // lora freq settins Index
    STATION_Utils::nearTrackerInit();
    startupScreen(loraIndex, versionDate);

    WIFI_Utils::checkIfWiFiAP();

    MSG_Utils::loadNumMessages();
    GPS_Utils::setup();
    currentLoRaType = &Config.loraTypes[loraIndex];
    LoRa_Utils::setup();
    Utils::i2cScannerForPeripherals();
    WX_Utils::setup();
    
    ackRequestNumber = random(1,999);

    WiFi.mode(WIFI_OFF);
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Main", "WiFi controller stopped");

    if (bluetoothActive) {
        if (Config.bluetooth.useBLE) {
            BLE_Utils::setup();
        } else {
            #ifdef HAS_BT_CLASSIC
                BLUETOOTH_Utils::setup();
            #endif
        }
    }

    if (!Config.simplifiedTrackerMode) {
        #ifdef BUTTON_PIN
            BUTTON_Utils::setup();
        #endif
        #ifdef HAS_JOYSTICK
            JOYSTICK_Utils::setup();
        #endif
        KEYBOARD_Utils::setup();
        #ifdef HAS_TOUCHSCREEN
            TOUCH_Utils::setup();
        #endif
    }

    POWER_Utils::lowerCpuFrequency();
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Main", "Smart Beacon is: %s", Utils::getSmartBeaconState());
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Setup Done!");
    menuDisplay = 0;
}

void loop() {
    currentBeacon = &Config.beacons[myBeaconsIndex];
    if (statusState) {
        if (Config.validateConfigFile(currentBeacon->callsign)) {
            KEYBOARD_Utils::rightArrow();
            currentBeacon = &Config.beacons[myBeaconsIndex];
        }
        miceActive = Config.validateMicE(currentBeacon->micE);
    }
    POWER_Utils::batteryManager();

    SMARTBEACON_Utils::checkSettings(currentBeacon->smartBeaconSetting);
    SMARTBEACON_Utils::checkState();

    if (!Config.simplifiedTrackerMode) {
        #ifdef BUTTON_PIN
            BUTTON_Utils::loop();
        #endif
    }

    Utils::checkDisplayEcoMode();

    KEYBOARD_Utils::read();
    #ifdef HAS_JOYSTICK
        JOYSTICK_Utils::loop();
    #endif
    #ifdef HAS_TOUCHSCREEN
        TOUCH_Utils::loop();
    #endif


    ReceivedLoRaPacket packet = LoRa_Utils::receivePacket();

    MSG_Utils::checkReceivedMessage(packet);
    MSG_Utils::processOutputBuffer();
    MSG_Utils::clean15SegBuffer();

    if (bluetoothActive && bluetoothConnected) {
        if (Config.bluetooth.useBLE) {
            BLE_Utils::sendToPhone(packet.text.substring(3));
            BLE_Utils::sendToLoRa();
        } else {
            #ifdef HAS_BT_CLASSIC
                BLUETOOTH_Utils::sendToPhone(packet.text.substring(3));
                BLUETOOTH_Utils::sendToLoRa();
            #endif
        }
    }
    
    MSG_Utils::ledNotification();
    Utils::checkFlashlight();
    STATION_Utils::checkListenedTrackersByTimeAndDelete();

    lastTx = millis() - lastTxTime;
    if (gpsIsActive) {
        GPS_Utils::getData();
        bool gps_time_update = gps.time.isUpdated();
        bool gps_loc_update  = gps.location.isUpdated();
        GPS_Utils::setDateFromData();

        int currentSpeed = (int) gps.speed.kmph();

        if (gps_loc_update) {
            Utils::checkStatus();
            STATION_Utils::checkTelemetryTx();
        }

        if (!gps.location.isValid()) BATTERY_Utils::checkVoltageWithoutGPSFix();

        if (!sendUpdate && gps_loc_update && smartBeaconActive) {
            GPS_Utils::calculateDistanceTraveled();
            if (!sendUpdate) GPS_Utils::calculateHeadingDelta(currentSpeed);
            STATION_Utils::checkStandingUpdateTime();
        }
        SMARTBEACON_Utils::checkFixedBeaconTime();
        if (sendUpdate && gps_loc_update) STATION_Utils::sendBeacon(0);
        if (gps_time_update) SMARTBEACON_Utils::checkInterval(currentSpeed);

        if (millis() - refreshDisplayTime >= 1000 || gps_time_update) {
            GPS_Utils::checkStartUpFrames();
            MENU_Utils::showOnScreen();
            refreshDisplayTime = millis();
        }
        SLEEP_Utils::checkIfGPSShouldSleep();
    } else {
        if (millis() - lastGPSTime > txInterval) {
            SLEEP_Utils::gpsWakeUp();
        }
        STATION_Utils::checkStandingUpdateTime();
        if (millis() - refreshDisplayTime >= 1000) {
            MENU_Utils::showOnScreen();
            refreshDisplayTime = millis();
        }
    }
}

// eliminar keyboardConnected y reemplazar por validar si es distinto de 0x00 ?