#include <logger.h>
#include "configuration.h"
#include "button_utils.h"
#include "msg_utils.h"
#include "display.h"

extern int              menuDisplay;
extern bool             displayState;
extern uint32_t         displayTime;
extern logging::Logger  logger;
extern int              messagesIterator;
extern bool             sendUpdate;
extern int              myBeaconsIndex;
extern bool             statusState;
extern bool             displayEcoMode;
extern int              myBeaconsSize;
extern Configuration    Config;
extern uint32_t         menuTime;


namespace BUTTON_Utils {

void singlePress() {
  if (menuDisplay == 0) {
    if (displayState) {
      sendUpdate = true;
    } else {
      display_toggle(true);
      displayTime = millis();   
      displayState = true;  
    }
  } else if (menuDisplay == 1) {
    MSG_Utils::loadMessagesFromMemory();
    if (MSG_Utils::warnNoMessages()) {
      menuDisplay = 1;
      menuTime = millis();
    } else {
      menuDisplay = 10;
      menuTime = millis();
    }
  } else if (menuDisplay == 2) {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Loop", "%s", "wrl");
    MSG_Utils::sendMessage("CD2RXU-15","wrl");
    menuTime = millis();
  } else if (menuDisplay == 10) {
    messagesIterator++;
    if (messagesIterator == MSG_Utils::getNumAPRSMessages()) {
      menuDisplay = 1;
      menuTime = millis();
      messagesIterator = 0;
    } else {
      menuDisplay = 10;
      menuTime = millis();
    }
  } else if (menuDisplay == 20) {
    menuDisplay = 2;
    menuTime = millis();
  } else if (menuDisplay == 21) {
    menuDisplay = 2;
    menuTime = millis();
  } if (menuDisplay == 3) {
    show_display("__INFO____", "", "Nothing Yet...", 1500);
  }
}

void longPress() {
  if (menuDisplay == 0) {
    if(myBeaconsIndex >= (myBeaconsSize-1)) {
      myBeaconsIndex = 0;
    } else {
      myBeaconsIndex++;
    }
    statusState  = true;
    display_toggle(true);
    displayTime = millis();
    show_display("__INFO____", "", "CHANGING CALLSIGN ...", 1000);
  } else if (menuDisplay == 1) {
    MSG_Utils::deleteFile();
    show_display("__INFO____", "", "ALL MESSAGES DELETED!", 2000);
    MSG_Utils::loadNumMessages();
  } else if (menuDisplay == 2) {
    menuDisplay = 20;
    menuTime = millis();
  } else if (menuDisplay == 3) {
    if (!displayEcoMode) {
      displayEcoMode = true;
      show_display("__DISPLAY_", "", "   ECO MODE -> ON", 1000);
    } else {
      displayEcoMode = false;
      show_display("__DISPLAY_", "", "   ECO MODE -> OFF", 1000);
    }
  }
}

void doublePress() {
  display_toggle(true);
  if (menuDisplay == 0) {
    menuDisplay = 1;
    menuTime = millis();
  } else if (menuDisplay == 1) {
    menuDisplay = 2;
    menuTime = millis();
    messagesIterator = 0;
  } else if (menuDisplay == 2) {
    menuDisplay = 3;
    menuTime = millis();
  } else if (menuDisplay == 3 || menuDisplay == 20) {
    menuDisplay = 0;
    menuTime = millis();
    displayTime = millis();
  } 
}

}