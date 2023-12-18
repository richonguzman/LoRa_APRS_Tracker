#ifndef MSG_UTILS_H_
#define MSG_UTILS_H_

#include <Arduino.h>

namespace MSG_Utils {

    bool    warnNoMessages();
    String  getLastHeardTracker();
    int     getNumAPRSMessages();
    void    loadNumMessages();
    void    loadMessagesFromMemory();
    void    ledNotification();
    void    deleteFile();
    void    saveNewMessage(String typeMessage, String station, String newMessage);
    void    sendMessage(String station, String textMessage);
    void    checkReceivedMessage(String packetReceived);
    
}

#endif