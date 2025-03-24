#include <APRSPacketLib.h>
#include <TinyGPS++.h>
#include <SPIFFS.h>
#include "notification_utils.h"
#include "bluetooth_utils.h"
#include "winlink_utils.h"
#include "configuration.h"
#include "board_pinout.h"
#include "lora_utils.h"
#include "ble_utils.h"
#include "msg_utils.h"
#include "gps_utils.h"
#include "display.h"
#include "logger.h"


extern Beacon               *currentBeacon;
extern logging::Logger      logger;
extern Configuration        Config;

extern int                  menuDisplay;
extern uint32_t             menuTime;

extern bool                 messageLed;
extern uint32_t             messageLedTime;

extern bool                 digipeaterActive;

extern int                  ackRequestNumber;

extern uint32_t             lastTxTime;

extern uint8_t              winlinkStatus;
extern uint32_t             lastChallengeTime;

extern bool                 wxRequestStatus;
extern uint32_t             wxRequestTime;

extern APRSPacket           lastReceivedPacket;

extern bool                 SleepModeActive;

String  lastMessageSaved        = "";
int     numAPRSMessages         = 0;
int     numWLNKMessages         = 0;
bool    noAPRSMsgWarning        = false;
bool    noWLNKMsgWarning        = false;
String  lastHeardTracker        = "NONE";

std::vector<String>             loadedAPRSMessages;
std::vector<String>             loadedWLNKMails;
std::vector<String>             outputMessagesBuffer;
std::vector<String>             outputAckRequestBuffer;
std::vector<Packet15SegBuffer>  packet15SegBuffer;

bool        ackRequestState     = false;
String      ackCallsignRequest  = "";
String      ackNumberRequest    = "";
uint32_t    lastMsgRxTime       = millis();
uint32_t    lastRetryTime       = millis();

bool        messageLed          = false;
uint32_t    messageLedTime      = millis();


namespace MSG_Utils {

    bool warnNoAPRSMessages() {
        return noAPRSMsgWarning;
    }

    bool warnNoWLNKMails() {
        return noWLNKMsgWarning;
    }

    const String getLastHeardTracker() {
        return lastHeardTracker;
    }

    int getNumAPRSMessages() {
        return numAPRSMessages;
    }

    int getNumWLNKMails() {
        return numWLNKMessages;
    }

