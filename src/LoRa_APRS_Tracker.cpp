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

Configuration                       Config;
HardwareSerial                      neo6m_gps(1);
TinyGPSPlus                         gps;
#ifdef HAS_BT_CLASSIC
BluetoothSerial                     SerialBT;
#endif
#ifdef BUTTON_PIN
OneButton userButton                = OneButton(BUTTON_PIN, true, true);
#endif

String      versionDate             = "2024.05.09";

uint8_t     myBeaconsIndex          = 0;
int         myBeaconsSize           = Config.beacons.size();
Beacon      *currentBeacon          = &Config.beacons[myBeaconsIndex];
uint8_t     loraIndex               = 0;
int         loraIndexSize           = Config.loraTypes.size();
LoraType    *currentLoRaType        = &Config.loraTypes[loraIndex];

int         menuDisplay             = 100;

int         messagesIterator        = 0;
std::vector<String>                 loadedAPRSMessages;
std::vector<String>                 loadedWLNKMails;
std::vector<String>                 outputMessagesBuffer;
std::vector<String>                 outputAckRequestBuffer;

bool        displayEcoMode          = Config.display.ecoMode;
bool        displayState            = true;
uint32_t    displayTime             = millis();
uint32_t    refreshDisplayTime      = millis();

bool        sendUpdate              = true;
uint8_t     updateCounter           = Config.sendCommentAfterXBeacons;
bool	    sendStandingUpdate      = false;
bool        statusState             = true;
uint32_t    statusTime              = millis();
bool        bluetoothConnected      = false;
bool        bluetoothActive         = Config.bluetoothActive;
bool        sendBleToLoRa           = false;
String      BLEToLoRaPacket         = "";

bool        messageLed              = false;
uint32_t    messageLedTime          = millis();
uint8_t     lowBatteryPercent       = 21;

uint32_t    lastTelemetryTx         = millis();
uint32_t    telemetryTx             = millis();

uint32_t    lastTx                  = 0.0;
uint32_t    txInterval              = 60000L;
uint32_t    lastTxTime              = millis();
double      lastTxLat               = 0.0;
double      lastTxLng               = 0.0;
double      lastTxDistance          = 0.0;
double      currentHeading          = 0;
double      previousHeading         = 0;

uint32_t    menuTime                = millis();
bool        symbolAvailable         = true;

uint32_t    bmeLastReading          = -60000;

uint8_t     screenBrightness        = 1;
bool        keyboardConnected       = false;
bool        keyDetected             = false;
uint32_t    keyboardTime            = millis();
String      messageCallsign         = "";
String      messageText             = "";

bool        flashlight              = false;
bool        digirepeaterActive      = false;
bool        sosActive               = false;
bool        disableGPS;

bool        miceActive              = false;

bool        smartBeaconValue        = true;

int         ackRequestNumber;
bool        ackRequestState         = false;
String      ackCallsignRequest      = "";
String      ackNumberRequest        = "";
uint32_t    lastMsgRxTime           = millis();
uint32_t    lastRetryTime           = millis();

uint8_t     winlinkStatus           = 0;
String      winlinkMailNumber       = "_?";
String      winlinkAddressee        = "";
String      winlinkSubject          = "";
String      winlinkBody             = "";
String      winlinkAlias            = "";
String      winlinkAliasComplete    = "";
bool        winlinkCommentState     = false;

bool        wxRequestStatus         = false;
uint32_t    wxRequestTime           = 0;
uint32_t    batteryMeasurmentTime   = 0;

APRSPacket                          lastReceivedPacket;

logging::Logger                     logger;

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
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "WiFi controller stopped");

    if (Config.bluetoothType==0) {
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
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Smart Beacon is: %s", Utils::getSmartBeaconState());
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

    if (keyboardConnected) KEYBOARD_Utils::read();
    #ifdef TTGO_T_DECK_GPS
    KEYBOARD_Utils::mouseRead();
    #endif

    GPS_Utils::getData();
    bool gps_time_update = gps.time.isUpdated();
    bool gps_loc_update  = gps.location.isUpdated();
    GPS_Utils::setDateFromData();

    MSG_Utils::checkReceivedMessage(LoRa_Utils::receivePacket());
    MSG_Utils::processOutputBuffer();
    MSG_Utils::ledNotification();
    Utils::checkFlashlight();
    STATION_Utils::checkListenedTrackersByTimeAndDelete();
    if (Config.bluetoothType == 0) {
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
    if (sendUpdate && gps_loc_update) STATION_Utils::sendBeacon("GPS");
    if (gps_time_update) STATION_Utils::checkSmartBeaconInterval(currentSpeed);
  
    if (millis() - refreshDisplayTime >= 1000 || gps_time_update) {
        GPS_Utils::checkStartUpFrames();
        MENU_Utils::showOnScreen();
        refreshDisplayTime = millis();
    }
}