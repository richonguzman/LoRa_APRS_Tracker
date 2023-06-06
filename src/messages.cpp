#include <Arduino.h>
#include <TinyGPS++.h>
#include "messages.h"
#include "configuration.h"
#include "logger.h"
#include "display.h"
#include "lora_utils.h"

extern Beacon               *currentBeacon;
extern logging::Logger      logger;
extern std::vector<String>  loadedAPRSMessages;
extern TinyGPSPlus          gps;
extern Configuration        Config;

namespace messages {

static String   lastMessageAPRS             = "";
static int      numAPRSMessages             = 0;
static bool     noMessageWarning            = false;
static String   lastHeardTracker            = "NONE";
static uint32_t lastDeleteListenedTracker   = millis();

static String   firstNearTracker            = "";
static String   secondNearTracker           = "";
static String   thirdNearTracker            = "";
static String   fourthNearTracker           = "";

bool warnNoMessages() {
    return noMessageWarning;
}

String getLastHeardTracker() {
    return lastHeardTracker;
}

int getNumAPRSMessages() {
    return numAPRSMessages;
}

String getFirstNearTracker() {
    return String(firstNearTracker.substring(0,firstNearTracker.indexOf(",")));
}

String getSecondNearTracker() {
    return String(secondNearTracker.substring(0,secondNearTracker.indexOf(",")));
}

String getThirdNearTracker() {
    return String(thirdNearTracker.substring(0,thirdNearTracker.indexOf(",")));
}

String getFourthNearTracker() {
    return String(fourthNearTracker.substring(0,fourthNearTracker.indexOf(",")));
}

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
    if ((millis() - firstTrackermillis) > Config.rememberStationTime*60*1000) {
      firstNearTracker = "";
    }
  }
  if (secondNearTracker != "") {
    secondNearTrackermillis = secondNearTracker.substring(secondNearTracker.indexOf(",")+1);
    secondTrackermillis = secondNearTrackermillis.toInt();
    if ((millis() - secondTrackermillis) > Config.rememberStationTime*60*1000) {
      secondNearTracker = "";
    }
  }
  if (thirdNearTracker != "") {
    thirdNearTrackermillis = thirdNearTracker.substring(thirdNearTracker.indexOf(",")+1);
    thirdTrackermillis = thirdNearTrackermillis.toInt();
    if ((millis() - thirdTrackermillis) > Config.rememberStationTime*60*1000) {
      thirdNearTracker = "";
    }
  }
  if (fourthNearTracker != "") {
    fourthNearTrackermillis = fourthNearTracker.substring(fourthNearTracker.indexOf(",")+1);
    fourthTrackermillis = fourthNearTrackermillis.toInt();
    if ((millis() - fourthTrackermillis) > Config.rememberStationTime*60*1000) {
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

void checkListenedTrackersByTimeAndDelete() {
  if (millis() - lastDeleteListenedTracker > Config.rememberStationTime*60*1000) {
    deleteListenedTrackersbyTime();
  }
}

void calculateDistanceCourse(String Callsign, double checkpointLatitude, double checkPointLongitude) {
  double distanceKm = TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(), checkpointLatitude, checkPointLongitude) / 1000.0;
  double courseTo   = TinyGPSPlus::courseTo(gps.location.lat(), gps.location.lng(), checkpointLatitude, checkPointLongitude);
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
  float decodedLatitude = 90.0 - ((((Y1-33) * pow(91,3)) + ((Y2-33) * pow(91,2)) + ((Y3-33) * 91) + Y4-33) / 380926.0);
    
  int X1 = int(encodedLongtitude[0]);
  int X2 = int(encodedLongtitude[1]);
  int X3 = int(encodedLongtitude[2]);
  int X4 = int(encodedLongtitude[3]);
  float decodedLongitude = -180.0 + ((((X1-33) * pow(91,3)) + ((X2-33) * pow(91,2)) + ((X3-33) * 91) + X4-33) / 190463.0);
    
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

void saveNewMessage(String typeMessage, String station, String newMessage) {
  if (typeMessage == "APRS" && lastMessageAPRS != newMessage) {
    File fileToAppendAPRS = SPIFFS.open("/aprsMessages.txt", FILE_APPEND);
    if(!fileToAppendAPRS){
      Serial.println("There was an error opening the file for appending");
      return;
    }
    newMessage.trim();
    if(!fileToAppendAPRS.println("1," + station + "," + newMessage)){
      Serial.println("File append failed");
    }
    lastMessageAPRS = newMessage;
    numAPRSMessages++;
    fileToAppendAPRS.close();
  }
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
  LoRaUtils::sendNewPacket(messageToSend);
}

void checkReceivedMessage(String packetReceived) {
  if(packetReceived.isEmpty()) {
    return;
  }
  String Sender, AddresseeAndMessage, Addressee, receivedMessage, ackMessage;
  if (packetReceived.substring(0,3) == "\x3c\xff\x01") {              // its an APRS packet
    Sender = packetReceived.substring(3,packetReceived.indexOf(">"));
    if (Sender != currentBeacon->callsign) {                          // avoid listening yourself by digirepeating                                
      if (packetReceived.indexOf("::") > 10) {                        // its a Message!
        AddresseeAndMessage = packetReceived.substring(packetReceived.indexOf("::")+2);  
        Addressee = AddresseeAndMessage.substring(0,AddresseeAndMessage.indexOf(":"));
        Addressee.trim();
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
              saveNewMessage("APRS", Sender, receivedMessage);
            }
          } else {
            show_display("<  MSG  >", "From --> " + Sender, "", receivedMessage , 3000);
            saveNewMessage("APRS", Sender, receivedMessage);
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

}