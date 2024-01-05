#include <TinyGPS++.h>
#include <SPIFFS.h>
#include "APRSPacketLib.h"
#include "notification_utils.h"
#include "bluetooth_utils.h"
#include "configuration.h"
#include "lora_utils.h"
#include "ble_utils.h"
#include "msg_utils.h"
#include "gps_utils.h"
#include "display.h"
#include "logger.h"

extern Beacon               *currentBeacon;
extern logging::Logger      logger;
extern std::vector<String>  loadedAPRSMessages;
extern Configuration        Config;

extern int                  menuDisplay;
extern uint32_t             menuTime;

extern bool                 messageLed;
extern uint32_t             messageLedTime;

extern bool                 digirepeaterActive;

extern APRSPacket           lastReceivedPacket;

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
    String newPacket = APRSPacketLib::generateMessagePacket(currentBeacon->callsign,"APLRT1",Config.path,station,textMessage);  
    if (textMessage.indexOf("ack")== 0) {
      show_display("<<ACK Tx>>", 500);
    } else if (station.indexOf("CA2RXU-15") == 0 && textMessage.indexOf("wrl")==0) {
      show_display("<WEATHER>","", "--- Sending Query ---",  1000);
    } else {
      show_display("MSG Tx >>", "", newPacket, 1000);
    }
    LoRa_Utils::sendNewPacket(newPacket);
  }

  void checkReceivedMessage(ReceivedLoRaPacket packet) {
    if(packet.text.isEmpty()) {
      return;
    }
    if (packet.text.substring(0,3) == "\x3c\xff\x01") {              // its an APRS packet
      //Serial.println(packet.text); // only for debug
      lastReceivedPacket = APRSPacketLib::processReceivedPacket(packet.text.substring(3),packet.rssi, packet.snr, packet.freqError);
      if (lastReceivedPacket.sender!=currentBeacon->callsign) {
        if (Config.bluetoothType==0) {
          BLE_Utils::sendToPhone(packet.text.substring(3));
        } else {
          #if !defined(TTGO_T_Beam_S3_SUPREME_V3) && !defined(HELTEC_V3_GPS)
          BLUETOOTH_Utils::sendPacket(packet.text.substring(3));
          #endif
        }        

        if (digirepeaterActive && lastReceivedPacket.addressee!=currentBeacon->callsign) {
          String digiRepeatedPacket = APRSPacketLib::generateDigiRepeatedPacket(lastReceivedPacket, currentBeacon->callsign);
          if (digiRepeatedPacket == "X") {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "Main", "%s", "Packet won't be Repeated (Missing WIDE1-X)");
          } else {
            delay(500);
            LoRa_Utils::sendNewPacket(digiRepeatedPacket);
          }
        }
        lastHeardTracker = lastReceivedPacket.sender;
        if (lastReceivedPacket.type==1 && lastReceivedPacket.addressee==currentBeacon->callsign) {
          if (lastReceivedPacket.message.indexOf("{")>=0) {
            String ackMessage = "ack" + lastReceivedPacket.message.substring(lastReceivedPacket.message.indexOf("{")+1);
            ackMessage.trim();
            delay(4000);
            sendMessage(lastReceivedPacket.sender, ackMessage);
            lastReceivedPacket.message = lastReceivedPacket.message.substring(lastReceivedPacket.message.indexOf(":")+1, lastReceivedPacket.message.indexOf("{"));
          } else {
            lastReceivedPacket.message = lastReceivedPacket.message.substring(lastReceivedPacket.message.indexOf(":")+1);
          }
          if (Config.notification.buzzerActive && Config.notification.messageRxBeep) {
            NOTIFICATION_Utils::messageBeep();
          }
          if (lastReceivedPacket.message.indexOf("ping")==0 || lastReceivedPacket.message.indexOf("Ping")==0 || lastReceivedPacket.message.indexOf("PING")==0) {
            delay(4000);
            sendMessage(lastReceivedPacket.sender, "pong, 73!");
          }
          if (lastReceivedPacket.sender == "CA2RXU-15" && lastReceivedPacket.message.indexOf("WX")==0) {    // WX = WeatherReport
            Serial.println("Weather Report Received");
            String wxCleaning     = lastReceivedPacket.message.substring(lastReceivedPacket.message.indexOf("WX ")+3);
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
            show_display("<WEATHER>", "From --> " + lastReceivedPacket.sender, place, summary, fifthLineWR, sixthLineWR);
            menuDisplay = 40;
            menuTime = millis();
          } else {
            show_display("< MSG Rx >", "From --> " + lastReceivedPacket.sender, "", lastReceivedPacket.message , 3000);
            if (!Config.simplifiedTrackerMode) {
              saveNewMessage("APRS", lastReceivedPacket.sender, lastReceivedPacket.message);
            }
          } 
        } else {
          if ((lastReceivedPacket.type==0 || lastReceivedPacket.type==4) && !Config.simplifiedTrackerMode) {
            GPS_Utils::calculateDistanceCourse(lastReceivedPacket.sender, lastReceivedPacket.latitude, lastReceivedPacket.longitude);
          }
          if (Config.notification.buzzerActive && Config.notification.stationBeep && !digirepeaterActive) {
            NOTIFICATION_Utils::stationHeardBeep();
          }
        }
      }
    }
  }

}