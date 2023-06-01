#ifdef ESP32
#include <esp_bt.h>
#endif
#include <APRS-Decoder.h>
#include <Arduino.h>
#include <LoRa.h>
#include <OneButton.h>
#include <TimeLib.h>
#include <TinyGPS++.h>
#include <WiFi.h>
#include <logger.h>
#include "SPIFFS.h"
#include <vector>
#include "configuration.h"
#include "display.h"
#include "pins.h"
#include "power_management.h"

#define VERSION "2023.06.01"

logging::Logger logger;

String configurationFilePath = "/tracker_config.json";
Configuration   Config(configurationFilePath);
static int      myBeaconsIndex = 0;
int             myBeaconsSize  = Config.beacons.size();
Beacon          *currentBeacon = &Config.beacons[myBeaconsIndex];
PowerManagement powerManagement;
OneButton       userButton = OneButton(BUTTON_PIN, true, true);
HardwareSerial  neo6m_gps(1);
TinyGPSPlus     gps;

void validateConfigFile();
void setup_lora();
void setup_gps();

char *ax25_base91enc(char *s, uint8_t n, uint32_t v);
String createDateString(time_t t);
String createTimeString(time_t t);
String getSmartBeaconState();
String padding(unsigned int number, unsigned int width);

static int      menuDisplay           = 0;
static bool     displayEcoMode        = Config.displayEcoMode;
static bool     displayState          = true;
static uint32_t displayTime           = millis();
static bool     send_update           = true;
static String   lastHeardTracker      = "NONE";
static int      numAPRSMessages       = 0;
static int      messagesIterator      = 0;
static String   lastMessageAPRS       = "";
static bool     noMessageWarning      = false;
static bool     statusAfterBootState  = true;

String          firstNearTracker      = "";
String          secondNearTracker     = "";
String          thirdNearTracker      = "";
String          fourthNearTracker     = "";

std::vector<String> loadedAPRSMessages;

static uint32_t lastDeleteListenedTracker = millis();

void loadNumMessages() {
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  File fileToReadAPRS = SPIFFS.open("/aprsMessages.txt");
  if(!fileToReadAPRS){
    Serial.println("Failed to open APRS_Msg for reading");
    return;
  }

  std::vector<String> v1;
  while (fileToReadAPRS.available()) {
    v1.push_back(fileToReadAPRS.readStringUntil('\n'));
  }
  fileToReadAPRS.close();

  numAPRSMessages = 0;
  for (String s1 : v1) {
    numAPRSMessages++;
  }
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Number of APRS Messages : %s", String(numAPRSMessages));  
}

void setup_gps() {
  neo6m_gps.begin(9600, SERIAL_8N1, GPS_TX, GPS_RX);
}

void setup_lora() {
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "Set SPI pins!");
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "Set LoRa pins!");
  LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);

  long freq = Config.loramodule.frequency;
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "frequency: %d", freq);
  if (!LoRa.begin(freq)) {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "LoRa", "Starting LoRa failed!");
    show_display("ERROR", "Starting LoRa failed!");
    while (true) {
    }
  }
  LoRa.setSpreadingFactor(Config.loramodule.spreadingFactor);
  LoRa.setSignalBandwidth(Config.loramodule.signalBandwidth);
  LoRa.setCodingRate4(Config.loramodule.codingRate4);
  LoRa.enableCrc();

  LoRa.setTxPower(Config.loramodule.power);
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "LoRa init done!");
}

void loadMessagesFromMemory() { 
  File fileToRead;
  noMessageWarning = false;
  if (numAPRSMessages == 0) {
    noMessageWarning = true;
  } else {
    loadedAPRSMessages.clear();
    fileToRead = SPIFFS.open("/aprsMessages.txt");
  }
  if (noMessageWarning) {
    show_display("__INFO____", "", "NO MESSAGES IN MEMORY", 1500);
  } else {
    if(!fileToRead){
      Serial.println("Failed to open file for reading");
      return;
    }
    while (fileToRead.available()) {
      loadedAPRSMessages.push_back(fileToRead.readStringUntil('\n'));
    }
    fileToRead.close();
  }  
}

void deleteFile() {
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  } 
  SPIFFS.remove("/aprsMessages.txt");
}

void sendNewLoraPacket(String newPacket) {
  LoRa.beginPacket();
  LoRa.write('<');
  LoRa.write(0xFF);
  LoRa.write(0x01);
  LoRa.write((const uint8_t *)newPacket.c_str(), newPacket.length());
  LoRa.endPacket();
}

void sendMessage(String station, String textMessage) {
  String messageToSend;
  for(int i = station.length(); i < 9; i++) {
    station += ' ';
  }
  messageToSend = currentBeacon->callsign + ">APLR01,WIDE1-1::" + station + ":" + textMessage;
  
  if (textMessage.indexOf("ack")== 0) {
    show_display("<<ACK Tx>>", 500);
  } else if (station.indexOf("CD2RXU-15") == 0 && textMessage.indexOf("wrl")==0) {
    show_display("<WEATHER>","", "--- Sending Query ---",  1000);
  } else {
    show_display("MSG Tx >>", "", messageToSend, 1000);
  }
  sendNewLoraPacket(messageToSend);
}

static void ButtonSinglePress() {
  if (menuDisplay == 0) {
    if (!displayState) {
      display_toggle(true);
      displayTime = millis();
    } else {
      send_update = true;
    }
  } else if (menuDisplay == 1) {
    loadMessagesFromMemory();
    if (noMessageWarning) {
      menuDisplay = 1;
    } else {
      menuDisplay = 10;
    }
  } else if (menuDisplay == 2) {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Loop", "%s", "wrl");
    sendMessage("CD2RXU-15","wrl");
  } else if (menuDisplay == 10) {
    messagesIterator++;
    if (messagesIterator == numAPRSMessages) {
      menuDisplay = 1;
      messagesIterator = 0;
    } else {
      menuDisplay = 10;
    }
  } else if (menuDisplay == 20) {
    menuDisplay = 2;
  } else if (menuDisplay == 3) {
    show_display("__INFO____", "", "NOTHING YET ...", 1000);
  }
}

