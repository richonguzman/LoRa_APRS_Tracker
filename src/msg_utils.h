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
    void    loadMessagesFromMemory(String typeOfMessage);
    void    ledNotification();
    void    deleteFile(String typeOfFile);
    void    saveNewMessage(String typeMessage, String station, String newMessage);
    void    sendMessage(int typeOfMessage, String station, String textMessage);
    void    processOutputBuffer();
    void    checkReceivedMessage(ReceivedLoRaPacket packetReceived);
    
}

#endif