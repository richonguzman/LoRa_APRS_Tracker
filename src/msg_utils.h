#ifndef MSG_UTILS_H_
#define MSG_UTILS_H_

#include <Arduino.h>
#include "lora_utils.h"

namespace MSG_Utils {

    bool    warnNoMessages();
    String  getLastHeardTracker();
    int     getNumAPRSMessages();
    void    loadNumMessages();
    void    loadMessagesFromMemory();
    void    ledNotification();
    void    deleteFile();
    void    saveNewMessage(const String& typeMessage, const String& station, String newMessage);
    void    sendMessage(const String& station, const String& textMessage);
    void    checkReceivedMessage(ReceivedLoRaPacket packetReceived);

}

#endif