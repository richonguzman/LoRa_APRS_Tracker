#include <logger.h>
#include "configuration.h"
#include "station_utils.h"
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
extern uint32_t         statusTime;
extern bool             displayEcoMode;
extern int              myBeaconsSize;
extern Configuration    Config;
extern uint32_t         menuTime;
extern bool             messageLed;
extern int              screenBrightness;


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
      menuDisplay = 2;
      menuTime = millis();
    }  else if (menuDisplay == 10) {
      menuDisplay = 11;
      menuTime = millis();
    } else if (menuDisplay == 100) {
      messagesIterator++;
      if (messagesIterator == MSG_Utils::getNumAPRSMessages()) {
        menuDisplay = 10;
        menuTime = millis();
        messagesIterator = 0;
        if (Config.notification.ledMessage){
          messageLed = false;
        }
      } else {
        menuDisplay = 10;
        menuTime = millis();
        messagesIterator = 0;
      }
    } else if (menuDisplay == 11) {
      menuDisplay = 12;
      menuTime = millis();
    } else if (menuDisplay == 110) { //////
      menuDisplay = 11;
      menuTime = millis();
    } else if (menuDisplay == 12) {
      menuDisplay = 10;
      menuTime = millis();
    } 
    
    else if (menuDisplay == 2) {
      menuDisplay = 3;
      menuTime = millis();
    } else if (menuDisplay == 20) {
      menuDisplay = 21;
      menuTime = millis();
    } else if (menuDisplay == 21) {
      menuDisplay = 20;
      menuTime = millis();
    } else if (menuDisplay == 200) {
      menuDisplay = 201;
      menuTime = millis();
    } else if (menuDisplay == 201) {
      menuDisplay = 200;
      menuTime = millis();
    } 
    
    else if (menuDisplay == 3) {
      menuDisplay = 4;
      menuTime = millis();
    } else if (menuDisplay == 30) {
      menuDisplay = 3;
      menuTime = millis();
    }
    
    else if (menuDisplay == 4) {
      menuDisplay = 5;
      menuTime = millis();
    } else if (menuDisplay == 40) {
      menuDisplay = 4;
      menuTime = millis();
    } 
    
    else if (menuDisplay == 5) {
      menuDisplay = 6;
      menuTime = millis();
    } else if (menuDisplay == 6) {
      menuDisplay = 7;
      menuTime = millis();
    } else if (menuDisplay == 7) {
      menuDisplay = 1;
      menuTime = millis();
    }
  }

  void longPress() {
    if (menuDisplay == 0) {
      if(myBeaconsIndex >= (myBeaconsSize-1)) {
        myBeaconsIndex = 0;
      } else {
        myBeaconsIndex++;
      }
      display_toggle(true);
      displayTime = millis();
      statusState  = true;
      statusTime = millis();
      show_display("__ INFO __", "", "  CHANGING CALLSIGN!", 1000);
      STATION_Utils::saveCallsingIndex(myBeaconsIndex);
    } else if (menuDisplay == 1) {
      menuDisplay = 10;
      menuTime = millis();
    } else if (menuDisplay == 10) {
      MSG_Utils::loadMessagesFromMemory();
      if (MSG_Utils::warnNoMessages()) {
        menuDisplay = 10;
        menuTime = millis();
      } else {
        menuDisplay = 100;
        menuTime = millis();
      }
    } else if (menuDisplay == 11) {
      menuDisplay = 110;
      menuTime = millis();
    } else if (menuDisplay == 12) {
      menuDisplay = 120;
      menuTime = millis();
    } else if (menuDisplay == 120) {
      MSG_Utils::deleteFile();
      show_display("__INFO____", "", "ALL MESSAGES DELETED!", 2000);
      MSG_Utils::loadNumMessages();
      menuDisplay = 12;
    } 
    
    else if (menuDisplay == 2) {
      menuDisplay = 20;
      menuTime = millis();
    } else if (menuDisplay == 20) {
      menuDisplay = 200;
      menuTime = millis();
    } else if (menuDisplay == 21) {
      show_display("_NOTIFIC_", "", "ALL NOTIFICATIONS OFF","STILL IN DEVELOPMENT!", 2000); /////////////////////////
    } else if (menuDisplay == 200) {
      if (!displayEcoMode) {
        displayEcoMode = true;
        show_display("_DISPLAY_", "", "   ECO MODE -> ON", 1000);
      } else {
        displayEcoMode = false;
        show_display("_DISPLAY_", "", "   ECO MODE -> OFF", 1000);
      }
    } else if (menuDisplay == 201) {
      if (screenBrightness ==1) {
        show_display("__SCREEN__", "", "SCREEN BRIGHTNESS MAX", 1000);
        screenBrightness = 255;
      } else {
        show_display("__SCREEN__", "", "SCREEN BRIGHTNESS MIN", 1000);
        screenBrightness = 1;
      }
    }

    else if (menuDisplay == 3) {
      menuDisplay = 30;
      menuTime = millis();
    }

    else if (menuDisplay == 4) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Loop", "%s", "wrl");
      MSG_Utils::sendMessage("CD2RXU-15","wrl");
      menuTime = millis();
    }

    else if (menuDisplay == 5) {
      show_display("__STATUS__", "still on", "development..", 1500);
    }

    else if (menuDisplay == 6) {
      show_display("_WINLINK_", "still on", "development..", 1500);
    }

    else if (menuDisplay == 7) {
      show_display("EMERGENCY", "still on", "development..", 1500);
    }
  }


  void doublePress() {
    display_toggle(true);
    if (menuDisplay == 0) {
      menuDisplay = 1;
      menuTime = millis();
    } else if (menuDisplay > 0) {
      menuDisplay = 0;
      menuTime = millis();
      displayTime = millis();
    }
  }

}