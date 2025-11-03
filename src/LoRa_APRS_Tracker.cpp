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

// 首先包含平台兼容性头文件，确保所有宏冲突都得到解决
#include "platform_compat.h"

// 其他包含将由platform_compat.h处理平台差异
#include <APRSPacketLib.h>
#include <TinyGPS++.h>
#include <Arduino.h>
// 注意：logger.h和WiFi.h的包含现在由platform_compat.h处理
#include "smartbeacon_utils.h"

// 仅在非NRF52840平台上包含蓝牙相关头文件
#ifndef PLATFORM_NRF52840
#include "bluetooth_utils.h"
#include "ble_utils.h"
#endif

#include "keyboard_utils.h"
#include "joystick_utils.h"
#include "configuration.h"
#include "battery_utils.h"

// 仅在非NRF52840平台上包含station_utils.h（因为它依赖SPIFFS）
#ifndef PLATFORM_NRF52840
#include "station_utils.h"
#endif

#include "board_pinout.h"
#include "button_utils.h"
#include "power_utils.h"
#include "sleep_utils.h"
#include "menu_utils.h"
#include "lora_utils.h"

// 仅在非NRF52840平台上包含WiFi相关头文件
#ifndef PLATFORM_NRF52840
#include "wifi_utils.h"
#include "msg_utils.h"
#include "web_utils.h"
#endif

// 仅在非NRF52840平台上包含wx_utils.h（因为它依赖Adafruit_BME280）
#ifndef PLATFORM_NRF52840
#include "wx_utils.h"
#endif

#include "gps_utils.h"
#include "display.h"
#include "utils.h"
#ifdef HAS_TOUCHSCREEN
#include "touch_utils.h"
#endif

Configuration                       Config;

// 在NRF52840平台上使用Serial而不是HardwareSerial(1)
#ifdef PLATFORM_NRF52840
#define gpsSerial Serial
#else
HardwareSerial                      gpsSerial(1);
#endif

TinyGPSPlus                         gps;

// 仅在非NRF52840平台上定义蓝牙相关变量和包含蓝牙库
#ifndef PLATFORM_NRF52840
#ifdef HAS_BT_CLASSIC
    #include <BluetoothSerial.h>
    BluetoothSerial                 SerialBT;
#endif
#endif

String      versionDate             = "2025-09-29";

uint8_t     myBeaconsIndex          = 0;
int         myBeaconsSize           = Config.beacons.size();
Beacon      *currentBeacon          = &Config.beacons[myBeaconsIndex];
uint8_t     loraIndex               = 0;
int         loraIndexSize           = Config.loraTypes.size();
LoraType    *currentLoRaType        = &Config.loraTypes[loraIndex];

int         menuDisplay             = 100;
uint32_t    menuTime                = millis();

bool        statusState             = true;
bool        displayEcoMode          = Config.display.ecoMode;
bool        displayState            = true;
uint32_t    displayTime             = millis();
uint32_t    refreshDisplayTime      = millis();

bool        sendUpdate              = true;

// 仅在非NRF52840平台上定义蓝牙相关变量
#ifndef PLATFORM_NRF52840
bool        bluetoothActive         = Config.bluetooth.active;
bool        bluetoothConnected      = false;
#endif

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

    #ifndef DEBUG
        logger.setDebugLevel(LOG_LEVEL_INFO);
    #endif

    POWER_Utils::setup();
    displaySetup();
    POWER_Utils::externalPinSetup();

    // 仅在非NRF52840平台上执行STATION_Utils相关代码
    #ifndef PLATFORM_NRF52840
    STATION_Utils::loadIndex(0);    // callsign Index
    STATION_Utils::loadIndex(1);    // lora freq settins Index
    STATION_Utils::nearStationInit();
    #endif
    
    startupScreen(loraIndex, versionDate);

    // 仅在非NRF52840平台上执行WiFi和消息相关代码
    #ifndef PLATFORM_NRF52840
    WIFI_Utils::checkIfWiFiAP();
    MSG_Utils::loadNumMessages();
    #endif
    
    GPS_Utils::setup();
    currentLoRaType = &Config.loraTypes[loraIndex];
    LoRa_Utils::setup();
    Utils::i2cScannerForPeripherals();
    
    // 仅在非NRF52840平台上执行WX_Utils相关代码
    #ifndef PLATFORM_NRF52840
    WX_Utils::setup();
    
    WiFi.mode(WIFI_OFF);
    logger.log(LOG_LEVEL_DEBUG, "Main", "WiFi controller stopped");
    #endif

    // 仅在非NRF52840平台上执行蓝牙相关代码
    #ifndef PLATFORM_NRF52840
    if (bluetoothActive) {
        if (Config.bluetooth.useBLE) {
            BLE_Utils::setup();
        } else {
            #ifdef HAS_BT_CLASSIC
                BLUETOOTH_Utils::setup();
            #endif
        }
    }
    #endif

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

    POWER_Utils::lowerCpuFrequency();
    logger.log(LOG_LEVEL_DEBUG, "Main", "Smart Beacon is: %s", Utils::getSmartBeaconState());
    logger.log(LOG_LEVEL_INFO, "Main", "Setup Done!");
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
    
    SMARTBEACON_Utils::checkSettings(currentBeacon->smartBeaconSetting);
    SMARTBEACON_Utils::checkState();
    
    BATTERY_Utils::monitor();
    Utils::checkDisplayEcoMode();

    #ifdef BUTTON_PIN
        BUTTON_Utils::loop();
    #endif
    KEYBOARD_Utils::read();
    #ifdef HAS_JOYSTICK
        JOYSTICK_Utils::loop();
    #endif
    #ifdef HAS_TOUCHSCREEN
        TOUCH_Utils::loop();
    #endif

    ReceivedLoRaPacket packet = LoRa_Utils::receivePacket();

    // 仅在非NRF52840平台上执行消息处理代码
    #ifndef PLATFORM_NRF52840
    MSG_Utils::checkReceivedMessage(packet);
    MSG_Utils::processOutputBuffer();
    MSG_Utils::clean15SegBuffer();
    
    // 仅在非NRF52840平台上执行蓝牙相关代码
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
    #endif
    
    Utils::checkFlashlight();
    
    // 仅在非NRF52840平台上执行STATION_Utils相关代码
    #ifndef PLATFORM_NRF52840
    STATION_Utils::checkListenedStationsByTimeAndDelete();
    #endif

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
            // 仅在非NRF52840平台上执行STATION_Utils相关代码
            #ifndef PLATFORM_NRF52840
            STATION_Utils::checkStandingUpdateTime();
            #endif
        }
        SMARTBEACON_Utils::checkFixedBeaconTime();
        // 仅在非NRF52840平台上执行STATION_Utils相关代码
        #ifndef PLATFORM_NRF52840
        if (sendUpdate && gps_loc_update) STATION_Utils::sendBeacon();
        #endif
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