static void ButtonLongPress() {
  if (menuDisplay == 0) {
    if(myBeaconsIndex >= (myBeaconsSize-1)) {
      myBeaconsIndex = 0;
    } else {
      myBeaconsIndex++;
    }
    statusAfterBootState  = true;
    display_toggle(true);
    displayTime = millis();
    show_display("__INFO____", "", "CHANGING CALLSIGN ...", 1000);
  } else if (menuDisplay == 1) {
    deleteFile();
    show_display("__INFO____", "", "ALL MESSAGES DELETED!", 2000);
    loadNumMessages();
  } else if (menuDisplay == 2) {
    menuDisplay = 20;
  } else if (menuDisplay == 3) {
    if (!displayEcoMode) {
      displayEcoMode = true;
      show_display("__DISPLAY_", "", "   ECO MODE -> ON", 1000);
    } else {
      displayEcoMode = false;
      show_display("__DISPLAY_", "", "   ECO MODE -> OFF", 1000);
    }
  }
}

static void ButtonDoublePress() {
  display_toggle(true);
  if (menuDisplay == 0) {
    menuDisplay = 1;
  } else if (menuDisplay == 1) {
    menuDisplay = 2;
    messagesIterator = 0;
  } else if (menuDisplay == 2) {
    menuDisplay = 3;
  } else if (menuDisplay == 3 || menuDisplay == 20) {
    menuDisplay = 0;
    displayTime = millis();
  } 
}

void startingStatus() {
  delay(2000);
  sendNewLoraPacket(currentBeacon->callsign + ">" + Config.destination + "," + Config.path + ":>" + Config.defaultStatus);
  statusAfterBootState = false;
}

void saveNewMessage(String typeMessage, String station, String gate, String newMessage) {
  if (typeMessage == "APRS" && lastMessageAPRS != newMessage) {
    File fileToAppendAPRS = SPIFFS.open("/aprsMessages.txt", FILE_APPEND);
    if(!fileToAppendAPRS){
      Serial.println("There was an error opening the file for appending");
      return;
    }
    newMessage.trim();
    if(!fileToAppendAPRS.println(station + "," + gate + "," + newMessage)){
      Serial.println("File append failed");
    }
    lastMessageAPRS = newMessage;
    numAPRSMessages++;
    fileToAppendAPRS.close();
  }
}

