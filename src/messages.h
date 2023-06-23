#ifndef MESSAGES_H_
#define MESSAGES_H_

#include <Arduino.h>

namespace messages {

bool    warnNoMessages();
int     getNumAPRSMessages();
void    loadNumMessages();
void    loadMessagesFromMemory();
void    deleteFile();
//void    checkListenedTrackersByTimeAndDelete();
void    sendMessage(String station, String textMessage);
void    checkReceivedMessage(String packetReceived);

String getLastHeardTracker();

}
#endif