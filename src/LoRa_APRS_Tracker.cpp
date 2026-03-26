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

#ifdef UNIT_TEST
#include "mock_esp_log.h"
#else
#include <esp_log.h>
#endif
static const char *TAG = "Main";

#include <BluetoothSerial.h>
#include <APRSPacketLib.h>
#include <esp_task_wdt.h>
#include <NMEAGPS.h>
#include <Arduino.h>
#include <WiFi.h>
#include "smartbeacon_utils.h"
#include "bluetooth_utils.h"
#include "keyboard_utils.h"
#include "joystick_utils.h"
#include "sd_logger.h"
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
String      versionNumber           = "2.4.1";
Configuration                       Config;
HardwareSerial                      gpsSerial(1);
NMEAGPS                             nmeaGPS;
gps_fix                             gpsFix;
#ifdef HAS_BT_CLASSIC
    BluetoothSerial                 SerialBT;
#endif

uint8_t     myBeaconsIndex          = 0;
int         myBeaconsSize           = Config.beacons.size();
Beacon      *currentBeacon          = &Config.beacons[myBeaconsIndex];

// Expose variables to UIMapManager namespace
#ifdef USE_LVGL_UI
namespace UIMapManager {
    Configuration& Config = ::Config;
    uint8_t& myBeaconsIndex = ::myBeaconsIndex;
}
#endif

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

//#define DEBUG

extern bool gpsIsActive;

void setup() {
    Serial.begin(115200);

    // Boost CPU to 240MHz for fast init
    setCpuFrequencyMhz(240);
    ESP_LOGI(TAG, "CPU frequency: %d MHz", getCpuFrequencyMhz());

    // Turn off backlight immediately to avoid garbage display during init
    #if defined(USE_LVGL_UI) && defined(BOARD_BL_PIN)
        pinMode(BOARD_BL_PIN, OUTPUT);
        digitalWrite(BOARD_BL_PIN, LOW);
    #endif

    POWER_Utils::setup();
    #ifndef USE_LVGL_UI
        displaySetup();  // Skip for LVGL - it does its own TFT init
    #endif
    POWER_Utils::externalPinSetup();

    STATION_Utils::loadIndex(0);    // callsign Index
    STATION_Utils::loadIndex(1);    // lora freq settins Index
    STATION_Utils::nearStationInit();
    STATION_Utils::mapStationsInit();
    #ifdef USE_LVGL_UI
        LVGL_UI::showSplashScreen(loraIndex, versionDate.c_str());
        LVGL_UI::showInitScreen();
    #else
        startupScreen(loraIndex, versionDate);
    #endif

    // Storage + Config first: SPIFFS must be ready before WiFi/BLE read Config
    #ifdef USE_LVGL_UI
        LVGL_UI::updateInitStatus("Storage...");
    #endif
    STORAGE_Utils::setup();        // Formats SPIFFS on first boot
    Config.init();                 // Now SPIFFS is ready, load or create config
    STORAGE_Utils::loadStats();

    // WiFi/BLE: no auto-start at boot — manual activation only via Settings
    // (first boot web-conf handled after LVGL init via needsWebConfig())
    WIFI_Utils::setup();
    MSG_Utils::loadNumMessages();

    // Initialize SD logger for debugging reboots
    SD_Logger::init();
    SD_Logger::logBootInfo();

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

    ESP_LOGD(TAG, "Smart Beacon is: %s", Utils::getSmartBeaconState());

    // Memory stats
    #ifdef BOARD_HAS_PSRAM
        ESP_LOGI(TAG, "PSRAM: %u KB total, %u KB free", ESP.getPsramSize()/1024, ESP.getFreePsram()/1024);
    #endif
    ESP_LOGI(TAG, "Heap: %u KB total, %u KB free", ESP.getHeapSize()/1024, ESP.getFreeHeap()/1024);

    // Initialize watchdog timer (30 seconds timeout)
    esp_task_wdt_init(30, true);  // 30 seconds, panic on timeout
    esp_task_wdt_add(NULL);       // Add current task to watchdog
    ESP_LOGI(TAG, "Watchdog initialized (30s timeout)");

    ESP_LOGI(TAG, "Setup Done!");

    // Lower CPU frequency for power saving in normal operation
    // Map screen will boost back to 240MHz when needed
    POWER_Utils::lowerCpuFrequency();

    menuDisplay = 0;
}

