#ifndef MESSAGES_H_
#define MESSAGES_H_

#include <Arduino.h>

namespace messages {

bool    warnNoMessages();
int     getNumAPRSMessages();
void    loadNumMessages();
void    loadMessagesFromMemory();
void    deleteFile();
void    checkListenedTrackersByTimeAndDelete();
void    sendMessage(String station, String textMessage);
void    checkReceivedMessage(String packetReceived);

String getFirstNearTracker();
String getSecondNearTracker();
String getThirdNearTracker();
String getFourthNearTracker();

String getLastHeardTracker();

/*void handlesNotSendUpdateAndGpsLocUpdateAndSmartBeaconState();
void handleSendUpdateAndGpsLocUpdate();

uint32_t getLastTxTime();
void setTxInterval(uint32_t val);
*/

}
#endif