    void loadNumMessages() {
        if(!SPIFFS.begin(true)) {
            Serial.println("An Error has occurred while mounting SPIFFS");
            return;
        }

        File fileToReadAPRS = SPIFFS.open("/aprsMessages.txt");
        if(!fileToReadAPRS) {
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
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Main", "Number of APRS Messages : %s", String(numAPRSMessages));
    
        File fileToReadWLNK = SPIFFS.open("/winlinkMails.txt");
        if(!fileToReadWLNK) {
            Serial.println("Failed to open Winlink_Msg for reading");
            return;
        }

        std::vector<String> v2;
        while (fileToReadWLNK.available()) {
            v2.push_back(fileToReadWLNK.readStringUntil('\n'));
        }
        fileToReadWLNK.close();

        numWLNKMessages = 0;
        for (String s2 : v2) {
            numWLNKMessages++;
        }
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Main", "Number of Winlink Mails : %s", String(numWLNKMessages));
    }

    void loadMessagesFromMemory(uint8_t typeOfMessage) {
        File fileToRead;
        if (typeOfMessage == 0) {  // APRS
            noAPRSMsgWarning = false;
            if (numAPRSMessages == 0) {
                noAPRSMsgWarning = true;
            } else {
                loadedAPRSMessages.clear();
                fileToRead = SPIFFS.open("/aprsMessages.txt");
            }
            if (noAPRSMsgWarning) {
                displayShow("   INFO", "", " NO APRS MSG SAVED", 1500);
            } else {
                if(!fileToRead) {
                    Serial.println("Failed to open file for reading");
                    return;
                }
                while (fileToRead.available()) {
                    loadedAPRSMessages.push_back(fileToRead.readStringUntil('\n'));
                }
                fileToRead.close();
            }
        } else if (typeOfMessage == 1) { // WLNK
            noWLNKMsgWarning = false;
            if (numWLNKMessages == 0) {
                noWLNKMsgWarning = true;
            } else {
                loadedWLNKMails.clear();
                fileToRead = SPIFFS.open("/winlinkMails.txt");
            }
            if (noWLNKMsgWarning) {
                displayShow("   INFO", "", " NO WLNK MAILS SAVED", 1500);
            } else {
                if(!fileToRead) {
                    Serial.println("Failed to open file for reading");
                    return;
                }
                while (fileToRead.available()) {
                    loadedWLNKMails.push_back(fileToRead.readStringUntil('\n'));
                }
                fileToRead.close();
            }
        }
    }

    void ledNotification() {
        uint32_t currentTime = millis();
        uint32_t ledTimeDelta = currentTime - messageLedTime;
        if (messageLed) {
            if (ledTimeDelta > 5 * 1000) {
                digitalWrite(Config.notification.ledMessagePin, HIGH);
                messageLedTime = currentTime;
            } else if (ledTimeDelta > 1 * 1000) {
                digitalWrite(Config.notification.ledMessagePin, LOW);
            }
        } else if (!messageLed && digitalRead(Config.notification.ledMessagePin) == HIGH) {
            digitalWrite(Config.notification.ledMessagePin, LOW);
        }
    }

    void deleteFile(uint8_t typeOfFile) {
        if(!SPIFFS.begin(true)) {
            Serial.println("An Error has occurred while mounting SPIFFS");
            return;
        }
        if (typeOfFile == 0) {  //APRS
            SPIFFS.remove("/aprsMessages.txt");
        } else if (typeOfFile == 1) {   //WLNK
            SPIFFS.remove("/winlinkMails.txt");
        }    
        if (Config.notification.ledMessage) messageLed = false;
    }

    void saveNewMessage(uint8_t typeMessage, const String& station, const String& newMessage) {
        String message = newMessage;
        if (typeMessage == 0 && lastMessageSaved != message) {   //APRS
            File fileToAppendAPRS = SPIFFS.open("/aprsMessages.txt", FILE_APPEND);
            if(!fileToAppendAPRS) {
                Serial.println("There was an error opening the file for appending");
                return;
            }
            message.trim();
            if(!fileToAppendAPRS.println(station + "," + message)) {
                Serial.println("File append failed");
            }
            lastMessageSaved = message;
            numAPRSMessages++;
            fileToAppendAPRS.close();
            if (Config.notification.ledMessage) {
                messageLed = true;
            }
        } else if (typeMessage == 1 && lastMessageSaved != message) {    //WLNK
            File fileToAppendWLNK = SPIFFS.open("/winlinkMails.txt", FILE_APPEND);
            if(!fileToAppendWLNK) {
                Serial.println("There was an error opening the file for appending");
                return;
            }
            message.trim();
            if(!fileToAppendWLNK.println(message)) {
                Serial.println("File append failed");
            }
            lastMessageSaved = message;
            numWLNKMessages++;
            fileToAppendWLNK.close();
            if (Config.notification.ledMessage) {
                messageLed = true;
            }
        }
    }

    void sendMessage(const String& station, const String& textMessage) {
        String newPacket = APRSPacketLib::generateMessagePacket(currentBeacon->callsign, "APLRT1", Config.path, station, textMessage);
        if (textMessage.indexOf("ack") == 0 && station != "WLNK-1") {  // don't show Winlink ACK
            displayShow("<<ACK Tx>>", "", "", 500);
        } else if (station.indexOf("CA2RXU-15") == 0 && textMessage.indexOf("wrl") == 0) {
            displayShow("<WEATHER>","", "--- Sending Query ---",  1000);
            wxRequestTime = millis();
            wxRequestStatus = true;
        } else {
            displayShow((station == "WLNK-1") ? "WINLINK Tx" : " MSG Tx >", "", newPacket, 100);
        }
        LoRa_Utils::sendNewPacket(newPacket);
    }

    const String ackRequestNumberGenerator() {
        ackRequestNumber++;
        if (ackRequestNumber > 999) {
            ackRequestNumber = 1;
        }
        return String(ackRequestNumber);
    }

    void addToOutputBuffer(uint8_t typeOfMessage, const String& station, const String& textMessage) {
        bool alreadyInBuffer;
        if (typeOfMessage == 1) {
            alreadyInBuffer = false;
            if (!outputMessagesBuffer.empty()) {
                for (int i = 0; i < outputMessagesBuffer.size(); i++) {
                    if (outputMessagesBuffer[i].indexOf(station + "," + textMessage) == 0) {
                        alreadyInBuffer = true;
                        break;
                    }
                }
            }
            if (!outputAckRequestBuffer.empty()) {
                for (int j = 0; j < outputAckRequestBuffer.size(); j++) {
                    if (outputAckRequestBuffer[j].indexOf(station + "," + textMessage) > 1) {
                        alreadyInBuffer = true;
                        break;
                    }
                }
            }               
            if (!alreadyInBuffer) {
                outputMessagesBuffer.push_back(station + "," + textMessage + "{" + ackRequestNumberGenerator());
            }
        } else if (typeOfMessage == 0) {
            alreadyInBuffer = false;
            if (!outputMessagesBuffer.empty()) {
                for (int k = 0; k < outputMessagesBuffer.size(); k++) {
                    if (outputMessagesBuffer[k].indexOf(station + "," + textMessage) == 0) {
                        alreadyInBuffer = true;
                        break;
                    }
                }
            }
            if (!alreadyInBuffer) {
                outputMessagesBuffer.push_back(station + "," + textMessage);
            }
        }
    }

    void processOutputBuffer() {
        if (!outputMessagesBuffer.empty() && (millis() - lastMsgRxTime) >= 6000 && (millis() - lastTxTime) > 3000) {
            String addressee = outputMessagesBuffer[0].substring(0, outputMessagesBuffer[0].indexOf(","));
            String message = outputMessagesBuffer[0].substring(outputMessagesBuffer[0].indexOf(",") + 1);
            if (message.indexOf("{") > 0) {     // message with ack Request
                outputAckRequestBuffer.push_back("6," + addressee + "," + message);  // 6 is for ack packets retries
                outputMessagesBuffer.erase(outputMessagesBuffer.begin());
            } else {                            // message without ack Request
                sendMessage(addressee, message);
                outputMessagesBuffer.erase(outputMessagesBuffer.begin());
                lastTxTime = millis();
            }
        }
        if (outputAckRequestBuffer.empty()) {
            ackRequestState = false;
        } else if (!outputAckRequestBuffer.empty() && (millis() - lastMsgRxTime) >= 4500 && (millis() - lastTxTime) > 3000) {
            bool sendRetry = false;
            String triesLeft = outputAckRequestBuffer[0].substring(0 , outputAckRequestBuffer[0].indexOf(","));
            switch (triesLeft.toInt()) {
                case 6:
                    sendRetry = true;
                    ackRequestState = true;
                    break;
                case 5:
                    if (millis() - lastRetryTime > 30 * 1000) sendRetry = true;
                    break;
                case 4:
                    if (millis() - lastRetryTime > 60 * 1000) sendRetry = true;
                    break;
                case 3:
                    if (millis() - lastRetryTime > 120 * 1000) sendRetry = true;
                    break;
                case 2:
                    if (millis() - lastRetryTime > 120 * 1000) sendRetry = true;
                    break;
                case 1:
                    if (millis() - lastRetryTime > 120 * 1000) sendRetry = true;
                    break;
                case 0:
                    if (millis() - lastRetryTime > 30 * 1000) {
                        ackRequestNumber = false;
                        outputAckRequestBuffer.erase(outputAckRequestBuffer.begin());
                        if (winlinkStatus > 0 && winlinkStatus < 5) {   // if not complete Winlink Challenge Process it will reset Login process
                            winlinkStatus = 0;
                        }                     
                    }
                    break;
            }
            if (sendRetry) {
                String rest = outputAckRequestBuffer[0].substring(outputAckRequestBuffer[0].indexOf(",") + 1);
                ackCallsignRequest = rest.substring(0, rest.indexOf(","));
                String payload = rest.substring(rest.indexOf(",") + 1);
                ackNumberRequest = payload.substring(payload.indexOf("{") + 1);                
                sendMessage(ackCallsignRequest, payload);
                lastTxTime = millis();
                lastRetryTime = millis();
                outputAckRequestBuffer[0] = String(triesLeft.toInt() - 1) + "," + ackCallsignRequest + "," + payload;
            }
        }
    }

    void cleanOutputAckRequestBuffer(const String& station) {
        if (!outputAckRequestBuffer.empty()) {
            for (int i = outputAckRequestBuffer.size() - 1; i >= 0; i--) {
                if (outputAckRequestBuffer[i].indexOf(station) == 0) outputAckRequestBuffer.erase(outputAckRequestBuffer.begin() + i);
            }
        }
    }

    void clean15SegBuffer() {
        if (!packet15SegBuffer.empty() && (millis() - packet15SegBuffer[0].receivedTime) >  15 * 1000) packet15SegBuffer.erase(packet15SegBuffer.begin());
    }

    bool check15SegBuffer(const String& station, const String& textMessage) {
        if (!packet15SegBuffer.empty()) {
            for (int i = 0; i < packet15SegBuffer.size(); i++) {
                if (packet15SegBuffer[i].station == station && packet15SegBuffer[i].payload == textMessage) return false;
            }
        }
        Packet15SegBuffer   packet;
        packet.receivedTime = millis();
        packet.station      = station;
        packet.payload      = textMessage;
        packet15SegBuffer.push_back(packet);
        return true;
    }
    
    void checkReceivedMessage(ReceivedLoRaPacket packet) {
        if(packet.text.isEmpty()) {
            return;
        }
        if (packet.text.substring(0,3) == "\x3c\xff\x01") {              // its an APRS packet
            //Serial.println(packet.text); // only for debug
            lastReceivedPacket = APRSPacketLib::processReceivedPacket(packet.text.substring(3),packet.rssi, packet.snr, packet.freqError);
            if (lastReceivedPacket.sender!=currentBeacon->callsign) {

                if (lastReceivedPacket.payload.indexOf("\x3c\xff\x01") != -1) {
                    lastReceivedPacket.payload = lastReceivedPacket.payload.substring(0, lastReceivedPacket.payload.indexOf("\x3c\xff\x01"));
                }

                if (check15SegBuffer(lastReceivedPacket.sender, lastReceivedPacket.payload)) {            

                    if (digipeaterActive && lastReceivedPacket.addressee != currentBeacon->callsign) {
                        String digipeatedPacket = APRSPacketLib::generateDigipeatedPacket(packet.text, currentBeacon->callsign, Config.path);
                        if (digipeatedPacket == "X") {
                            logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "Main", "%s", "Packet won't be Repeated (Missing WIDEn-N)");
                        } else {
                            delay(500);
                            LoRa_Utils::sendNewPacket(digipeatedPacket);
                        }
                    }
                    lastHeardTracker = lastReceivedPacket.sender;

                    if (lastReceivedPacket.type == 1 && lastReceivedPacket.addressee == currentBeacon->callsign) {

                        if (ackRequestState && lastReceivedPacket.payload.indexOf("ack") == 0) {
                            if (ackCallsignRequest == lastReceivedPacket.sender && ackNumberRequest == lastReceivedPacket.payload.substring(lastReceivedPacket.payload.indexOf("ack") + 3)) {
                                outputAckRequestBuffer.erase(outputAckRequestBuffer.begin());
                                ackRequestState = false;
                            }
                        }
                        if (lastReceivedPacket.payload.indexOf("{") >= 0) {
                            MSG_Utils::addToOutputBuffer(0, lastReceivedPacket.sender, "ack" + lastReceivedPacket.payload.substring(lastReceivedPacket.payload.indexOf("{") + 1));
                            lastMsgRxTime = millis();
                            lastReceivedPacket.payload = lastReceivedPacket.payload.substring(0, lastReceivedPacket.payload.indexOf("{"));
                        }

                        if (Config.notification.buzzerActive && Config.notification.messageRxBeep) NOTIFICATION_Utils::messageBeep();
                        
                        if (lastReceivedPacket.payload.indexOf("ping") == 0 || lastReceivedPacket.payload.indexOf("Ping") == 0 || lastReceivedPacket.payload.indexOf("PING") == 0) {
                            lastMsgRxTime = millis();
                            MSG_Utils::addToOutputBuffer(0, lastReceivedPacket.sender, "pong, 73!");
                        }

                        if (lastReceivedPacket.sender == "CA2RXU-15" && lastReceivedPacket.payload.indexOf("WX") == 0) {    // WX = WeatherReport
                            Serial.println("Weather Report Received");
                            const String& wxCleaning     = lastReceivedPacket.payload.substring(lastReceivedPacket.payload.indexOf("WX ") + 3);
                            const String& place          = wxCleaning.substring(0,wxCleaning.indexOf(","));
                            const String& placeCleaning  = wxCleaning.substring(wxCleaning.indexOf(",")+1);
                            const String& summary        = placeCleaning.substring(0,placeCleaning.indexOf(","));
                            const String& sumCleaning    = placeCleaning.substring(placeCleaning.indexOf(",")+2);
                            const String& temperature    = sumCleaning.substring(0,sumCleaning.indexOf("P"));
                            const String& tempCleaning   = sumCleaning.substring(sumCleaning.indexOf("P")+1);
                            const String& pressure       = tempCleaning.substring(0,tempCleaning.indexOf("H"));
                            const String& presCleaning   = tempCleaning.substring(tempCleaning.indexOf("H")+1);
                            const String& humidity       = presCleaning.substring(0,presCleaning.indexOf("W"));
                            const String& humCleaning    = presCleaning.substring(presCleaning.indexOf("W")+1);
                            const String& windSpeed      = humCleaning.substring(0,humCleaning.indexOf(","));
                            const String& windCleaning   = humCleaning.substring(humCleaning.indexOf(",")+1);
                            const String& windDegrees    = windCleaning.substring(windCleaning.indexOf(",")+1,windCleaning.indexOf("\n"));

                            String fifthLineWR = temperature;
                            fifthLineWR += "C  ";
                            fifthLineWR += pressure;
                            fifthLineWR += "hPa  ";
                            fifthLineWR += humidity;
                            fifthLineWR += "%";

                            String sixthLineWR = "(wind ";
                            sixthLineWR += windSpeed;
                            sixthLineWR += "m/s ";
                            sixthLineWR += windDegrees;
                            sixthLineWR += "deg)";

                            displayShow("<WEATHER>", "From --> " + lastReceivedPacket.sender, place, summary, fifthLineWR, sixthLineWR);
                            menuDisplay = 300;
                            menuTime = millis();
                        } else if (lastReceivedPacket.sender == "WLNK-1") {
                            if (winlinkStatus == 0 && !Config.simplifiedTrackerMode) {
                                lastMsgRxTime = millis();
                                if (lastReceivedPacket.payload.indexOf("ack") != 0) {
                                    saveNewMessage(0, lastReceivedPacket.sender, lastReceivedPacket.payload);
                                }                                    
                            } else if (winlinkStatus == 1 && ackNumberRequest == lastReceivedPacket.payload.substring(lastReceivedPacket.payload.indexOf("ack") + 3)) {
                                logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Winlink","---> Waiting Challenge");
                                lastMsgRxTime = millis();
                                winlinkStatus = 2;
                                menuDisplay = 500;
                            } else if ((winlinkStatus >= 1 || winlinkStatus <= 3) &&lastReceivedPacket.payload.indexOf("Login [") == 0) {
                                WINLINK_Utils::processWinlinkChallenge(lastReceivedPacket.payload.substring(lastReceivedPacket.payload.indexOf("[")+1,lastReceivedPacket.payload.indexOf("]")));
                                logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Winlink","---> Challenge Received/Processed/Sent");
                                lastMsgRxTime = millis();
                                winlinkStatus = 3;
                                menuDisplay = 501;
                            } else if (winlinkStatus == 3 && ackNumberRequest == lastReceivedPacket.payload.substring(lastReceivedPacket.payload.indexOf("ack") + 3)) {
                                logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Winlink","---> Challenge Ack Received");
                                lastMsgRxTime = millis();
                                winlinkStatus = 4;
                                menuDisplay = 502;
                            } else if (lastReceivedPacket.payload.indexOf("Login valid for") > 0) {
                                logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Winlink","---> Login Succesfull");
                                lastMsgRxTime = millis();
                                winlinkStatus = 5;
                                displayShow(" WINLINK>", "", " LOGGED !!!!", 2000);
                                cleanOutputAckRequestBuffer("WLNK-1");
                                menuDisplay = 5000;
                            } else if (winlinkStatus == 5 && lastReceivedPacket.payload.indexOf("Log off successful") == 0 ) {
                                logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Winlink","---> Log Out");
                                lastMsgRxTime = millis();
                                displayShow(" WINLINK>", "", "    LOG OUT !!!", 2000);
                                cleanOutputAckRequestBuffer("WLNK-1");
                                lastChallengeTime = 0;
                                winlinkStatus = 0;
                            } else if ((winlinkStatus == 5) && (lastReceivedPacket.payload.indexOf("Log off successful") == -1) && (lastReceivedPacket.payload.indexOf("Login valid") == -1) && (lastReceivedPacket.payload.indexOf("Login [") == -1) && (lastReceivedPacket.payload.indexOf("ack") == -1)) {
                                lastMsgRxTime = millis();
                                displayShow("<WLNK Rx >", "", lastReceivedPacket.payload, 3000);
                                saveNewMessage(1, lastReceivedPacket.sender, lastReceivedPacket.payload);
                            } 
                        } else {
                            if (!Config.simplifiedTrackerMode) {
                                lastMsgRxTime = millis();

                                #ifdef HAS_TFT
                                    displayMessage(lastReceivedPacket.sender,lastReceivedPacket.payload, false, 3000);
                                #else
                                    displayShow("< MSG Rx >", "From --> " + lastReceivedPacket.sender, "", lastReceivedPacket.payload , "", "", 3000);
                                #endif

                                if (lastReceivedPacket.payload.indexOf("ack") != 0) {
                                    saveNewMessage(0, lastReceivedPacket.sender, lastReceivedPacket.payload);
                                }                            
                            }
                        }
                    } else {
                        if ((lastReceivedPacket.type == 0 || lastReceivedPacket.type == 4) && !Config.simplifiedTrackerMode) {
                            GPS_Utils::calculateDistanceCourse(lastReceivedPacket.sender, lastReceivedPacket.latitude, lastReceivedPacket.longitude);
                        }
                        if (Config.notification.buzzerActive && Config.notification.stationBeep && !digipeaterActive) {
                            NOTIFICATION_Utils::stationHeardBeep();
                        }
                    }
                }                
            }
        }   
    }

}