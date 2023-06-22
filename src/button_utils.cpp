#include "button_utils.h"
#include "messages.h"
#include "display.h"
#include "configuration.h"
#include <logger.h>

extern int              menuDisplay;
extern bool             displayState;
extern uint32_t         displayTime;
extern logging::Logger  logger;
extern int              messagesIterator;
extern bool             send_update;
extern int              myBeaconsIndex;
extern bool             statusAfterBootState;
extern bool             displayEcoMode;
extern int              myBeaconsSize;
extern Configuration    Config;


namespace BUTTON_Utils {

void singlePress() {
  if (menuDisplay == 0) {
    if (displayState) {
      send_update = true;
    } else {
      display_toggle(true);
      displayTime = millis();   
      displayState = true;  
    }
  } else if (menuDisplay == 1) {
    messages::loadMessagesFromMemory();
    if (messages::warnNoMessages()) {
      menuDisplay = 1;
    } else {
      menuDisplay = 10;
    }
  } else if (menuDisplay == 2) {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Loop", "%s", "wrl");
    messages::sendMessage("CD2RXU-15","wrl");
  } else if (menuDisplay == 10) {
    messagesIterator++;
    if (messagesIterator == messages::getNumAPRSMessages()) {
      menuDisplay = 1;
      messagesIterator = 0;
    } else {
      menuDisplay = 10;
    }
  } else if (menuDisplay == 20) {
    menuDisplay = 2;
  } else if (menuDisplay == 3) {
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
    if (Config.defaultStatus) {
      statusAfterBootState  = true;
    }
    display_toggle(true);
    displayTime = millis();
    show_display("__INFO____", "", "CHANGING CALLSIGN ...", 1000);
  } else if (menuDisplay == 1) {
    messages::deleteFile();
    show_display("__INFO____", "", "ALL MESSAGES DELETED!", 2000);
    messages::loadNumMessages();
  } else if (menuDisplay == 2) {
    menuDisplay = 20;
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
  } else if (menuDisplay == 1) {
    menuDisplay = 2;
    messagesIterator = 0;
  } else if (menuDisplay == 2) {
    menuDisplay = 3;
  } else if (menuDisplay == 3 || menuDisplay == 20) {
    menuDisplay = 0;
    displayTime = millis();
  } 
}

}