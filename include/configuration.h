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

#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

// 首先包含平台兼容性头文件，确保所有宏冲突都得到解决
#include "platform_compat.h"

#include <Arduino.h>
#include <vector>

// 根据平台条件包含文件系统库
#ifdef PLATFORM_ESP32
#include <FS.h>
#else
// NRF52840平台不需要文件系统库
#endif

class WiFiAP {
public:
    bool    active;
    String  password;
};

class Beacon {
public:
    String  callsign;
    String  symbol;
    String  overlay;
    String  comment;
    bool    smartBeaconActive;
    byte    smartBeaconSetting;
    String  micE;
    bool    gpsEcoMode;
    String  profileLabel;
};

class Display {
public:
    bool    showSymbol;
    bool    ecoMode;
    int     timeout;
    bool    turn180;
};

class Battery {
public:
    bool    sendVoltage;
    bool    voltageAsTelemetry;
    bool    sendVoltageAlways;
    bool    monitorVoltage;
    float   sleepVoltage;
};

class Winlink {
public:
    String  password;
};

class Telemetry {
public:
    bool    active;
    bool    sendTelemetry;
    float   temperatureCorrection;
};

class Notification {
public:
    bool    ledTx;
    int     ledTxPin;
    bool    ledMessage;
    int     ledMessagePin;
    bool    ledFlashlight;
    int     ledFlashlightPin;
    bool    buzzerActive;
    int     buzzerPinTone;
    int     buzzerPinVcc;
    bool    bootUpBeep;
    bool    txBeep;
    bool    messageRxBeep;
    bool    stationBeep;
    bool    lowBatteryBeep;
    bool    shutDownBeep;
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
    bool    useBLE;
    bool    useKISS;
};


class Configuration {
public:

    WiFiAP                  wifiAP;
    std::vector<Beacon>     beacons;  
    Display                 display;
    Battery                 battery;
    Winlink                 winlink;
    Telemetry               telemetry;
    Notification            notification;
    std::vector<LoraType>   loraTypes;
    PTT                     ptt;
    BLUETOOTH               bluetooth;
    
    bool    simplifiedTrackerMode;
    int     sendCommentAfterXBeacons;
    String  path;
    String  email;
    int     nonSmartBeaconRate;
    int     rememberStationTime;
    int     standingUpdateTime;
    bool    sendAltitude;
    bool    disableGPS;
    bool    acceptOwnFrameFromTNC;

    void init();
    void writeFile();
    Configuration();
    bool validateConfigFile(const String& currentBeaconCallsign);
    bool validateMicE(const String& currentBeaconMicE);
    bool validateCallsign();

private:
    bool readFile();
};

#endif