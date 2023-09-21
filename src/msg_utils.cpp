#include <TinyGPS++.h>
#include <SPIFFS.h>
#include "notification_utils.h"
#include "bluetooth_utils.h"
#include "configuration.h"
#include "station_utils.h"
#include "lora_utils.h"
#include "msg_utils.h"
#include "gps_utils.h"
#include "display.h"
#include "logger.h"

extern Beacon               *currentBeacon;
extern logging::Logger      logger;
extern std::vector<String>  loadedAPRSMessages;
extern TinyGPSPlus          gps;
extern Configuration        Config;

extern int                  menuDisplay;
extern uint32_t             menuTime;

extern bool                 messageLed;
extern uint32_t             messageLedTime;

String   firstNearTracker            = "";
String   secondNearTracker           = "";
String   thirdNearTracker            = "";
String   fourthNearTracker           = "";

String   lastMessageAPRS             = "";
int      numAPRSMessages             = 0;
bool     noMessageWarning            = false;
String   lastHeardTracker            = "NONE";
uint32_t lastDeleteListenedTracker   = millis();

namespace MSG_Utils {

  bool warnNoMessages() {
      return noMessageWarning;
  }

  String getLastHeardTracker() {
      return lastHeardTracker;
  }

  int getNumAPRSMessages() {
      return numAPRSMessages;
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

  void ledNotification() {
    uint32_t ledTimeDelta = millis() - messageLedTime;
    if (messageLed && ledTimeDelta > 5*1000) {
      digitalWrite(Config.notification.ledMessagePin, HIGH);
      messageLedTime = millis();
    }
    uint32_t ledOnDelta = millis() - messageLedTime;
    if (messageLed && ledOnDelta > 1*1000) {
      digitalWrite(Config.notification.ledMessagePin, LOW);
    }
    if (!messageLed && digitalRead(Config.notification.ledMessagePin)==HIGH) {
      digitalWrite(Config.notification.ledMessagePin, LOW);
    }
  }

  void deleteFile() {
    if(!SPIFFS.begin(true)){
      Serial.println("An Error has occurred while mounting SPIFFS");
      return;
    }
    SPIFFS.remove("/aprsMessages.txt");
    if (Config.notification.ledMessage){
      messageLed = false;
    }
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
      if (Config.notification.ledMessage){
        messageLed = true;
      }
    }
  }

  void sendMessage(String station, String textMessage) {
    String messageToSend;
    for(int i = station.length(); i < 9; i++) {
      station += ' ';
    }
    messageToSend = currentBeacon->callsign + ">APLRT1";
    if (Config.path != "") {
      messageToSend += "," + Config.path;
    }
    messageToSend += "::" + station + ":" + textMessage;
    if (textMessage.indexOf("ack")== 0) {
      show_display("<<ACK Tx>>", 500);
    } else if (station.indexOf("CD2RXU-15") == 0 && textMessage.indexOf("wrl")==0) {
      show_display("<WEATHER>","", "--- Sending Query ---",  1000);
    } else {
      show_display("MSG Tx >>", "", messageToSend, 1000);
    }
    LoRa_Utils::sendNewPacket(messageToSend);
  }

  void checkReceivedMessage(String packetReceived) {
    if(packetReceived.isEmpty()) {
      return;
    }
    String Sender, AddresseeAndMessage, Addressee, receivedMessage, ackMessage;
    if (packetReceived.substring(0,3) == "\x3c\xff\x01") {              // its an APRS packet
      BLUETOOTH_Utils::sendPacket(packetReceived.substring(3));
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
            if (Config.notification.buzzerActive && Config.notification.messageRxBeep) {
              NOTIFICATION_Utils::messageBeep();
            }            
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
                show_display("<WEATHER>", "From --> " + Sender, place, summary, fifthLineWR, sixthLineWR);
                menuDisplay = 21;
                menuTime = millis();
              } else {
                show_display("< MSG Rx >", "From --> " + Sender, "", receivedMessage , 3000);
                if (!Config.simplifiedTrackerMode) {
                  saveNewMessage("APRS", Sender, receivedMessage);
                }
              }
            } else {
              show_display("<  MSG  >", "From --> " + Sender, "", receivedMessage , 3000);
              if (!Config.simplifiedTrackerMode) {
                saveNewMessage("APRS", Sender, receivedMessage);
              }
            }
          }
        } else if (packetReceived.indexOf(":!") > 10 || packetReceived.indexOf(":=") > 10 ) {     // packetReceived has APRS - GPS info
          lastHeardTracker = Sender;
          if (!Config.simplifiedTrackerMode) {
            int encodedBytePosition = 0;
            if (packetReceived.indexOf(":!") > 10) {
              encodedBytePosition = packetReceived.indexOf(":!") + 14;
            }
            if (packetReceived.indexOf(":=") > 10) {
              encodedBytePosition = packetReceived.indexOf(":=") + 14;
            }
            if (encodedBytePosition != 0) {
              if (Config.notification.buzzerActive && Config.notification.stationBeep) {
                NOTIFICATION_Utils::stationHeardBeep();
              }
              if (String(packetReceived[encodedBytePosition]) == "G" || String(packetReceived[encodedBytePosition]) == "Q" || String(packetReceived[encodedBytePosition]) == "[" || String(packetReceived[encodedBytePosition]) == "H") {
                GPS_Utils::decodeEncodedGPS(packetReceived, Sender);
              } else {
                GPS_Utils::getReceivedGPS(packetReceived, Sender); 
              }
            }
          }
        }
      }
    }
  }

}