void orderListenedTrackersByDistance(String callsign, float distance, float course) {
  String firstNearTrackerDistance, secondNearTrackerDistance, thirdNearTrackerDistance, fourthNearTrackerDistance, newTrackerInfo, firstNearTrackerCallsign, secondNearTrackerCallsign,thirdNearTrackerCallsign, fourthNearTrackerCallsign;
  newTrackerInfo = callsign + "> " + String(distance,2) + "km " + String(int(course)) + "," + String(millis());
  float firstDistance   = 0.0;
  float secondDistance  = 0.0;
  float thirdDistance   = 0.0;
  float fourthDistance  = 0.0;
  if (firstNearTracker != "") {
    firstNearTrackerCallsign = firstNearTracker.substring(0,firstNearTracker.indexOf(">"));
    firstNearTrackerDistance = firstNearTracker.substring(firstNearTracker.indexOf(">")+1,firstNearTracker.indexOf("km"));
    firstDistance = firstNearTrackerDistance.toFloat();
  }
  if (secondNearTracker != "") {
    secondNearTrackerCallsign = secondNearTracker.substring(0,secondNearTracker.indexOf(">"));
    secondNearTrackerDistance = secondNearTracker.substring(secondNearTracker.indexOf(">")+1,secondNearTracker.indexOf("km"));
    secondDistance = secondNearTrackerDistance.toFloat();
  }
  if (thirdNearTracker != "") {
    thirdNearTrackerCallsign = thirdNearTracker.substring(0,thirdNearTracker.indexOf(">"));
    thirdNearTrackerDistance = thirdNearTracker.substring(thirdNearTracker.indexOf(">")+1,thirdNearTracker.indexOf("km"));
    thirdDistance = thirdNearTrackerDistance.toFloat();
  }
  if (fourthNearTracker != "") {
    fourthNearTrackerCallsign = fourthNearTracker.substring(0,fourthNearTracker.indexOf(">"));
    fourthNearTrackerDistance = fourthNearTracker.substring(fourthNearTracker.indexOf(">")+1,fourthNearTracker.indexOf("km"));
    fourthDistance = fourthNearTrackerDistance.toFloat();
  } 

  if (firstNearTracker == "" && secondNearTracker == "" && thirdNearTracker == "" && fourthNearTracker == "") {
    firstNearTracker = newTrackerInfo;
  } else if (firstNearTracker != "" && secondNearTracker == "" && thirdNearTracker == "" && fourthNearTracker == "") {
    if (callsign != firstNearTrackerCallsign) {
      if (distance < firstDistance) {
        secondNearTracker = firstNearTracker;
        firstNearTracker  = newTrackerInfo;
      } else {
        secondNearTracker = newTrackerInfo;
      }
    } else { 
      if (distance != firstDistance) {
        firstNearTracker  = newTrackerInfo;
      }
    }
  } else if (firstNearTracker != "" && secondNearTracker != "" && thirdNearTracker == "" && fourthNearTracker == "") {
    if (callsign != firstNearTrackerCallsign && callsign != secondNearTrackerCallsign) {
      if (distance < firstDistance) {
        thirdNearTracker  = secondNearTracker;
        secondNearTracker = firstNearTracker;
        firstNearTracker  = newTrackerInfo;
      } else if (distance < secondDistance && distance >= firstDistance) {
        thirdNearTracker  = secondNearTracker;
        secondNearTracker = newTrackerInfo;
      } else if (distance >= secondDistance) {
        thirdNearTracker  = newTrackerInfo;
      }
    } else {  
      if (callsign == firstNearTrackerCallsign) {
        if (distance != firstDistance) {
          Serial.print("Distance Updated for : "); Serial.println(callsign);
          if (distance > secondDistance) {
            firstNearTracker  = secondNearTracker;
            secondNearTracker = newTrackerInfo;
          } else {
            firstNearTracker  = newTrackerInfo;
          }
        }
      } else if (callsign == secondNearTrackerCallsign) {
        if (distance != secondDistance) {
          Serial.print("Distance Updated for : "); Serial.println(callsign);
          if (distance < firstDistance) {
            secondNearTracker = firstNearTracker;
            firstNearTracker  = newTrackerInfo;
          } else {
            secondNearTracker = newTrackerInfo;
          }
        }
      }     
    }
  } else if (firstNearTracker != "" && secondNearTracker != "" && thirdNearTracker != "" && fourthNearTracker == "") {
    if (callsign != firstNearTrackerCallsign && callsign != secondNearTrackerCallsign && callsign != thirdNearTrackerCallsign) {
      if (distance < firstDistance) {
        fourthNearTracker = thirdNearTracker;
        thirdNearTracker  = secondNearTracker;
        secondNearTracker = firstNearTracker;
        firstNearTracker  = newTrackerInfo;
      } else if (distance >= firstDistance && distance < secondDistance) {
        fourthNearTracker = thirdNearTracker;
        thirdNearTracker  = secondNearTracker;
        secondNearTracker = newTrackerInfo;
      } else if (distance >= secondDistance && distance < thirdDistance) {
        fourthNearTracker = thirdNearTracker;
        thirdNearTracker  = newTrackerInfo;
      } else if (distance >= thirdDistance) {
        fourthNearTracker = newTrackerInfo;
      }
    } else {  
      if (callsign == firstNearTrackerCallsign) {
        if (distance != firstDistance) {
          Serial.print("Distance Updated for : "); Serial.println(callsign);
          if (distance > thirdDistance) {
            firstNearTracker  = secondNearTracker;
            secondNearTracker = thirdNearTracker;
            thirdNearTracker  = newTrackerInfo;
          } else if (distance <= thirdDistance && distance > secondDistance) {
            firstNearTracker  = secondNearTracker;
            secondNearTracker = newTrackerInfo;
          } else if (distance <= secondDistance) {
            firstNearTracker  = newTrackerInfo;
          }
        }
      } else if (callsign == secondNearTrackerCallsign) {
        if (distance != secondDistance) {
          Serial.print("Distance Updated for : "); Serial.println(callsign);
          if (distance > thirdDistance) {
            secondNearTracker = thirdNearTracker;
            thirdNearTracker  = newTrackerInfo;
          } else if (distance <= thirdDistance && distance > firstDistance) {
            secondNearTracker = newTrackerInfo;
          } else if (distance <= firstDistance) {
            secondNearTracker = firstNearTracker;
            firstNearTracker  = newTrackerInfo;
          }
        }
      } else if (callsign == thirdNearTrackerCallsign) {
        if (distance != thirdDistance) {
          Serial.print("Distance Updated for : "); Serial.println(callsign);
          if (distance <= firstDistance) {
            thirdNearTracker  = secondNearTracker;
            secondNearTracker = firstNearTracker;
            firstNearTracker  = newTrackerInfo;
          } else if (distance > firstDistance && distance <= secondDistance) {
            thirdNearTracker  = secondNearTracker;
            secondNearTracker = newTrackerInfo;
          } else if (distance > secondDistance) {
            thirdNearTracker  = newTrackerInfo;
          }
        }
      }  
    }
  } else if (firstNearTracker != "" && secondNearTracker != "" && thirdNearTracker != "" && fourthNearTracker != "") {
    if (callsign != firstNearTrackerCallsign && callsign != secondNearTrackerCallsign && callsign != thirdNearTrackerCallsign && callsign != fourthNearTrackerCallsign) {
      if (distance < firstDistance) {
        fourthNearTracker = thirdNearTracker;
        thirdNearTracker  = secondNearTracker;
        secondNearTracker = firstNearTracker;
        firstNearTracker  = newTrackerInfo;
      } else if (distance < secondDistance && distance >= firstDistance) {
        fourthNearTracker = thirdNearTracker;
        thirdNearTracker  = secondNearTracker;
        secondNearTracker = newTrackerInfo;
      } else if (distance < thirdDistance && distance >= secondDistance) {

        fourthNearTracker = thirdNearTracker;
        thirdNearTracker  = newTrackerInfo;
      } else if (distance < fourthDistance && distance >= thirdDistance) {
        fourthNearTracker = newTrackerInfo;
      }
    } else {
      if (callsign == firstNearTrackerCallsign) {
        if (distance != firstDistance) {
          Serial.print("Distance Updated for : "); Serial.println(callsign);
          if (distance > fourthDistance) {
            firstNearTracker  = secondNearTracker;
            secondNearTracker = thirdNearTracker;
            thirdNearTracker  = fourthNearTracker;
            fourthNearTracker = newTrackerInfo;
          } else if (distance > thirdDistance && distance <= fourthDistance) {
            firstNearTracker  = secondNearTracker;
            secondNearTracker = thirdNearTracker;
            thirdNearTracker  = newTrackerInfo;
          } else if (distance > secondDistance && distance <= thirdDistance) {
            firstNearTracker  = secondNearTracker;
            secondNearTracker = newTrackerInfo;
          } else if (distance <= secondDistance) {
            firstNearTracker  = newTrackerInfo;
          }
        }
      } else if (callsign == secondNearTrackerCallsign) {
        if (distance != secondDistance) {
          Serial.print("Distance Updated for : "); Serial.println(callsign);
          if (distance > fourthDistance) {
            secondNearTracker = thirdNearTracker;
            thirdNearTracker  = fourthNearTracker;
            fourthNearTracker = newTrackerInfo;
          } else if (distance > thirdDistance && distance <= fourthDistance) {
            secondNearTracker = thirdNearTracker;
            thirdNearTracker  = newTrackerInfo;
          } else if (distance > firstDistance && distance <= thirdDistance) {
            secondNearTracker = newTrackerInfo;
          } else if (distance <= firstDistance) {
            secondNearTracker = firstNearTracker;
            firstNearTracker  = newTrackerInfo;
          }
        }
      } else if (callsign == thirdNearTrackerCallsign) {
        if (distance != thirdDistance) {
          Serial.print("Distance Updated for : "); Serial.println(callsign);
          if (distance > fourthDistance) {
            thirdNearTracker  = fourthNearTracker;
            fourthNearTracker = newTrackerInfo;
          } else if (distance > secondDistance && distance <= fourthDistance) {
            thirdNearTracker  = newTrackerInfo;
          } else if (distance > firstDistance && distance <= secondDistance) {
            thirdNearTracker  = secondNearTracker;
            secondNearTracker = newTrackerInfo;
          } else if (distance <= firstDistance) {
            thirdNearTracker  = secondNearTracker;
            secondNearTracker = firstNearTracker;
            firstNearTracker  = newTrackerInfo;
          }
        }
      } else if (callsign == fourthNearTrackerCallsign) {
        if (distance != fourthDistance) {
          Serial.print("Distance Updated for : "); Serial.println(callsign);
          if (distance > thirdDistance) {
            fourthNearTracker = newTrackerInfo;
          } else if (distance > secondDistance && distance <= thirdDistance) {
            fourthNearTracker = thirdNearTracker;
            thirdNearTracker  = newTrackerInfo;
          } else if (distance > firstDistance && distance <= secondDistance) {
            fourthNearTracker = thirdNearTracker;
            thirdNearTracker  = secondNearTracker;
            secondNearTracker = newTrackerInfo;
          } else if (distance <= firstDistance) {
            fourthNearTracker = thirdNearTracker;
            thirdNearTracker  = secondNearTracker;
            secondNearTracker = firstNearTracker;
            firstNearTracker  = newTrackerInfo;
          }
        }
      }       
    }
  }
}

