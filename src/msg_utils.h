#ifndef MSG_UTILS_H_
#define MSG_UTILS_H_

#include <Arduino.h>

namespace MSG_Utils {

bool    warnNoMessages();
int     getNumAPRSMessages();
void    loadNumMessages();
void    loadMessagesFromMemory();
void    ledNotification();
void    deleteFile();
void    sendMessage(String station, String textMessage);
void    checkReceivedMessage(String packetReceived);

String getLastHeardTracker();

}

#endif