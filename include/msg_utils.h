#ifndef MSG_UTILS_H_
#define MSG_UTILS_H_

#include <Arduino.h>
#include "lora_utils.h"

struct Packet15SegBuffer {
    uint32_t    receivedTime;
    String      station;
    String      payload;
};

namespace MSG_Utils {

    bool    warnNoAPRSMessages();
    bool    warnNoWLNKMails();
    const String getLastHeardTracker();
    int     getNumAPRSMessages();
    int     getNumWLNKMails();
    void    loadNumMessages();
    void    loadMessagesFromMemory(uint8_t typeOfMessage);
    void    ledNotification();
    void    deleteFile(uint8_t typeOfFile);
    void    saveNewMessage(uint8_t typeMessage, const String& station, const String& newMessage);
    void    sendMessage(const String& station, const String& textMessage);
    const String ackRequestNumberGenerator();
    void    addToOutputBuffer(uint8_t typeOfMessage, const String& station, const String& textMessage);
    void    processOutputBuffer();
    void    clean15SegBuffer();
    bool    check15SegBuffer(const String& station, const String& textMessage);
    void    checkReceivedMessage(ReceivedLoRaPacket packetReceived);
    
}

#endif