void deleteListenedTrackersbyTime() {
  String firstNearTrackermillis, secondNearTrackermillis, thirdNearTrackermillis, fourthNearTrackermillis;
  uint32_t firstTrackermillis, secondTrackermillis, thirdTrackermillis, fourthTrackermillis;
  if (firstNearTracker != "") {
    firstNearTrackermillis = firstNearTracker.substring(firstNearTracker.indexOf(",")+1);
    firstTrackermillis = firstNearTrackermillis.toInt();
    if ((millis() - firstTrackermillis) > Config.listeningTrackerTime*60*1000) {
      firstNearTracker = "";
    }
  }
  if (secondNearTracker != "") {
    secondNearTrackermillis = secondNearTracker.substring(secondNearTracker.indexOf(",")+1);
    secondTrackermillis = secondNearTrackermillis.toInt();
    if ((millis() - secondTrackermillis) > Config.listeningTrackerTime*60*1000) {
      secondNearTracker = "";
    }
  }
  if (thirdNearTracker != "") {
    thirdNearTrackermillis = thirdNearTracker.substring(thirdNearTracker.indexOf(",")+1);
    thirdTrackermillis = thirdNearTrackermillis.toInt();
    if ((millis() - thirdTrackermillis) > Config.listeningTrackerTime*60*1000) {
      thirdNearTracker = "";
    }
  }
  if (fourthNearTracker != "") {
    fourthNearTrackermillis = fourthNearTracker.substring(fourthNearTracker.indexOf(",")+1);
    fourthTrackermillis = fourthNearTrackermillis.toInt();
    if ((millis() - fourthTrackermillis) > Config.listeningTrackerTime*60*1000) {
      fourthNearTracker = "";
    }
  }

  if (thirdNearTracker == "") {
    thirdNearTracker = fourthNearTracker;
    fourthNearTracker = "";
  } 
  if (secondNearTracker == "") {
    secondNearTracker = thirdNearTracker;
    thirdNearTracker = fourthNearTracker;
    fourthNearTracker = "";
  }
  if  (firstNearTracker == "") {
    firstNearTracker = secondNearTracker;
    secondNearTracker = thirdNearTracker;
    thirdNearTracker = fourthNearTracker;
    fourthNearTracker = "";
  }
  lastDeleteListenedTracker = millis();
}

void calculateDistanceCourse(String Callsign, double checkpointLatitude, double checkPointLongitude) {
  double distanceKm             = TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(), checkpointLatitude, checkPointLongitude) / 1000.0;
  double courseTo               = TinyGPSPlus::courseTo(gps.location.lat(), gps.location.lng(), checkpointLatitude, checkPointLongitude);
  deleteListenedTrackersbyTime();
  orderListenedTrackersByDistance(Callsign, distanceKm, courseTo);
}

