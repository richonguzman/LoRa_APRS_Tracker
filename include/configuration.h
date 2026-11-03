/* Copyright (C) 2026 Ricardo Guzman - CA2RXU
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

#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

#include <Arduino.h>
#include <vector>
#ifndef ARDUINO_ARCH_NRF52
#include <FS.h>          // header is unused in this declaration; kept ESP32-side for upstream-merge friendliness
#endif
#include "smartbeacon_utils.h"

enum DeviceRole {
    ROLE_TRACKER    = 0,
    ROLE_IGATE      = 1,
    ROLE_DIGIPEATER = 2
};

// Digipeat mode — independent of DeviceRole.
// Any role (Tracker, iGate, Digipeater) can have digipeating enabled.
enum DigiMode {
    DIGI_OFF         = 0,  // No digipeating
    DIGI_WIDE1       = 1,  // Fill-in: respond to WIDE1-1 only
    DIGI_WIDE1_WIDE2 = 2   // Infrastructure: respond to WIDE1-1 and WIDE2-n
};

enum GPSSource {
    GPS_INTERNAL = 0,  // hardware UART GPS; SmartBeacon
    GPS_FIXED    = 1,  // user-configured lat/lon/elev; fixed interval beaconing
    GPS_NONE     = 2   // no position source; no beacons (pure TNC/relay)
};

static constexpr uint8_t MAX_WIFI_NETWORKS = 5;

class WiFiNetwork {
public:
    String  ssid;
    String  password;
};

class WiFiSTA {
public:
    bool                     enabled;
    std::vector<WiFiNetwork> networks;   // 0..MAX_WIFI_NETWORKS entries, priority = list order
};

class WiFiAP {
public:
    String  password;
};

class Beacon {
public:
    String  callsign;
    String  symbol;
    String  overlay;
    String  micE;
    String  comment;
    bool    smartBeaconActive;
    byte    smartBeaconSetting;
    String  profileLabel;
    String  status;
    String  tacticalCallsign;
};

class Display {
public:
    bool    ecoMode;
    int     timeout;
    bool    turn180;
    bool    invertDisplay;   // true = black text on white (OLED invert mode)
    bool    ledEnabled;      // false = LED always off
};

class Battery {
public:
    bool    sendVoltage;        // append Bat=X.XXV to beacon comment
    bool    sendVoltageAlways;  // on every beacon; otherwise every sendCommentAfterXBeacons
    float   sleepVoltage;       // shutdown threshold (V); only active on ADC_CTRL boards
};

class LoraType {
public:
    long    frequency;
    int     spreadingFactor;
    long    signalBandwidth;
    int     codingRate4;
    int     power;
};

class PTT {
public:
    bool    active;
    int     io_pin;
    int     preDelay;
    int     postDelay;
    bool    reverse;
};

class BLUETOOTH {
public:
    bool    active;
    String  deviceName;
    // Stack and framing are board-determined: nRF52 uses Bluefruit BLE,
    // ESP32 uses NimBLE when available, otherwise BT Classic SPP.
    // All paths use KISS (AX.25) framing — no config needed.
};

class APRSISS {
public:
    String  server;
    uint16_t port;
    String  passcode;
    String  filter;
    // Enables the IS->RF downlink (listenAPRSIS() in aprs_is_utils.cpp).
    // Independent of passcode validity: an unverified passcode already blocks
    // downlink (and degrades RF->IS uploads to qAO), so this is a separate,
    // explicit on/off for the downlink direction rather than overloading the
    // passcode field. Downlink is always restricted to messages addressed to
    // a station heard directly on RF recently, regardless of this setting.
    bool    downlinkEnabled;
};

class TCPKISS {
public:
    uint16_t port;           // TCP port (server runs automatically when WiFi STA is connected)
    // USB serial is always KISS TNC mode by default.
    // Type 'setup' or 'log' over serial to switch modes.
};

class FixedPosition {
public:
    float   latitude;
    float   longitude;
    float   elevation;
};

class PHGConfig {
public:
    bool    enabled;
    uint8_t power;       // APRS PHG power code 0-9 (0=0W,1=1W,2=4W,...,9=81W)
    uint8_t height;      // APRS PHG HAAT code 0-9 (0=10ft,1=20ft,2=40ft,...,9=5120ft)
    uint8_t gain;        // Antenna gain in dBd 0-9
    uint8_t directivity; // 0=Omni,1=NE,2=E,3=SE,4=S,5=SW,6=W,7=NW,8=N,9=Hi-Gain Omni
    uint8_t beaconRate;  // Minutes between PHG beacons (independent of normal beacon rate)
};

class RemoteCfg {
public:
    bool    enabled;          // master switch; false disables all CS?/CSR/CSU/CSW handling
    String  token;             // shared secret for CSU unlock; empty = write stage disabled
    int     unlockWindowSec;   // write-window duration after a successful CSU, in seconds
};


class Configuration {
public:

    WiFiAP                  wifiAP;
    WiFiSTA                 wifiSTA;
    std::vector<Beacon>     beacons;
    Display                 display;
    Battery                 battery;
    std::vector<LoraType>   loraTypes;
    PTT                     ptt;
    BLUETOOTH               bluetooth;
    SmartBeaconValues       customSmartBeacon;
    APRSISS                 aprsIS;
    TCPKISS                 tcpKISS;
    FixedPosition           fixedPosition;
    PHGConfig               phg;
    RemoteCfg               remoteCfg;

    DeviceRole              deviceRole;
    GPSSource               gpsSource;
    DigiMode                digiMode;       // replaces bool digipeating

    int     sendCommentAfterXBeacons;
    String  beaconPath;     // APRS path for OWN TX (e.g. WIDE1-1). Not used by the digi relay.
    int     nonSmartBeaconRate;    // seconds (JSON key "nonSmartBeaconRateSec"; legacy "nonSmartBeaconRate" was minutes)
    bool    sendAltitude;
    bool    sendSpeedCourse;
    bool    cadActive;

    void setDefaultValues();
    bool writeFile();
    Configuration();
    // nRF52-only: filesystem-backed setup deferred from the constructor because
    // static init runs before FreeRTOS / InternalFS is up. ESP32 builds run the
    // same logic from the constructor as before. Call once early in setup().
    #ifdef ARDUINO_ARCH_NRF52
    void init();
    #endif

private:
    bool readFile();
};

#endif