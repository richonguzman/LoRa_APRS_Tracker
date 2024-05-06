#include <TinyGPS++.h>
#include <SPIFFS.h>
#include "APRSPacketLib.h"
#include "notification_utils.h"
#include "bluetooth_utils.h"
#include "winlink_utils.h"
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
extern std::vector<String>  loadedWLNKMails;
extern std::vector<String>  outputMessagesBuffer;
extern std::vector<String>  outputAckRequestBuffer;

extern Configuration        Config;

extern int                  menuDisplay;
extern uint32_t             menuTime;

extern bool                 messageLed;
extern uint32_t             messageLedTime;

extern bool                 digirepeaterActive;

extern String               ackCallsignRequest;
extern String               ackNumberRequest;

extern int                  ackRequestNumber;   //si
//extern int                  ackNumberSend;
extern String               ackDataExpected;
extern bool                 ackRequestState;
extern uint8_t              winlinkStatus;

extern APRSPacket           lastReceivedPacket;
extern uint32_t             lastMsgRxTime;
extern uint32_t             lastRetryTime;

String  lastMessageSaved      = "";
int     numAPRSMessages       = 0;
int     numWLNKMessages       = 0;
bool    noAPRSMsgWarning      = false;
bool    noWLNKMsgWarning      = false;
String  lastHeardTracker      = "NONE";


namespace MSG_Utils {

    bool warnNoAPRSMessages() {
        return noAPRSMsgWarning;
    }

    bool warnNoWLNKMails() {
        return noWLNKMsgWarning;
    }

    String getLastHeardTracker() {
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
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Number of APRS Messages : %s", String(numAPRSMessages));
    
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
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Number of Winlink Mails : %s", String(numWLNKMessages));
    }