void decodeEncodedGPS(String packet, String sender) {
  String GPSPacket = packet.substring(packet.indexOf(":!/")+3);
  String encodedLatitude    = GPSPacket.substring(0,4);
  String encodedLongtitude  = GPSPacket.substring(4,8);

  int Y1 = int(encodedLatitude[0]);
  int Y2 = int(encodedLatitude[1]);
  int Y3 = int(encodedLatitude[2]);
  int Y4 = int(encodedLatitude[3]);
  float decodedLatitude = 90 - ((((Y1-33) * pow(91,3)) + ((Y2-33) * pow(91,2)) + ((Y3-33) * 91) + Y4-33) / 380926);
    
  int X1 = int(encodedLongtitude[0]);
  int X2 = int(encodedLongtitude[1]);
  int X3 = int(encodedLongtitude[2]);
  int X4 = int(encodedLongtitude[3]);
  float decodedLongitude = -180 + ((((X1-33) * pow(91,3)) + ((X2-33) * pow(91,2)) + ((X3-33) * 91) + X4-33) / 190463);
    
  Serial.print(sender); 
  Serial.print(" GPS : "); 
  Serial.print(decodedLatitude); Serial.print(" N "); 
  Serial.print(decodedLongitude);Serial.println(" E");

  calculateDistanceCourse(sender, decodedLatitude, decodedLongitude);
}

void getReceivedGPS(String packet, String sender) {
  String infoGPS;
  if (packet.indexOf(":!") > 10) {
    infoGPS = packet.substring(packet.indexOf(":!")+2);
  } else if (packet.indexOf(":=") > 10) {
    infoGPS = packet.substring(packet.indexOf(":=")+2);
  }
  String Latitude       = infoGPS.substring(0,8);
  String Longitude      = infoGPS.substring(9,18);

  float convertedLatitude, convertedLongitude;
  String firstLatPart   = Latitude.substring(0,2);
  String secondLatPart  = Latitude.substring(2,4);
  String thirdLatPart   = Latitude.substring(Latitude.indexOf(".")+1,Latitude.indexOf(".")+3);
  String firstLngPart   = Longitude.substring(0,3);
  String secondLngPart  = Longitude.substring(3,5);
  String thirdLngPart   = Longitude.substring(Longitude.indexOf(".")+1,Longitude.indexOf(".")+3);
  convertedLatitude     = firstLatPart.toFloat() + (secondLatPart.toFloat()/60) + (thirdLatPart.toFloat()/(60*100));
  convertedLongitude    = firstLngPart.toFloat() + (secondLngPart.toFloat()/60) + (thirdLngPart.toFloat()/(60*100));
  
  String LatSign = String(Latitude[7]);
  String LngSign = String(Longitude[8]);
  if (LatSign == "S") {
    convertedLatitude = -convertedLatitude;
  } 
  if (LngSign == "W") {
    convertedLongitude = -convertedLongitude;
  } 
  Serial.print(sender); 
  Serial.print(" GPS : "); 
  Serial.print(convertedLatitude); Serial.print(" N "); 
  Serial.print(convertedLongitude);Serial.println(" E");

  calculateDistanceCourse(sender, convertedLatitude, convertedLongitude);
}

void checkReceivedMessage(String packetReceived) {
  Serial.println(packetReceived);
  String Sender, AddresseeAndMessage, Addressee, receivedMessage, ackMessage, iGate;
  if (packetReceived.substring(0,3) == "\x3c\xff\x01") {              // its an APRS packet
    Sender = packetReceived.substring(3,packetReceived.indexOf(">"));
    if (Sender != currentBeacon->callsign) {                          // avoid listening yourself by digirepeating                                
      if (packetReceived.indexOf("::") > 10) {                        // its a Message!
        AddresseeAndMessage = packetReceived.substring(packetReceived.indexOf("::")+2);  
        Addressee = AddresseeAndMessage.substring(0,AddresseeAndMessage.indexOf(":"));
        Addressee.trim();
        iGate = packetReceived.substring(packetReceived.indexOf("TCPIP,")+6,packetReceived.indexOf("::"));
        if (Addressee == currentBeacon->callsign) {                   // its for me!
          if (AddresseeAndMessage.indexOf("{")>0) {                   // ack?
            ackMessage = "ack" + AddresseeAndMessage.substring(AddresseeAndMessage.indexOf("{")+1);
            ackMessage.trim();
            delay(4000);
            sendMessage(Sender, ackMessage);
            receivedMessage = AddresseeAndMessage.substring(AddresseeAndMessage.indexOf(":")+1, AddresseeAndMessage.indexOf("{"));
          } else {
            receivedMessage = AddresseeAndMessage.substring(AddresseeAndMessage.indexOf(":")+1);
          }
          //Serial.println(receivedMessage);
          if (receivedMessage.indexOf("ping")==0 || receivedMessage.indexOf("Ping")==0 || receivedMessage.indexOf("PING")==0) {
            delay(4000);
            sendMessage(Sender, "pong, 73!");
          }
          if (Sender == "CD2RXU-15") {
            if (receivedMessage.indexOf("WX")==0) {                   // WX = WeatherReport
              Serial.println("Weather Report Received");
              String wxCleaning     = receivedMessage.substring(receivedMessage.indexOf("WX ")+3);
              String place          = wxCleaning.substring(0,wxCleaning.indexOf(","));
              String placeCleaning  = wxCleaning.substring(wxCleaning.indexOf(",")+1);
              String summary        = placeCleaning.substring(0,placeCleaning.indexOf(","));
              String sumCleaning    = placeCleaning.substring(placeCleaning.indexOf(",")+2);
              String temperature    = sumCleaning.substring(0,sumCleaning.indexOf("P"));
              String tempCleaning   = sumCleaning.substring(sumCleaning.indexOf("P")+1);
              String pressure       = tempCleaning.substring(0,tempCleaning.indexOf("H"));
              String presCleaning   = tempCleaning.substring(tempCleaning.indexOf("H")+1);
              String humidity       = presCleaning.substring(0,presCleaning.indexOf("W"));
              String humCleaning    = presCleaning.substring(presCleaning.indexOf("W")+1);
              String windSpeed      = humCleaning.substring(0,humCleaning.indexOf(","));
              String windCleaning   = humCleaning.substring(humCleaning.indexOf(",")+1);
              String windDegrees    = windCleaning.substring(windCleaning.indexOf(",")+1,windCleaning.indexOf("\n"));

              String fifthLineWR    = temperature + "C  " + pressure + "hPa  " + humidity +"%";
              String sixthLineWR    = "(wind " + windSpeed + "m/s " + windDegrees + "deg)";
              show_display("<WEATHER>", "From --> " + Sender, place, summary, fifthLineWR, sixthLineWR , 6000);
            } else {
              show_display("< MSG Rx >", "From --> " + Sender, "", receivedMessage , 3000);
              saveNewMessage("APRS", Sender, iGate, receivedMessage);
            }
          } else {
            show_display("<  MSG  >", "From --> " + Sender, "", receivedMessage , 3000);
            saveNewMessage("APRS", Sender, iGate, receivedMessage);
          }
        }
      } else if (packetReceived.indexOf(":!") > 10 || packetReceived.indexOf(":=") > 10 ) {     // packetReceived has APRS - GPS info
        lastHeardTracker = Sender; // eliminar?
        if (packetReceived.indexOf(":!/") > 10) {         // encoded GPS
          decodeEncodedGPS(packetReceived, Sender);
        } else {
          getReceivedGPS(packetReceived, Sender);         // not encoded GPS          }
        }
      }
    }
  }
}

