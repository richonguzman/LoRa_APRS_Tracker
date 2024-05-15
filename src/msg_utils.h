#ifndef MSG_UTILS_H_
#define MSG_UTILS_H_

#include <Arduino.h>
#include "lora_utils.h"

namespace MSG_Utils {

    bool    warnNoAPRSMessages();
    bool    warnNoWLNKMails();
    String  getLastHeardTracker();
    int     getNumAPRSMessages();
    int     getNumWLNKMails();
    void    loadNumMessages();
    void    loadMessagesFromMemory(uint8_t typeOfMessage);
    void    ledNotification();
    void    deleteFile(uint8_t typeOfFile);
    void    saveNewMessage(uint8_t typeMessage, const String& station, const String& newMessage);
    void    sendMessage(const String& station, const String& textMessage);
    String  ackRequestNumberGenerator();
    void    addToOutputBuffer(uint8_t typeOfMessage, const String& station, const String& textMessage);
    void    processOutputBuffer();
    void    clean25SegBuffer();
    bool    check25SegBuffer(const String& station, const String& textMessage);
    void    checkReceivedMessage(ReceivedLoRaPacket packetReceived);
    
}

#endif