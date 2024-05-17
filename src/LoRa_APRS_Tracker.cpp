/*__________________________________________________________________________________________________________________________________

██╗      ██████╗ ██████╗  █████╗      █████╗ ██████╗ ██████╗ ███████╗    ████████╗██████╗  █████╗  ██████╗██╗  ██╗███████╗██████╗ 
██║     ██╔═══██╗██╔══██╗██╔══██╗    ██╔══██╗██╔══██╗██╔══██╗██╔════╝    ╚══██╔══╝██╔══██╗██╔══██╗██╔════╝██║ ██╔╝██╔════╝██╔══██╗
██║     ██║   ██║██████╔╝███████║    ███████║██████╔╝██████╔╝███████╗       ██║   ██████╔╝███████║██║     █████╔╝ █████╗  ██████╔╝
██║     ██║   ██║██╔══██╗██╔══██║    ██╔══██║██╔═══╝ ██╔══██╗╚════██║       ██║   ██╔══██╗██╔══██║██║     ██╔═██╗ ██╔══╝  ██╔══██╗
███████╗╚██████╔╝██║  ██║██║  ██║    ██║  ██║██║     ██║  ██║███████║       ██║   ██║  ██║██║  ██║╚██████╗██║  ██╗███████╗██║  ██║
╚══════╝ ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝    ╚═╝  ╚═╝╚═╝     ╚═╝  ╚═╝╚══════╝       ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝

                                                    Ricardo Guzman - CA2RXU 
                                          https://github.com/richonguzman/LoRa_APRS_Tracker
                                            (donations : http://paypal.me/richonguzman)                                                                       
__________________________________________________________________________________________________________________________________*/

#include <BluetoothSerial.h>
#include <OneButton.h>
#include <TinyGPS++.h>
#include <Arduino.h>
#include <logger.h>
#include <WiFi.h>
#include "APRSPacketLib.h"
#include "bluetooth_utils.h"
#include "keyboard_utils.h"
#include "configuration.h"
#include "station_utils.h"
#include "boards_pinout.h"
#include "button_utils.h"
#include "power_utils.h"
#include "menu_utils.h"
#include "lora_utils.h"
#include "msg_utils.h"
#include "gps_utils.h"
#include "bme_utils.h"
#include "ble_utils.h"
#include "display.h"
#include "utils.h"

Configuration                       Config;
HardwareSerial                      neo6m_gps(1);
TinyGPSPlus                         gps;
#ifdef HAS_BT_CLASSIC
    BluetoothSerial                     SerialBT;
#endif
#ifdef BUTTON_PIN
    OneButton userButton                = OneButton(BUTTON_PIN, true, true);
#endif

String      versionDate             = "2024.05.17";

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

bool        bluetoothConnected      = false;
bool        sendBleToLoRa           = false;
String      BLEToLoRaPacket         = "";

uint32_t    lastTx                  = 0.0;
uint32_t    txInterval              = 60000L;
uint32_t    lastTxTime              = millis();
double      lastTxLat               = 0.0;
double      lastTxLng               = 0.0;
double      lastTxDistance          = 0.0;

uint32_t    menuTime                = millis();

bool        flashlight              = false;
bool        digirepeaterActive      = false;
bool        sosActive               = false;
bool        disableGPS;

bool        miceActive              = false;

bool        smartBeaconValue        = true;

int         ackRequestNumber;

APRSPacket                          lastReceivedPacket;

logging::Logger                     logger;
//#define DEBUG

void setup() {
    Serial.begin(115200);
    
    #ifndef DEBUG
        logger.setDebugLevel(logging::LoggerLevel::LOGGER_LEVEL_INFO);
    #endif

    POWER_Utils::setup();
    setup_display();
    POWER_Utils::externalPinSetup();

    STATION_Utils::loadIndex(0);
    STATION_Utils::loadIndex(1);
    startupScreen(loraIndex, versionDate);

    MSG_Utils::loadNumMessages();
    GPS_Utils::setup();
    currentLoRaType = &Config.loraTypes[loraIndex];
    LoRa_Utils::setup();
    BME_Utils::setup();
    
    ackRequestNumber = random(1,999);

    WiFi.mode(WIFI_OFF);
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Main", "WiFi controller stopped");

    if (Config.bluetoothType == 0 || Config.bluetoothType == 3) {
        BLE_Utils::setup();
    } else {
        #ifdef HAS_BT_CLASSIC
            BLUETOOTH_Utils::setup();
        #endif
    }

    if (!Config.simplifiedTrackerMode) {
        #ifdef BUTTON_PIN
            userButton.attachClick(BUTTON_Utils::singlePress);
            userButton.attachLongPressStart(BUTTON_Utils::longPress);
            userButton.attachDoubleClick(BUTTON_Utils::doublePress);
            userButton.attachMultiClick(BUTTON_Utils::multiPress);
        #endif
        KEYBOARD_Utils::setup();
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
    STATION_Utils::checkSmartBeaconValue();

    POWER_Utils::batteryManager();

    if (!Config.simplifiedTrackerMode) {
        #ifdef BUTTON_PIN
            userButton.tick();
        #endif
    }

    Utils::checkDisplayEcoMode();

    KEYBOARD_Utils::read();
    #ifdef TTGO_T_DECK_GPS
        KEYBOARD_Utils::mouseRead();
    #endif

    GPS_Utils::getData();
    bool gps_time_update = gps.time.isUpdated();
    bool gps_loc_update  = gps.location.isUpdated();
    GPS_Utils::setDateFromData();

    MSG_Utils::checkReceivedMessage(LoRa_Utils::receivePacket());
    MSG_Utils::processOutputBuffer();
    MSG_Utils::clean25SegBuffer();
    MSG_Utils::ledNotification();
    Utils::checkFlashlight();
    STATION_Utils::checkListenedTrackersByTimeAndDelete();
    if (Config.bluetoothType == 0 || Config.bluetoothType == 3) {
        BLE_Utils::sendToLoRa();
    } else {
        #ifdef HAS_BT_CLASSIC
            BLUETOOTH_Utils::sendToLoRa();
        #endif
    }

    int currentSpeed = (int) gps.speed.kmph();

    if (gps_loc_update) {
        Utils::checkStatus();
        STATION_Utils::checkTelemetryTx();
    }
    lastTx = millis() - lastTxTime;
    if (!sendUpdate && gps_loc_update && smartBeaconValue) {
        GPS_Utils::calculateDistanceTraveled();
        if (!sendUpdate) {
            GPS_Utils::calculateHeadingDelta(currentSpeed);
        }
        STATION_Utils::checkStandingUpdateTime();
    }
    STATION_Utils::checkSmartBeaconState();
    if (sendUpdate && gps_loc_update) STATION_Utils::sendBeacon(0);
    if (gps_time_update) STATION_Utils::checkSmartBeaconInterval(currentSpeed);
  
    if (millis() - refreshDisplayTime >= 1000 || gps_time_update) {
        GPS_Utils::checkStartUpFrames();
        MENU_Utils::showOnScreen();
        refreshDisplayTime = millis();
    }
}