// cppcheck-suppress unusedFunction
void setup() {
  Serial.begin(115200);

  #ifdef TTGO_T_Beam_V1_0
    Wire.begin(SDA, SCL);
    if (!powerManagement.begin(Wire)) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "AXP192", "init done!");
    } else {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "AXP192", "init failed!");
    }
    powerManagement.activateLoRa();
    powerManagement.activateOLED();
    powerManagement.activateGPS();
    powerManagement.activateMeasurement();
  #endif

  delay(500);
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "RichonGuzman -> CD2RXU --> LoRa APRS Tracker/Station");
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Version: " VERSION);
  setup_display();

  show_display(" LoRa APRS", "", "     Richonguzman", "     -- CD2RXU --", "", "      " VERSION, 4000);
  validateConfigFile();
  loadNumMessages();
  setup_gps();
  setup_lora();

  WiFi.mode(WIFI_OFF);
  btStop();
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "WiFi and BT controller stopped");
  esp_bt_controller_disable();
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "BT controller disabled");

  userButton.attachClick(ButtonSinglePress);
  userButton.attachLongPressStart(ButtonLongPress);
  userButton.attachDoubleClick(ButtonDoublePress);

  #if defined(TTGO_T_Beam_V1_0) 
  if (setCpuFrequencyMhz(80)) {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "CPU frequency set to 80MHz");
  } else {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "CPU frequency unchanged");
  }
  #endif

  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Smart Beacon is: %s", getSmartBeaconState());
  logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Setup Done!");
  delay(500);
}

