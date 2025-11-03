#include <Arduino.h>
#include "configuration.h"

#ifdef PLATFORM_ESP32
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <FS.h>
#endif

Configuration::Configuration() {}

void Configuration::init() {
    // 设置默认值
    beacon.active = true;
    beacon.message = "LoRa APRS Tracker";
    beacon.sendTelemetry = true;
    beacon.positionSymbol = "/";
    beacon.positionSymbolTable = "/";
    beacon.comment = "";
    beacon.useMicE = false;
    beacon.micEDestination = "APZMDM";
    beacon.overrideDestination = false;
    beacon.destinationOverride = "";
    beacon.overridePath = false;
    beacon.pathOverride = "";
    
    display.brightness = 128;
    display.alwaysOn = true;
    display.useInverted = false;
    display.showAltitude = true;
    display.showSpeed = true;
    display.showTime = true;
    display.showSats = true;
    display.showBattery = true;
    display.showTemp = true;
    display.showRSSI = true;
    display.showLastHeard = true;
    
    battery.showPercentage = true;
    battery.useExternalADC = false;
    battery.useADC1 = true;
    battery.sensePin = 34;
    battery.adcMaxValue = 4095;
    battery.batteryMaxVoltage = 4.2f;
    battery.batteryMinVoltage = 3.3f;
    battery.useVoltageDivider = true;
    battery.voltageDividerR1 = 100000.0f;
    battery.voltageDividerR2 = 100000.0f;
    
    winlink.active = false;
    winlink.myCall = "";
    winlink.myPassword = "";
    winlink.destination = "";
    winlink.gateway = "";
    winlink.attachment = false;
    winlink.frequency = 0;
    winlink.bandwidth = 250;
    winlink.spreadFactor = 8;
    winlink.codingRate = 5;
    
    telemetry.active = true;
    telemetry.tempSensorType = "DHT22";
    telemetry.tempSensorPin = 4;
    telemetry.sendEveryNBeacons = 3;
    
    notification.active = true;
    notification.delay = 500;
    notification.message = "";
    notification.useBeep = false;
    notification.beepDuration = 50;
    notification.useVibrator = false;
    notification.vibratorPin = 2;
    notification.vibratorDuration = 100;
    
    bluetooth.active = false;
    bluetooth.deviceName = "LoRa APRS Tracker";
    bluetooth.useKISS = false;
    
    lora.useBand = "EU868";
    lora.frequency = 433.775f;
    lora.bandwidth = 125;
    lora.spreadFactor = 12;
    lora.codingRate = 5;
    lora.power = 20;
    lora.syncWord = 0x34;
    lora.preambleLength = 8;
    lora.txEnabled = true;
    
    ptt.active = false;
    ptt.io_pin = 27;
    ptt.preDelay = 100;
}

#ifdef PLATFORM_ESP32
bool Configuration::readFile() {
    // 实现文件读取逻辑
    String configFile = "/tracker_conf.json";
    Serial.println("Reading configuration from " + configFile);
    File file = SPIFFS.open(configFile, "r");
    if (!file) {
        Serial.println("Failed to open config file");
        return false;
    }
    
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
        Serial.println("Failed to parse config file");
        file.close();
        return false;
    }
    
    // 读取配置项
    if (doc.containsKey("beacon")) {
        JsonObject beaconObj = doc["beacon"];
        beacon.active = beaconObj["active"];
        beacon.message = beaconObj["message"].as<String>();
        beacon.sendTelemetry = beaconObj["sendTelemetry"];
        beacon.positionSymbol = beaconObj["positionSymbol"].as<String>();
        beacon.positionSymbolTable = beaconObj["positionSymbolTable"].as<String>();
        beacon.comment = beaconObj["comment"].as<String>();
        beacon.useMicE = beaconObj["useMicE"];
        beacon.micEDestination = beaconObj["micEDestination"].as<String>();
        beacon.overrideDestination = beaconObj["overrideDestination"];
        beacon.destinationOverride = beaconObj["destinationOverride"].as<String>();
        beacon.overridePath = beaconObj["overridePath"];
        beacon.pathOverride = beaconObj["pathOverride"].as<String>();
    }
    
    // 关闭文件
    file.close();
    
    return true;
}

void Configuration::writeFile() {
    String configFile = "/tracker_conf.json";
    Serial.println("Writing configuration to " + configFile);
    
    DynamicJsonDocument doc(4096);
    
    // 写入配置项
    JsonObject beaconObj = doc.createNestedObject("beacon");
    beaconObj["active"] = beacon.active;
    beaconObj["message"] = beacon.message;
    beaconObj["sendTelemetry"] = beacon.sendTelemetry;
    beaconObj["positionSymbol"] = beacon.positionSymbol;
    beaconObj["positionSymbolTable"] = beacon.positionSymbolTable;
    beaconObj["comment"] = beacon.comment;
    beaconObj["useMicE"] = beacon.useMicE;
    beaconObj["micEDestination"] = beacon.micEDestination;
    beaconObj["overrideDestination"] = beacon.overrideDestination;
    beaconObj["destinationOverride"] = beacon.destinationOverride;
    beaconObj["overridePath"] = beacon.overridePath;
    beaconObj["pathOverride"] = beacon.pathOverride;
    
    // 其他配置项...
    
    // 写入文件
    File file = SPIFFS.open(configFile, "w");
    if (!file) {
        Serial.println("Failed to open config file for writing");
        return;
    }
    
    serializeJson(doc, file);
    file.close();
}
#else
// NRF52840平台的简化实现
void Configuration::readFile(String configFile) {
    Serial.println("Configuration file not available on NRF52840 platform");
}

void Configuration::writeFile() {
    Serial.println("Configuration file not available on NRF52840 platform");
}
#endif

bool Configuration::validateConfigFile(const String& currentBeaconCallsign) {
    // 验证配置文件
    // 假设我们使用currentBeaconCallsign作为Mic-E验证的输入
    return validateCallsign() && validateMicE(currentBeaconCallsign);
}

bool Configuration::validateCallsign() {
    // 验证呼号格式
    // 这里简化实现
    return true;
}

bool Configuration::validateMicE(const String& currentBeaconMicE) {
    // 验证Mic-E格式
    // 这里简化实现
    return true;
}

// 全局配置实例
Configuration Config;