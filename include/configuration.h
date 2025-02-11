#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

#include <Arduino.h>
#include <vector>
#include <FS.h>

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

class WXSENSOR {
public:
    bool    active;
    float   temperatureCorrection;
    bool    sendTelemetry;
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
    WXSENSOR                wxsensor;
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

private:
    bool readFile();
};

#endif