    void loadMessagesFromMemory(String typeOfMessage) {
        File fileToRead;
        if (typeOfMessage == "APRS") {
            noAPRSMsgWarning = false;
            if (numAPRSMessages == 0) {
                noAPRSMsgWarning = true;
            } else {
                loadedAPRSMessages.clear();
                fileToRead = SPIFFS.open("/aprsMessages.txt");
            }
            if (noAPRSMsgWarning) {
                show_display("___INFO___", "", " NO APRS MSG SAVED", 1500);
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
        } else if (typeOfMessage == "WLNK") {
            noWLNKMsgWarning = false;
            if (numWLNKMessages == 0) {
                noWLNKMsgWarning = true;
            } else {
                loadedWLNKMails.clear();
                fileToRead = SPIFFS.open("/winlinkMails.txt");
            }
            if (noWLNKMsgWarning) {
                show_display("___INFO___", "", " NO WLNK MAILS SAVED", 1500);
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
        uint32_t ledTimeDelta = millis() - messageLedTime;
        if (messageLed && ledTimeDelta > 5 * 1000) {
            digitalWrite(Config.notification.ledMessagePin, HIGH);
            messageLedTime = millis();
        }
        uint32_t ledOnDelta = millis() - messageLedTime;
        if (messageLed && ledOnDelta > 1 * 1000) {
            digitalWrite(Config.notification.ledMessagePin, LOW);
        }
        if (!messageLed && digitalRead(Config.notification.ledMessagePin) == HIGH) {
            digitalWrite(Config.notification.ledMessagePin, LOW);
        }
    }

    void deleteFile(String typeOfFile) {
        if(!SPIFFS.begin(true)) {
            Serial.println("An Error has occurred while mounting SPIFFS");
            return;
        }
        if (typeOfFile == "APRS") {
            SPIFFS.remove("/aprsMessages.txt");
        } else if (typeOfFile == "WLNK") {
            SPIFFS.remove("/winlinkMails.txt");
        }    
        if (Config.notification.ledMessage) {
            messageLed = false;
        }
    }

    void saveNewMessage(String typeMessage, String station, String newMessage) {
        if (typeMessage == "APRS" && lastMessageSaved != newMessage) {
            File fileToAppendAPRS = SPIFFS.open("/aprsMessages.txt", FILE_APPEND);
            if(!fileToAppendAPRS) {
                Serial.println("There was an error opening the file for appending");
                return;
            }
            newMessage.trim();
            if(!fileToAppendAPRS.println(station + "," + newMessage)) {
                Serial.println("File append failed");
            }
            lastMessageSaved = newMessage;
            numAPRSMessages++;
            fileToAppendAPRS.close();
            if (Config.notification.ledMessage) {
                messageLed = true;
            }
        } else if (typeMessage == "WLNK" && lastMessageSaved != newMessage) {
            File fileToAppendWLNK = SPIFFS.open("/winlinkMails.txt", FILE_APPEND);
            if(!fileToAppendWLNK) {
                Serial.println("There was an error opening the file for appending");
                return;
            }
            newMessage.trim();
            if(!fileToAppendWLNK.println(newMessage)) {
                Serial.println("File append failed");
            }
            lastMessageSaved = newMessage;
            numWLNKMessages++;
            fileToAppendWLNK.close();
            if (Config.notification.ledMessage) {
                messageLed = true;
            }
        }
    }

    void sendMessage(uint8_t typeOfMessage, String station, String textMessage) {
        String newPacket = APRSPacketLib::generateMessagePacket(currentBeacon->callsign, "APLRT1", Config.path, station, textMessage);
        #if HAS_TFT
        cleanTFT();
        #endif
        /*if (textMessage.indexOf("ack") == 0) {
            if (station != "WLNK-1") {  // don't show Winlink ACK
                show_display("<<ACK Tx>>", 500);
            }
        } else if (station.indexOf("CA2RXU-15") == 0 && textMessage.indexOf("wrl") == 0) {
            show_display("<WEATHER>","", "--- Sending Query ---",  1000);
        } else {
            if (station == "WLNK-1") {
                show_display("WINLINK Tx", "", newPacket, 1000);
            } else {
                show_display("MSG Tx >>", "", newPacket, 1000);
            }
        }
        if (typeOfMessage == 1) {   //forced to send MSG with ack confirmation
            ackNumberSend++;
            newPacket += "{" + String(ackNumberSend);
        }*/

        // MSG_Utils::addToOutputBuffer
        LoRa_Utils::sendNewPacket(newPacket);
    }

    String ackRequestNumberGenerator() {
        ackRequestNumber++;
        if (ackRequestNumber > 999) {
            ackRequestNumber = 1;
        }
        return String(ackRequestNumber);
    }

    void addToOutputBuffer(uint8_t typeOfMessage, String station, String textMessage) {
        if (typeOfMessage == 1) {
            outputMessagesBuffer.push_back(station + "," + textMessage + "{" + ackRequestNumberGenerator());
        } else {
            outputMessagesBuffer.push_back(station + "," + textMessage);
        }
    }

    void processOutputBuffer() {    // todos los mensajes de salida deben llegar a este buffer !!!

        /*  no olvidar revisar que los mensajes no se envien muy pronto despues de gps
            ni los de gps despues de mensajes       
            lastOutputBufferTx ????
            lastMsgRxTime??
        */

        uint32_t lastOutputBufferTx = millis() - lastMsgRxTime;
        if (!outputMessagesBuffer.empty() && lastOutputBufferTx >= 4500) {
            String addressee = outputMessagesBuffer[0].substring(0, outputMessagesBuffer[0].indexOf(","));
            String payload = outputMessagesBuffer[0].substring(outputMessagesBuffer[0].indexOf(",") + 1);
            if (payload.indexOf("{") > 0) {   // message Has ack Request
                outputAckRequestBuffer.push_back("5," + addressee + "," + payload);  // 5 is for ack packets retries
                outputMessagesBuffer.erase(outputMessagesBuffer.begin());
                lastMsgRxTime = millis();   // ??
            } else {     // Normal message without ack Request
                /*unit32-t lastPacketTx = millis() - lastTxTime;
                if (lastPacketTx > 7 * 1000) {            // no enviar un mensaje antes de 7 segundos del ultimo gps.
                }*/
                sendMessage(0, addressee, payload);     //????? cero??
                outputMessagesBuffer.erase(outputMessagesBuffer.begin());
                lastMsgRxTime = millis();      //   ?          
            }
        }
        if (outputAckRequestBuffer.empty()) {
            ackRequestState = false;            /// validar que donde se escuchan packets se revise si recibio X ack para sacarlo de los retrys
        } else if (!outputAckRequestBuffer.empty() && lastOutputBufferTx >= 4500) {
            /*  asegurarse que la creacion del mensaje desde su origen agregue el ackNumber y no en el sendMessage!!! */
            bool sendRetry = false;
            String triesLeftString = outputAckRequestBuffer[0].substring(0 , outputAckRequestBuffer[0].indexOf(","));
            int triesLeft = triesLeftString.toInt();
            switch (triesLeft) {
                case 5:
                    sendRetry = true;
                    break;
                case 4:
                    if (millis() - lastRetryTime > 30 * 1000) sendRetry = true;
                    break;
                case 3:
                    if (millis() - lastRetryTime > 30 * 1000) sendRetry = true;
                    break;
                case 2:
                    if (millis() - lastRetryTime > 90 * 1000) sendRetry = true;
                    break;
                case 1:
                    if (millis() - lastRetryTime > 180 * 1000) sendRetry = true;
                    break;
            }
            if (sendRetry) {
                String rest = outputAckRequestBuffer[0].substring(outputAckRequestBuffer[0].indexOf(",") + 1);
                ackCallsignRequest = rest.substring(0, rest.indexOf(","));
                String payload = rest.substring(rest.indexOf(",") + 1);
                ackNumberRequest = payload.substring(payload.indexOf("{") + 1);
                ackRequestState = true;
                sendMessage(1, ackCallsignRequest, payload);     // cambiar "1" !!!          //????? cero??
                lastRetryTime = millis();
                if (triesLeft == 1) {
                    outputAckRequestBuffer.erase(outputAckRequestBuffer.begin());
                } else {
                    outputAckRequestBuffer[0] = String(triesLeft - 1) + "," + ackCallsignRequest + "," + payload;
                }
            }

        }
        
    }

    void checkReceivedMessage(ReceivedLoRaPacket packet) {
        if(packet.text.isEmpty()) {
            return;
        }
        if (packet.text.substring(0,3) == "\x3c\xff\x01") {              // its an APRS packet
            //Serial.println(packet.text); // only for debug
            lastReceivedPacket = APRSPacketLib::processReceivedPacket(packet.text.substring(3),packet.rssi, packet.snr, packet.freqError);
            if (lastReceivedPacket.sender!=currentBeacon->callsign) {

                // segun bluetoothType y si estan activos?
                if (Config.bluetoothType == 0) {
                    BLE_Utils::sendToPhone(packet.text.substring(3));
                } else {
                    #ifdef HAS_BT_CLASSIC
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
                if (lastReceivedPacket.type == 1 && lastReceivedPacket.addressee == currentBeacon->callsign) {
                    
                    //
                    String ackAnswer = "";
                    if (ackRequestState && lastReceivedPacket.message.indexOf("ack") == 0) {
                        ackAnswer = lastReceivedPacket.message.substring(lastReceivedPacket.message.indexOf("ack") + 3);
                        if (ackCallsignRequest == lastReceivedPacket.sender && ackNumberRequest == ackAnswer) {
                            // lo saco del buffer de ackrequest
                        } 
                    }
                    //

                    if (lastReceivedPacket.message.indexOf("{") >= 0) {
                        String ackMessage = "ack" + lastReceivedPacket.message.substring(lastReceivedPacket.message.indexOf("{") + 1);
                        ackMessage.trim();
                        addToOutputBuffer(0, lastReceivedPacket.sender, ackMessage);
                        lastMsgRxTime = millis();
                        lastReceivedPacket.message = lastReceivedPacket.message.substring(0, lastReceivedPacket.message.indexOf("{"));
                    }
                    if (Config.notification.buzzerActive && Config.notification.messageRxBeep) {
                        NOTIFICATION_Utils::messageBeep();
                    }
                    if (lastReceivedPacket.message.indexOf("ping") == 0 || lastReceivedPacket.message.indexOf("Ping") == 0 || lastReceivedPacket.message.indexOf("PING") == 0) {
                        lastMsgRxTime = millis();
                        addToOutputBuffer(0, lastReceivedPacket.sender, "pong, 73!");
                    }
                    if (lastReceivedPacket.sender == "CA2RXU-15" && lastReceivedPacket.message.indexOf("WX") == 0) {    // WX = WeatherReport
                        Serial.println("Weather Report Received");
                        String wxCleaning     = lastReceivedPacket.message.substring(lastReceivedPacket.message.indexOf("WX ") + 3);
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
                    } else if (lastReceivedPacket.sender == "WLNK-1") {
                        if (winlinkStatus == 0) {
                            if (!Config.simplifiedTrackerMode) {
                                lastMsgRxTime = millis();
                                saveNewMessage("APRS", lastReceivedPacket.sender, lastReceivedPacket.message);
                            }
                        } else if (winlinkStatus == 1 && ackNumberRequest == ackAnswer) {
                            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Winlink","---> Waiting Challenge");
                            lastMsgRxTime = millis();
                            winlinkStatus = 2;
                            menuDisplay = 500;
                        } else if (lastReceivedPacket.message.indexOf("Login [") == 0) {
                            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Winlink","---> Challenge received");
                            WINLINK_Utils::processWinlinkChallenge(lastReceivedPacket.message.substring(lastReceivedPacket.message.indexOf("[")+1,lastReceivedPacket.message.indexOf("]")));
                            // controlar en proceso anterior tirar al outputMessagesBuffer tambien!
                            lastMsgRxTime = millis();
                            winlinkStatus = 3;
                            menuDisplay = 501;
                        } /*                        
                        que pasa si es que se reinicio pero esta logeado en las 2 horas?

                        else if (winlinkStatus == 2 && lastReceivedPacket.message.indexOf("Login [") == -1) {
                            Serial.println("We were already logged to WINLINK!!!!");
                            show_display("_WINLINK_>", "", " LOGGED !!!!", 2000);
                            winlinkStatus = 5;
                            menuDisplay = 5000;
                        } */else if (winlinkStatus == 3 && ackNumberRequest == ackAnswer) {
                            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Winlink","---> Challenge Answer Send"); // edit show_display : Challenge Answer Send!!!!
                            lastMsgRxTime = millis();
                            winlinkStatus = 4;
                            menuDisplay = 502;
                        } else if (lastReceivedPacket.message.indexOf("Login valid for") > 0) {
                            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Winlink","---> Login Succesfull");
                            lastMsgRxTime = millis();
                            winlinkStatus = 5;
                            show_display("_WINLINK_>", "", " LOGGED !!!!", 2000);
                            menuDisplay = 5000;
                        } else if (winlinkStatus == 5 && lastReceivedPacket.message.indexOf("Log off successful") == 0 ) {
                            lastMsgRxTime = millis();
                            show_display("_WINLINK_>", "", "    LOG OUT !!!", 2000);
                            winlinkStatus = 0;
                        } else if ((winlinkStatus == 5) && (lastReceivedPacket.message.indexOf("Log off successful") == -1) && (lastReceivedPacket.message.indexOf("Login valid") == -1) && (lastReceivedPacket.message.indexOf("Login [") == -1) && (lastReceivedPacket.message.indexOf("ack") == -1)) {
                            lastMsgRxTime = millis();
                            show_display("<WLNK Rx >", "", lastReceivedPacket.message , "", 3000);
                            saveNewMessage("WLNK", lastReceivedPacket.sender, lastReceivedPacket.message);
                        } 
                    } else {
                        if (!Config.simplifiedTrackerMode) {
                            lastMsgRxTime = millis();
                            show_display("< MSG Rx >", "From --> " + lastReceivedPacket.sender, "", lastReceivedPacket.message , 3000);
                            saveNewMessage("APRS", lastReceivedPacket.sender, lastReceivedPacket.message);
                        }
                    }
                } else {
                    if ((lastReceivedPacket.type == 0 || lastReceivedPacket.type == 4) && !Config.simplifiedTrackerMode) {
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