// cppcheck-suppress unusedFunction
void loop() {
  uint32_t lastDisplayTime = millis() - displayTime;
  if (displayEcoMode) {
    if (menuDisplay == 0 && lastDisplayTime >= Config.displayTimeout*1000) {
      display_toggle(false);
      displayState = false;
    }
  }

  currentBeacon = &Config.beacons[myBeaconsIndex];

  userButton.tick();

  while (neo6m_gps.available() > 0) {
    gps.encode(neo6m_gps.read());
  }

  bool gps_time_update = gps.time.isUpdated();
  bool gps_loc_update  = gps.location.isUpdated();

  String loraPacket = "";
  int packetSize = LoRa.parsePacket();  // Listening for LoRa Packets
  if (packetSize) {
    while (LoRa.available()) {
      int inChar = LoRa.read();
      loraPacket += (char)inChar;
    }
    checkReceivedMessage(loraPacket);
  }

  if (millis() - lastDeleteListenedTracker > Config.listeningTrackerTime*60*1000) {
    deleteListenedTrackersbyTime();
  }

  /*if (gps_loc_update != gps_loc_update_valid) {
    gps_loc_update_valid = gps_loc_update;
    if (gps_loc_update) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Loop", "GPS fix state went to VALID");
    } else {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Loop", "GPS fix state went to INVALID");
    }
  }*/

  static double       currentHeading          = 0;
  static double       previousHeading         = 0;

  if (gps.time.isValid()) {
    setTime(gps.time.hour(), gps.time.minute(), gps.time.second(), gps.date.day(), gps.date.month(), gps.date.year());
  }

  static double   lastTxLat             = 0.0;
  static double   lastTxLng             = 0.0;
  static double   lastTxDistance        = 0.0;
  static uint32_t txInterval            = 60000L;
  static uint32_t lastTxTime            = millis();
  static bool		  sendStandingUpdate 		= false;
  int 			      currentSpeed 			    = (int)gps.speed.kmph();

  static bool   BatteryIsConnected   = false;
  static String batteryVoltage       = "";
  static String batteryChargeCurrent = "";
  #ifdef TTGO_T_Beam_V1_0
    static unsigned int rate_limit_check_battery = 0;
    if (!(rate_limit_check_battery++ % 60))
      BatteryIsConnected = powerManagement.isBatteryConnect();
    if (BatteryIsConnected) {
      batteryVoltage       = String(powerManagement.getBatteryVoltage(), 2);
      batteryChargeCurrent = String(powerManagement.getBatteryChargeDischargeCurrent(), 0);
    }
  #endif

  if (powerManagement.isChargeing()) {
    powerManagement.enableChgLed();
  } else {
    powerManagement.disableChgLed();
  }

  if (!send_update && gps_loc_update && currentBeacon->smartBeaconState) {
    uint32_t lastTx = millis() - lastTxTime;
    currentHeading  = gps.course.deg();
    lastTxDistance  = TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(), lastTxLat, lastTxLng);
    if (lastTx >= txInterval) {
      if (lastTxDistance > currentBeacon->minTxDist) {
        send_update = true;
        sendStandingUpdate = false;
      }
    }

    if (!send_update) {
      int TurnMinAngle;
      double headingDelta = abs(previousHeading - currentHeading);
      if (lastTx > currentBeacon->minDeltaBeacon * 1000) {
        if (currentSpeed == 0) {
					TurnMinAngle = currentBeacon->turnMinDeg + (currentBeacon->turnSlope/(currentSpeed+1));
				} else {
          TurnMinAngle = currentBeacon->turnMinDeg + (currentBeacon->turnSlope/currentSpeed);
				}
				if (headingDelta > TurnMinAngle && lastTxDistance > currentBeacon->minTxDist) {
          send_update = true;
          sendStandingUpdate = false;
        }
      }
    }
    if (!send_update && lastTx >= Config.standingUpdateTime*60*1000) {
			send_update = true;
			sendStandingUpdate = true;
		}
  }

  if (!currentBeacon->smartBeaconState) {
    uint32_t lastTx = millis() - lastTxTime;
    if (lastTx >= Config.nonSmartBeaconRate*60*1000) {
      send_update = true;
    }
  }

  if (send_update && gps_loc_update) {
    APRSMessage msg;
    msg.setSource(currentBeacon->callsign);
    msg.setDestination(Config.destination);
    msg.setPath(Config.path);
    

    float Tlat, Tlon;
    float Tspeed=0, Tcourse=0;
    Tlat    = gps.location.lat();
    Tlon    = gps.location.lng();
    Tcourse = gps.course.deg();
    Tspeed  = gps.speed.knots();

    uint32_t aprs_lat, aprs_lon;
    aprs_lat = 900000000 - Tlat * 10000000;
    aprs_lat = aprs_lat / 26 - aprs_lat / 2710 + aprs_lat / 15384615;
    aprs_lon = 900000000 + Tlon * 10000000 / 2;
    aprs_lon = aprs_lon / 26 - aprs_lon / 2710 + aprs_lon / 15384615;

    String Ns, Ew, helper;
    if(Tlat < 0) { Ns = "S"; } else { Ns = "N"; }
    if(Tlat < 0) { Tlat= -Tlat; }

    if(Tlon < 0) { Ew = "W"; } else { Ew = "E"; }
    if(Tlon < 0) { Tlon= -Tlon; }

    String infoField = "!";
    infoField += Config.overlay;

    char helper_base91[] = {"0000\0"};
    int i;
    ax25_base91enc(helper_base91, 4, aprs_lat);
    for (i=0; i<4; i++) {
      infoField += helper_base91[i];
      }
    ax25_base91enc(helper_base91, 4, aprs_lon);
    for (i=0; i<4; i++) {
      infoField += helper_base91[i];
    }
    
    infoField += currentBeacon->symbol;

    if (Config.sendAltitude) {      // Send Altitude or... (APRS calculates Speed also)
      int Alt1, Alt2;
      int Talt;
      Talt = gps.altitude.feet();
      if(Talt>0){
        double ALT=log(Talt)/log(1.002);
        Alt1= int(ALT/91);
        Alt2=(int)ALT%91;
      }else{
        Alt1=0;
        Alt2=0;
      }
      if (sendStandingUpdate) {
        infoField += " ";
      } else {
        infoField +=char(Alt1+33);
      }
      infoField +=char(Alt2+33);
      infoField +=char(0x30+33);
    } else {                      // ... just send Course and Speed
      ax25_base91enc(helper_base91, 1, (uint32_t) Tcourse/4 );
      if (sendStandingUpdate) {
        infoField += " ";
      } else {
        infoField += helper_base91[0];
      }
      ax25_base91enc(helper_base91, 1, (uint32_t) (log1p(Tspeed)/0.07696));
      infoField += helper_base91[0];
      infoField += "\x47";
    }

    if (currentBeacon->comment != "") {
      infoField += currentBeacon->comment;
    }

    msg.getBody()->setData(infoField);
    String data = msg.encode();
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Loop", "%s", data.c_str());
    show_display("<<< TX >>>", "", data);

    sendNewLoraPacket(data);

    if (currentBeacon->smartBeaconState) {
      lastTxLat       = gps.location.lat();
      lastTxLng       = gps.location.lng();
      previousHeading = currentHeading;
      lastTxDistance  = 0.0;
    }
    lastTxTime = millis();
    send_update = false;

    if (Config.defaultStatusAfterBoot && statusAfterBootState) {
      startingStatus();
    }
  }

  if (gps_time_update) {
    switch (menuDisplay) { // Graphic Menu is in here!!!!
      case 1:
        show_display("__MENU_1__", "", "1P -> Read Msg (" + String(numAPRSMessages) + ")", "LP -> Delete Msg", "2P -> Menu 2");
        break;
      case 2:
        show_display("__MENU_2__", "", "1P -> Weather Report", "LP -> Listen Trackers", "2P -> Menu 3");
        break;
      case 3:
        show_display("__MENU_3__", "", "1P -> Nothing Yet", "LP -> Display EcoMode", "2P -> (Back) Tracking");
        break;

      case 10:            // Display Received/Saved APRS Messages
        {
          String msgSender      = loadedAPRSMessages[messagesIterator].substring(0, loadedAPRSMessages[messagesIterator].indexOf(","));
          String restOfMessage  = loadedAPRSMessages[messagesIterator].substring(loadedAPRSMessages[messagesIterator].indexOf(",")+1);
          String msgGate        = restOfMessage.substring(0,restOfMessage.indexOf(","));
          String msgText        = restOfMessage.substring(restOfMessage.indexOf(",")+1);
          show_display("MSG_APRS>", msgSender + "-->" + msgGate, msgText, "", "", "               Next>");
        }
        break;

      case 20:            // Display Heared Tracker/Stations
        show_display("LISTENING>", String(firstNearTracker.substring(0,firstNearTracker.indexOf(","))), String(secondNearTracker.substring(0,secondNearTracker.indexOf(","))), String(thirdNearTracker.substring(0,thirdNearTracker.indexOf(","))), String(fourthNearTracker.substring(0,fourthNearTracker.indexOf(","))), "<Back");
        break;

      case 0:       ///////////// MAIN MENU //////////////
        String hdopState, firstRowMainMenu, secondRowMainMenu, thirdRowMainMenu, fourthRowMainMenu, fifthRowMainMenu, sixthRowMainMenu;;

        firstRowMainMenu = currentBeacon->callsign;
        secondRowMainMenu = createDateString(now()) + "   " + createTimeString(now());
        
        if (gps.hdop.hdop() > 5) {
          hdopState = "X";
        } else if (gps.hdop.hdop() > 2 && gps.hdop.hdop() < 5) {
          hdopState = "-";
        } else if (gps.hdop.hdop() <= 2) {
          hdopState = "+";
        }
        thirdRowMainMenu = String(gps.location.lat(), 4) + " " + String(gps.location.lng(), 4);
        for(int i = thirdRowMainMenu.length(); i < 18; i++) {
          thirdRowMainMenu += " ";
        }
        if (gps.satellites.value() > 9) { 
          thirdRowMainMenu += String(gps.satellites.value()) + hdopState;
        } else {
          thirdRowMainMenu += " " + String(gps.satellites.value()) + hdopState;
        }
        
        String fourthRowAlt = String(gps.altitude.meters(),0);
        fourthRowAlt.trim();
        for (int a=fourthRowAlt.length();a<4;a++) {
          fourthRowAlt = "0" + fourthRowAlt;
        }
        String fourthRowSpeed = String(gps.speed.kmph(),0);
        fourthRowSpeed.trim();
        for (int b=fourthRowSpeed.length(); b<3;b++) {
          fourthRowSpeed = " " + fourthRowSpeed;
        }
        String fourthRowCourse = String(gps.course.deg(),0);
        if (fourthRowSpeed == "  0") {
          fourthRowCourse = "---";
        } else {
          fourthRowCourse.trim();
          for(int c=fourthRowCourse.length();c<3;c++) {
            fourthRowCourse = "0" + fourthRowCourse;
          }
        }
        fourthRowMainMenu = "A=" + fourthRowAlt + "m  " + fourthRowSpeed + "km/h  " + fourthRowCourse;
        if (numAPRSMessages > 0){
          fourthRowMainMenu = "*** MESSAGES: " + String(numAPRSMessages) + " ***";
        }
                
        fifthRowMainMenu  = "LAST Rx = " + lastHeardTracker;
            
        if (BatteryIsConnected) {
          if (batteryChargeCurrent.toInt() == 0) {
            sixthRowMainMenu = "Battery Charged " + String(batteryVoltage) + "V";
          } else if (batteryChargeCurrent.toInt() > 0) {
            sixthRowMainMenu = "Bat: " + String(batteryVoltage) + "V (charging)";
          } else {
            sixthRowMainMenu = "Battery " + String(batteryVoltage) + "V " + String(batteryChargeCurrent) + "mA";
          }
        } else {
          sixthRowMainMenu = "No Battery Connected." ;
        }
        show_display(String(firstRowMainMenu),
                    String(secondRowMainMenu),
                    String(thirdRowMainMenu),
                    String(fourthRowMainMenu),
                    String(fifthRowMainMenu),
                    String(sixthRowMainMenu));
        break;
    }
    

    if (currentBeacon->smartBeaconState) {
      if (currentSpeed < currentBeacon->slowSpeed) {
        txInterval = currentBeacon->slowRate * 1000;
      } else if (currentSpeed > currentBeacon->fastSpeed) {
        txInterval = currentBeacon->fastRate * 1000;
      } else {
        txInterval = min(currentBeacon->slowRate, currentBeacon->fastSpeed * currentBeacon->fastRate / currentSpeed) * 1000;
      }
    }
  }

  if ((millis() > 5000 && gps.charsProcessed() < 10)) {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "GPS",
               "No GPS frames detected! Try to reset the GPS Chip with this "
               "firmware: https://github.com/lora-aprs/TTGO-T-Beam_GPS-reset");
    show_display("No GPS frames detected!", "Try to reset the GPS Chip", "https://github.com/lora-aprs/TTGO-T-Beam_GPS-reset", 2000);
  }
}

