/* Copyright (C) 2025 Ricardo Guzman - CA2RXU
 * 
 * This file is part of LoRa APRS Tracker.
 * 
 * LoRa APRS Tracker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or 
 * (at your option) any later version.
 * 
 * LoRa APRS Tracker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with LoRa APRS Tracker. If not, see <https://www.gnu.org/licenses/>.
 */

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
#include "storage_utils.h"
#include "msg_utils.h"
#include "gps_utils.h"
#include "aprs_is_utils.h"
#include "web_utils.h"
#include "ble_utils.h"
#include "wx_utils.h"
#include "display.h"
#include "utils.h"
#ifdef HAS_TOUCHSCREEN
#include "touch_utils.h"
#endif
#ifdef USE_LVGL_UI
#include "lvgl_ui.h"
#endif


String      versionDate             = "2026-01-12";
String      versionNumber           = "2.4";
Configuration                       Config;
HardwareSerial                      gpsSerial(1);
TinyGPSPlus                         gps;
#ifdef HAS_BT_CLASSIC
    BluetoothSerial                 SerialBT;
#endif

uint8_t     myBeaconsIndex          = 0;
int         myBeaconsSize           = Config.beacons.size();
Beacon      *currentBeacon          = &Config.beacons[myBeaconsIndex];
uint8_t     loraIndex               = 0;
int         loraIndexSize           = Config.loraTypes.size();
LoraType    *currentLoRaType        = &Config.loraTypes[loraIndex];

int         menuDisplay             = 100;
uint32_t    menuTime                = millis();

bool        statusUpdate            = true;
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

bool        flashlight              = false;
bool        digipeaterActive        = false;
bool        sosActive               = false;

bool        miceActive              = false;

bool        smartBeaconActive       = true;

uint32_t    lastGPSTime             = 0;

APRSPacket                          lastReceivedPacket;

logging::Logger                     logger;
//#define DEBUG

extern bool gpsIsActive;

void setup() {
    Serial.begin(115200);

    // Turn off backlight immediately to avoid garbage display during init
    #if defined(USE_LVGL_UI) && defined(BOARD_BL_PIN)
        pinMode(BOARD_BL_PIN, OUTPUT);
        digitalWrite(BOARD_BL_PIN, LOW);
    #endif

    #ifndef DEBUG
        logger.setDebugLevel(logging::LoggerLevel::LOGGER_LEVEL_INFO);
    #endif

    POWER_Utils::setup();
    #ifndef USE_LVGL_UI
        displaySetup();  // Skip for LVGL - it does its own TFT init
    #endif
    POWER_Utils::externalPinSetup();

    POWER_Utils::lowerCpuFrequency();

    STATION_Utils::loadIndex(0);    // callsign Index
    STATION_Utils::loadIndex(1);    // lora freq settins Index
    STATION_Utils::nearStationInit();
    #ifdef USE_LVGL_UI
        LVGL_UI::showSplashScreen(loraIndex, versionDate.c_str());
        LVGL_UI::showInitScreen();
    #else
        startupScreen(loraIndex, versionDate);
    #endif

    // WiFi/BLE coexistence: WiFi first, then BLE
    // Start WiFi setup - will enter blocking web-conf mode if:
    // - NOCALL callsign (fresh install)
    // - No WiFi configured
    // - wifiAutoAP.active flag set
    #ifdef USE_LVGL_UI
        LVGL_UI::updateInitStatus("WiFi...");
    #endif
    WIFI_Utils::setup();

    // Then start BLE if active
    if (bluetoothActive) {
        #ifdef USE_LVGL_UI
            LVGL_UI::updateInitStatus("Bluetooth...");
        #endif
        if (Config.bluetooth.useBLE) {
            BLE_Utils::setup();
        } else {
            #ifdef HAS_BT_CLASSIC
                BLUETOOTH_Utils::setup();
            #endif
        }
    }

    #ifdef USE_LVGL_UI
        LVGL_UI::updateInitStatus("Storage...");
    #endif
    STORAGE_Utils::setup();
    MSG_Utils::loadNumMessages();

    #ifdef USE_LVGL_UI
        LVGL_UI::updateInitStatus("GPS...");
    #endif
    GPS_Utils::setup();

    #ifdef USE_LVGL_UI
        LVGL_UI::updateInitStatus("LoRa...");
    #endif
    currentLoRaType = &Config.loraTypes[loraIndex];
    LoRa_Utils::setup();
    Utils::i2cScannerForPeripherals();
    WX_Utils::setup();

    #ifdef BUTTON_PIN
        BUTTON_Utils::setup();
    #endif
    #ifdef HAS_JOYSTICK
        JOYSTICK_Utils::setup();
    #endif
    KEYBOARD_Utils::setup();
    #ifdef HAS_TOUCHSCREEN
        #ifndef USE_LVGL_UI
            TOUCH_Utils::setup();  // Only use old touch when LVGL not active
        #endif
    #endif

    #ifdef USE_LVGL_UI
        LVGL_UI::updateInitStatus("Ready!");
        delay(500);
        LVGL_UI::setup();  // LVGL handles its own touch - also cleans up init screens

        // Check if first boot web-conf needed (NOCALL or no WiFi configured)
        if (WIFI_Utils::needsWebConfig()) {
            LVGL_UI::showBootWebConfig();  // Blocking - never returns, reboots after config
        }
    #endif

    logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Main", "Smart Beacon is: %s", Utils::getSmartBeaconState());

    // Memory stats
    #ifdef BOARD_HAS_PSRAM
        Serial.printf("[Memory] PSRAM: %u KB total, %u KB free\n", ESP.getPsramSize()/1024, ESP.getFreePsram()/1024);
    #endif
    Serial.printf("[Memory] Heap: %u KB total, %u KB free\n", ESP.getHeapSize()/1024, ESP.getFreeHeap()/1024);

    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Setup Done!");
    menuDisplay = 0;
}

