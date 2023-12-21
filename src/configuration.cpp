#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "configuration.h"
#include "display.h"
#include "logger.h"

extern logging::Logger logger;

extern bool             displayState;
extern uint32_t         displayTime;

Configuration::Configuration() {
    _filePath = "/tracker_config.json";
    if (!SPIFFS.begin(false)) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "Config", "SPIFFS mount failed");
      Serial.println("SPIFFS Mount Failed"); // why Serial instead of logger ?
      return;
    }
    readFile(SPIFFS, _filePath.c_str());
}

void Configuration::readFile(FS &fs, const char *fileName) {
    StaticJsonDocument<2560> data;
    File configFile = fs.open(fileName, "r");
    DeserializationError error = deserializeJson(data, configFile);
    if (error) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "Config", "Failed to read file, using default configuration");
      Serial.println("Failed to read file, using default configuration"); // why Serial instead of logger ?
    }

    JsonArray BeaconsArray = data["beacons"];
    for (auto && i : BeaconsArray) {
        Beacon bcn;

        bcn.callsign          = i["callsign"].as<String>();
        bcn.symbol            = i["symbol"].as<String>();
        bcn.overlay           = i["overlay"].as<String>();
        bcn.micE              = i["micE"].as<String>();
        bcn.comment           = i["comment"].as<String>();

        bcn.smartBeaconState  = i["smart_beacon"]["active"].as<bool>();
        bcn.slowRate          = i["smart_beacon"]["slowRate"].as<int>();
        bcn.slowSpeed         = i["smart_beacon"]["slowSpeed"].as<int>();
        bcn.fastRate          = i["smart_beacon"]["fastRate"].as<int>();
        bcn.fastSpeed         = i["smart_beacon"]["fastSpeed"].as<int>();
        bcn.minTxDist         = i["smart_beacon"]["minTxDist"].as<int>();
        bcn.minDeltaBeacon    = i["smart_beacon"]["minDeltaBeacon"].as<int>();
        bcn.turnMinDeg        = i["smart_beacon"]["turnMinDeg"].as<int>();
        bcn.turnSlope         = i["smart_beacon"]["turnSlope"].as<int>();

        beacons.push_back(bcn);
    }

    loramodule.frequency          = data["lora"]["frequency"].as<long>();
    loramodule.spreadingFactor    = data["lora"]["spreadingFactor"].as<int>();
    loramodule.signalBandwidth    = data["lora"]["signalBandwidth"].as<long>();
    loramodule.codingRate4        = data["lora"]["codingRate4"].as<int>();
    loramodule.power              = data["lora"]["power"].as<int>();

    ptt.active                    = data["pttTrigger"]["active"].as<bool>();
    ptt.io_pin                    = data["pttTrigger"]["io_pin"].as<int>();
    ptt.preDelay                  = data["pttTrigger"]["preDelay"].as<int>();
    ptt.postDelay                 = data["pttTrigger"]["postDelay"].as<int>();
    ptt.reverse                   = data["pttTrigger"]["reverse"].as<bool>();

    bme.active                    = data["bme"]["active"].as<bool>();
    bme.sendTelemetry             = data["bme"]["sendTelemetry"].as<bool>();
    bme.heightCorrection          = data["bme"]["heightCorrection"].as<int>();

    notification.ledTx            = data["notification"]["ledTx"].as<bool>();
    notification.ledTxPin         = data["notification"]["ledTxPin"].as<int>();
    notification.ledMessage       = data["notification"]["ledMessage"].as<bool>();
		notification.ledMessagePin    = data["notification"]["ledMessagePin"].as<int>();
		notification.buzzerActive     = data["notification"]["buzzerActive"].as<bool>();
		notification.buzzerPinTone    = data["notification"]["buzzerPinTone"].as<int>();
    notification.buzzerPinVcc     = data["notification"]["buzzerPinVcc"].as<int>();
		notification.bootUpBeep       = data["notification"]["bootUpBeep"].as<bool>();
		notification.txBeep           = data["notification"]["txBeep"].as<bool>();
		notification.messageRxBeep    = data["notification"]["messageRxBeep"].as<bool>();
		notification.stationBeep      = data["notification"]["stationBeep"].as<bool>();
    notification.lowBatteryBeep   = data["notification"]["lowBatteryBeep"].as<bool>();

    simplifiedTrackerMode         = data["other"]["simplifiedTrackerMode"].as<bool>();
    showSymbolOnScreen            = data["other"]["showSymbolOnScreen"].as<bool>();
    sendCommentAfterXBeacons      = data["other"]["sendCommentAfterXBeacons"].as<int>();
    displayEcoMode                = data["other"]["displayEcoMode"].as<bool>();
    displayTimeout                = data["other"]["displayTimeout"].as<int>();
    path                          = data["other"]["path"].as<String>();
    nonSmartBeaconRate            = data["other"]["nonSmartBeaconRate"].as<int>();
    rememberStationTime           = data["other"]["rememberStationTime"].as<int>();
    maxDistanceToTracker          = data["other"]["maxDistanceToTracker"].as<int>();
    standingUpdateTime            = data["other"]["standingUpdateTime"].as<int>();
    sendAltitude                  = data["other"]["sendAltitude"].as<bool>();
    sendBatteryInfo               = data["other"]["sendBatteryInfo"].as<bool>();
    bluetoothType                 = data["other"]["bluetoothType"].as<int>();
    bluetoothActive               = data["other"]["bluetoothActive"].as<bool>();
    disableGPS                    = data["other"]["disableGPS"].as<bool>();

    configFile.close();
}

void Configuration::validateConfigFile(const String& currentBeaconCallsign) {
  if (currentBeaconCallsign.indexOf("NOCALL") != -1) {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "Config", "Change all your callsigns in 'data/tracker_config.json' and upload it via 'Upload File System image'");
    show_display("ERROR", "Change all callsigns!", "'tracker_config.json'", "upload it via --> ", "'Upload File System image'");
    while (true) {
        delay(1000);
    }
  }
}

bool Configuration::writeConfigFile(const String& json) {
  if (json[0] != '{' || json[json.length() - 1] != '}') {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "Config", "Malformed JSON received");
    return false;
  }

  File configFile = SPIFFS.open(_filePath.c_str(), "w");
  if (!configFile) {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "Config", "Failed to open file");
    return false;
  }

  if (configFile.print(json) != json.length()) {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "Config", "Failed to write all json in file");
    configFile.close();
    return false;
  }

  configFile.close();

  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Config", "New config written, reload it");
  display_toggle(true);
  displayTime = millis();
  displayState = true;
  show_display("Config", "New config saved", "Will reload...", 2500);

  readFile(SPIFFS, _filePath.c_str());

  return true;
}
String Configuration::readRawConfigFile() const {
  File configFile = SPIFFS.open(_filePath.c_str(), "r");
  if (!configFile) {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "Config", "Failed to open file");
    return F("{}");
  }

  String json = configFile.readString();
  configFile.close();
  json.replace('\n', ' ');
  json.replace('\r', ' ');
  json.replace('\t', ' ');
  return json;
}

bool Configuration::validateMicE(const String& currentBeaconMicE) {
  String miceMessageTypes[] = {"111", "110", "101", "100", "011", "010", "001" , "000"};
  int arraySize = sizeof(miceMessageTypes) / sizeof(miceMessageTypes[0]);
  bool validType = false;
  for (int i=0; i<arraySize;i++) {
    if (currentBeaconMicE == miceMessageTypes[i]) {
      validType = true;
    }
  }
  return validType;
}