/// FUNCTIONS ///
void validateConfigFile() {
  if (currentBeacon->callsign == "NOCALL-7") {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "Config", "Change your settings in 'data/tracker_config.json' and upload it via 'Upload File System image'");
    show_display("ERROR", "Change your settings", "'tracker_config.json'", "upload it via --> ", "'Upload File System image'");
    while (true) {}
  }
}

char *ax25_base91enc(char *s, uint8_t n, uint32_t v) {
  /* Creates a Base-91 representation of the value in v in the string */
  /* pointed to by s, n-characters long. String length should be n+1. */
  for(s += n, *s = '\0'; n; n--) {
    *(--s) = v % 91 + 33;
    v /= 91;
  }
  return(s);
}

String createDateString(time_t t) {
  return String(padding(year(t), 4) + "-" + padding(month(t), 2) + "-" + padding(day(t), 2));
}

String createTimeString(time_t t) {
  return String(padding(hour(t), 2) + ":" + padding(minute(t), 2) + ":" + padding(second(t), 2));
}

String getSmartBeaconState() {
  if (currentBeacon->smartBeaconState) {
    return "On";
  }
  return "Off";
}

String padding(unsigned int number, unsigned int width) {
  String result;
  String num(number);
  if (num.length() > width) {
    width = num.length();
  }
  for (unsigned int i = 0; i < width - num.length(); i++) {
    result.concat('0');
  }
  result.concat(num);
  return result;
}