void loop() {
    currentBeacon = &Config.beacons[myBeaconsIndex];
    if (statusUpdate) {
        if (APRSPacketLib::checkNocall(currentBeacon->callsign)) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "Config", "Change your callsigns in WebConfig");
            displayShow("ERROR", "Callsigns = NOCALL!", "---> change it !!!", 2000);
            KEYBOARD_Utils::rightArrow();
            currentBeacon = &Config.beacons[myBeaconsIndex];
        }
        miceActive = APRSPacketLib::validateMicE(currentBeacon->micE);
    }

    SMARTBEACON_Utils::checkSettings(currentBeacon->smartBeaconSetting);
    SMARTBEACON_Utils::checkState();

    BATTERY_Utils::monitor();
    Utils::checkDisplayEcoMode();
    WIFI_Utils::checkWiFi();
    APRS_IS_Utils::checkConnection();

    #ifdef BUTTON_PIN
        BUTTON_Utils::loop();
    #endif
    KEYBOARD_Utils::read();
    #ifdef HAS_JOYSTICK
        JOYSTICK_Utils::loop();
    #endif
    #ifdef HAS_TOUCHSCREEN
        #ifndef USE_LVGL_UI
            TOUCH_Utils::loop();
        #endif
    #endif

    // Traiter les changements de configuration LoRa pendants (depuis ISR)
    LoRa_Utils::processPendingChanges();

    ReceivedLoRaPacket packet = LoRa_Utils::receivePacket();

    MSG_Utils::checkReceivedMessage(packet);
    MSG_Utils::processOutputBuffer();
    MSG_Utils::clean15SegBuffer();

    if (bluetoothActive && bluetoothConnected) {
        if (Config.bluetooth.useBLE) {
            BLE_Utils::sendToPhone(packet.text.substring(3));
            BLE_Utils::sendToLoRa();
            BLE_Utils::tryReadDeviceName();  // Try to read device name after connection
        } else {
            #ifdef HAS_BT_CLASSIC
                BLUETOOTH_Utils::sendToPhone(packet.text.substring(3));
                BLUETOOTH_Utils::sendToLoRa();
            #endif
        }
    }

    MSG_Utils::ledNotification();
    Utils::checkFlashlight();
    STATION_Utils::checkListenedStationsByTimeAndDelete();

    lastTx = millis() - lastTxTime;
    if (gpsIsActive) {
        GPS_Utils::getData();
        bool gps_time_update = gps.time.isUpdated();
        bool gps_loc_update  = gps.location.isUpdated();
        GPS_Utils::setDateFromData();

        int currentSpeed = (int) gps.speed.kmph();

        if (gps_loc_update) Utils::checkStatus();

        if (!sendUpdate && gps_loc_update && smartBeaconActive) {
            GPS_Utils::calculateDistanceTraveled();
            if (!sendUpdate) GPS_Utils::calculateHeadingDelta(currentSpeed);
            STATION_Utils::checkStandingUpdateTime();
        }
        SMARTBEACON_Utils::checkFixedBeaconTime();
        if (sendUpdate && gps_loc_update) STATION_Utils::sendBeacon();
        if (gps_time_update) SMARTBEACON_Utils::checkInterval(currentSpeed);

        if (millis() - refreshDisplayTime >= 1000 || gps_time_update) {
            GPS_Utils::checkStartUpFrames();
            #ifndef USE_LVGL_UI
                MENU_Utils::showOnScreen();
            #endif
            refreshDisplayTime = millis();
        }
        SLEEP_Utils::checkIfGPSShouldSleep();
    } else {
        if (millis() - lastGPSTime > txInterval) {
            SLEEP_Utils::gpsWakeUp();
        }
        STATION_Utils::checkStandingUpdateTime();
        if (millis() - refreshDisplayTime >= 1000) {
            #ifndef USE_LVGL_UI
                MENU_Utils::showOnScreen();
            #endif
            refreshDisplayTime = millis();
        }
    }

    #ifdef USE_LVGL_UI
        LVGL_UI::loop();
    #endif

    yield();
}