void loop() {
    currentBeacon = &Config.beacons[myBeaconsIndex];
    if (statusUpdate) {
        if (APRSPacketLib::checkNocall(currentBeacon->callsign)) {
            ESP_LOGE(TAG, "Change your callsigns in WebConfig");
            displayShow("ERROR", "Callsigns = NOCALL!", "---> change it !!!", 2000);
            KEYBOARD_Utils::rightArrow();
            currentBeacon = &Config.beacons[myBeaconsIndex];
        }
        miceActive = APRSPacketLib::validateMicE(currentBeacon->micE);
    }

    SMARTBEACON_Utils::checkSettings(currentBeacon->smartBeaconSetting);
    SMARTBEACON_Utils::checkState();

    BATTERY_Utils::monitor();
    #ifndef USE_LVGL_UI
        Utils::checkDisplayEcoMode();
    #endif
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

    // Process pending LoRa configuration changes (from ISR)
    LoRa_Utils::processPendingChanges();

    ReceivedLoRaPacket packet = LoRa_Utils::receivePacket();

    // Log raw frame and update stats (if packet received)
    if (!packet.text.isEmpty()) {
        String rawFrame = packet.text.substring(3);
        
        // --- 1. Extract path and detect Direct/Digi ---
        int pathStart = rawFrame.indexOf('>');
        int pathEnd = rawFrame.indexOf(':');
        bool isDirect = true; // Direct by default

        if (pathStart != -1 && pathEnd != -1) {
            String path = rawFrame.substring(pathStart + 1, pathEnd);
            if (path.indexOf('*') != -1) {
                isDirect = false; // Asterisk found = Relayed
            }
            // Update digi stats (original code)
            STORAGE_Utils::updateDigiStats(path);
        }

        // --- 2. Frame recording ---
        STORAGE_Utils::logRawFrame(rawFrame, packet.rssi, packet.snr, isDirect);
        STORAGE_Utils::updateRxStats(packet.rssi, packet.snr);

        // --- 3. Per-station stats (Sender) ---
        if (pathStart > 0) {
            String sender = rawFrame.substring(0, pathStart);
            STORAGE_Utils::updateStationStats(sender, packet.rssi, packet.snr, isDirect);
        }

        #ifdef USE_LVGL_UI
            LVGL_UI::refreshFramesList();
        #endif

        // Repeater mode: retransmit received packet with proper APRS digipeating
        if (Config.lora.repeaterMode) {
            String digipeatedPacket = APRSPacketLib::generateDigipeatedPacket(packet.text, currentBeacon->callsign, Config.path);
            if (digipeatedPacket != "X") {
                ESP_LOGI(TAG, "Digipeating: %s", digipeatedPacket.c_str());
                delay(random(100, 500)); // Random delay to avoid collisions
                LoRa_Utils::sendNewPacket(digipeatedPacket);
            } else {
                ESP_LOGW(TAG, "Packet won't be repeated (Missing WIDEn-N)");
            }
        }
    }

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

    // Check BLE eco mode timeout (sleep BLE after inactivity)
    if (bluetoothActive && Config.bluetooth.useBLE) {
        BLE_Utils::checkEcoMode();
    }

    MSG_Utils::ledNotification();
    Utils::checkFlashlight();
    STATION_Utils::checkListenedStationsByTimeAndDelete();
    STATION_Utils::cleanOldMapStations();

    lastTx = millis() - lastTxTime;
    if (gpsIsActive) {
        GPS_Utils::getData();
        bool gps_time_update = GPS_Utils::hasNewFix() && gpsFix.valid.time;
        bool gps_loc_update  = GPS_Utils::hasNewFix() && gpsFix.valid.location;
        GPS_Utils::setDateFromData();

        // Keep SD Logger timestamps accurate with GPS wall-clock time
        if (gps_time_update && gpsFix.valid.date) {
            SD_Logger::setGpsTime(
                gpsFix.dateTime.hours, gpsFix.dateTime.minutes, gpsFix.dateTime.seconds,
                gpsFix.dateTime.date,  gpsFix.dateTime.month,   2000 + gpsFix.dateTime.year
            );
        }

        int currentSpeed = gpsFix.valid.speed ? (int)gpsFix.speed_kph() : 0;

        if (gps_loc_update) Utils::checkStatus();

        if (!sendUpdate && gps_loc_update && smartBeaconActive) {
            GPS_Utils::calculateDistanceTraveled();
            if (!sendUpdate) GPS_Utils::calculateHeadingDelta(currentSpeed);
            STATION_Utils::checkStandingUpdateTime();
        }
        SMARTBEACON_Utils::checkFixedBeaconTime();

        // Check if GPS quality meets criteria for sending a beacon
        bool gpsQualityOk = false;
        if (Config.gpsConfig.strict3DFix) {
            // Strict Mountain Mode: Requires a very accurate 3D position to avoid sending wrong altitudes
            // PDOP <= 5.0 ensures that both HDOP and VDOP are geometrically solid
            gpsQualityOk = (gpsFix.satellites >= 6) && (gpsPdop() <= 5.0f);
        } else {
            // Normal Mode: Only cares about 2D horizontal accuracy (HDOP)
            gpsQualityOk = (gpsFix.satellites >= 6) && (gpsHdop() <= 5.0f);
        }

        if (sendUpdate && gps_loc_update && gpsQualityOk) {
            STATION_Utils::sendBeacon();
        } else if (sendUpdate && gps_loc_update && !gpsQualityOk) {
            // Rate-limited log: at most once every 30 seconds to avoid serial flood
            static uint32_t lastGpsQualityLogMs = 0;
            static uint32_t gpsQualitySkipCount = 0;
            gpsQualitySkipCount++;
            uint32_t now = millis();
            if (now - lastGpsQualityLogMs >= 30000) {
                if (Config.gpsConfig.strict3DFix) {
                    ESP_LOGD(TAG, "GPS strict 3D quality too low (sats=%d, PDOP=%.1f), skipping beacon (x%u)",
                             gpsFix.satellites, gpsPdop(), gpsQualitySkipCount);
                } else {
                    ESP_LOGD(TAG, "GPS quality too low (sats=%d, HDOP=%.1f), skipping beacon (x%u)",
                             gpsFix.satellites, gpsHdop(), gpsQualitySkipCount);
                }
                lastGpsQualityLogMs = now;
                gpsQualitySkipCount = 0;
            }
        }

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

    STORAGE_Utils::checkStatsSave();

    #ifdef USE_LVGL_UI
        LVGL_UI::loop();
    #endif

    // Reset watchdog timer
    esp_task_wdt_reset();

    // Periodic memory monitoring (every 60s serial, every 5 min SD log)
    static uint32_t lastMemLog = 0;
    static uint32_t lastHeartbeat = 0;
    if (millis() - lastMemLog >= 10000) {  // 10 seconds
        lastMemLog = millis();
        ESP_LOGI(TAG, "DRAM: %u  PSRAM: %u  Largest DRAM: %u",
                      heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                      heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    }
    if (millis() - lastHeartbeat >= 300000) {  // 5 minutes
        lastHeartbeat = millis();
        SD_Logger::logf(SD_Logger::INFO, "LOOP", "Heartbeat - Free heap: %u KB", ESP.getFreeHeap() / 1024);
    }

    // Update RTC crash context every 5s — readable at next boot after PANIC/WDT
    static uint32_t lastCrashCtxUpdate = 0;
    if (millis() - lastCrashCtxUpdate >= 5000) {
        lastCrashCtxUpdate = millis();
        float lat = gpsFix.valid.location ? (float)gpsFix.latitude() : 0.0f;
        float lon = gpsFix.valid.location ? (float)gpsFix.longitude() : 0.0f;
        SD_Logger::updateCrashContext("LOOP", lat, lon);
    }

    yield();
}
