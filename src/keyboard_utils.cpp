#include <logger.h>
#include <Wire.h>
#include "keyboard_utils.h"
#include "winlink_utils.h"
#include "station_utils.h"
#include "configuration.h"
#include "button_utils.h"
#include "power_utils.h"
#include "msg_utils.h"
#include "display.h"

#define CARDKB_ADDR 0x5F    // CARDKB from m5stack.com

extern Configuration    Config;
extern logging::Logger  logger;
extern bool             sendUpdate;
extern int              menuDisplay;
extern uint32_t         menuTime;
extern int              myBeaconsSize;
extern int              myBeaconsIndex;
extern bool             keyboardConnected;
extern bool             keyDetected;
extern uint32_t         keyboardTime;
extern bool             displayState;
extern uint32_t         displayTime;
extern bool             displayEcoMode;
extern int              screenBrightness;
extern bool             statusState;
extern uint32_t         statusTime;
extern int              messagesIterator;
extern bool             messageLed;
extern String           messageCallsign;
extern String           messageText;
extern bool             flashlight;
extern bool             digirepeaterActive;
extern bool             sosActive;
extern String           winlinkMailNumber;

namespace KEYBOARD_Utils {

  void upArrow() {
    if (menuDisplay >= 1 && menuDisplay <= 6) {
      menuDisplay--;
      if (menuDisplay < 1) {
        menuDisplay = 6;
      }
    } else if (menuDisplay >= 10 && menuDisplay <= 13) {
      menuDisplay--;
      if (menuDisplay < 10) {
        menuDisplay = 13;
      }
    } else if (menuDisplay >= 130 && menuDisplay <= 133) {
      menuDisplay--;
      if (menuDisplay < 130) {
        menuDisplay = 133;
      }
    } 
    
    else if (menuDisplay >= 20 && menuDisplay <= 26) {
      menuDisplay--;
      if (menuDisplay < 20) {
        menuDisplay = 26;
      }
    } else if (menuDisplay >= 210 && menuDisplay <= 211) {
      menuDisplay--;
      if (menuDisplay < 210) {
        menuDisplay = 211;
      }
    } else if (menuDisplay >= 220 && menuDisplay <= 221) {
      menuDisplay--;
      if (menuDisplay < 220) {
        menuDisplay = 221;
      }
    } 
    
    else if (menuDisplay >= 30 && menuDisplay <= 31) {
      menuDisplay--;
      if (menuDisplay < 30) {
        menuDisplay = 31;
      }
    } 
    
    else if (menuDisplay >= 50 && menuDisplay <= 52) {
      menuDisplay--;
      if (menuDisplay < 50) {
        menuDisplay = 52;
      }
    } else if (menuDisplay >= 5000 && menuDisplay <= 5080) {
      menuDisplay = menuDisplay - 10;
      if (menuDisplay < 5000) {
        menuDisplay = 5080;
      }
    } 
    
    else if (menuDisplay >= 60 && menuDisplay <= 62) {
      menuDisplay--;
      if (menuDisplay < 60) {
        menuDisplay = 62;
      }
    }
  }

  void downArrow() {
    if (menuDisplay == 0) {
      if (displayState) {
        sendUpdate = true;
      } else {
        display_toggle(true);
        displayTime = millis();   
        displayState = true;  
      }
    }    
    if (menuDisplay >= 1 && menuDisplay <= 6) {       
      menuDisplay++;
      if (menuDisplay > 6) {
        menuDisplay = 1;
      }
    } 
    else if (menuDisplay >= 10 && menuDisplay <= 13) {
      menuDisplay++;
      if (menuDisplay > 13) {
        menuDisplay = 10;
      }
    } else if (menuDisplay >= 130 && menuDisplay <= 133) {
      menuDisplay++;
      if (menuDisplay > 133) {
        menuDisplay = 130;
      }
    } else if (menuDisplay == 100) {
      messagesIterator++;
      if (messagesIterator == MSG_Utils::getNumAPRSMessages()) {
        menuDisplay = 10;
        messagesIterator = 0;
        if (Config.notification.ledMessage){
          messageLed = false;
        }
      } else {
        menuDisplay = 100;
      }
    } else if (menuDisplay == 110) {
      menuDisplay = 11;
    } 
    
    else if (menuDisplay >= 20 && menuDisplay <= 26) {
      menuDisplay++;
      if (menuDisplay > 26) {
        menuDisplay = 20;
      }
    } else if (menuDisplay >= 210 && menuDisplay <= 211) {
      menuDisplay++;
      if (menuDisplay > 211) {
        menuDisplay = 210;
      } 
    } else if (menuDisplay >= 220 && menuDisplay <= 221) {
      menuDisplay++;
      if (menuDisplay > 221) {
        menuDisplay = 220;
      } 
    }

    else if (menuDisplay >= 30 && menuDisplay <= 31) {
      menuDisplay++;  
      if (menuDisplay > 31) {
        menuDisplay = 30;
      }
    }
    
    else if (menuDisplay == 40) {
      menuDisplay = 4;
    }

    else if (menuDisplay >= 50 && menuDisplay <= 52) {
      menuDisplay++;  
      if (menuDisplay > 52) {
        menuDisplay = 50;
      }
    } else if (menuDisplay >= 5000 && menuDisplay <= 5080) {
      menuDisplay = menuDisplay + 10;
      if (menuDisplay > 5080) {
        menuDisplay = 5000;
      }
    } 

    else if (menuDisplay >= 60 && menuDisplay <= 62) {
      menuDisplay++;
      if (menuDisplay > 62) {
        menuDisplay = 60;
      } 
    }
  }

  void leftArrow() {
    if (menuDisplay >= 1 && menuDisplay <= 6) {
      menuDisplay = 0;
    } else if (menuDisplay==110) {
      messageCallsign = "";
      menuDisplay = 11;
    } else if (menuDisplay==111) {
      messageText = "";
      menuDisplay = 110;
    } else if (menuDisplay==1300 ||  menuDisplay==1310) {
      messageText = "";
      menuDisplay = menuDisplay/10;
    } else if ((menuDisplay>=10 && menuDisplay<=13) || (menuDisplay>=20 && menuDisplay<=29) || (menuDisplay==120) || (menuDisplay>=130 && menuDisplay<=133) || (menuDisplay>=50 && menuDisplay<=52) || (menuDisplay>=200 && menuDisplay<=290) || (menuDisplay>=60 && menuDisplay<=62) || (menuDisplay>=30 && menuDisplay<=31) || (menuDisplay>=300 && menuDisplay<=310) || (menuDisplay==40)) {
      menuDisplay = int(menuDisplay/10);
    } else if (menuDisplay==5000 || menuDisplay== 5010 || menuDisplay == 5020 || menuDisplay==5030 || menuDisplay==5040 || menuDisplay==5050 || menuDisplay==5060 || menuDisplay==5070 || menuDisplay==5080) {
      menuDisplay = 5;
    }


    /*               winlinkMailNumber = "";*/
  }

  void rightArrow() {
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
    } else if ((menuDisplay>=1 && menuDisplay<=3) || (menuDisplay>=11 &&menuDisplay<=13) || (menuDisplay>=20 && menuDisplay<=29) || (menuDisplay>=30 && menuDisplay<=31))  {
      menuDisplay = menuDisplay*10;
    } else if (menuDisplay == 10) {
      MSG_Utils::loadMessagesFromMemory();
      if (MSG_Utils::warnNoMessages()) {
        menuDisplay = 10;
      } else {
        menuDisplay = 100;
      }
    } else if (menuDisplay == 120) {
      MSG_Utils::deleteFile();
      show_display("__INFO____", "", "ALL MESSAGES DELETED!", 2000);
      MSG_Utils::loadNumMessages();
      menuDisplay = 12;
    } else if (menuDisplay == 130) {
      if (keyDetected) {
        menuDisplay = 1300;
      } else {
        show_display(" APRS Thu.", "Sending:", "Happy #APRSThursday", "from LoRa Tracker 73!", 2000);
        MSG_Utils::sendMessage(0, "ANSRVR", "CQ HOTG Happy #APRSThursday from LoRa Tracker 73!");
      }
    } else if (menuDisplay == 131) {
      if (keyDetected) {
        menuDisplay = 1310;
      } else {
        show_display(" APRS Thu.", "Sending:", "Happy #APRSThursday", "from LoRa Tracker 73!", 2000);
        MSG_Utils::sendMessage(0, "APRSPH", "HOTG Happy #APRSThursday from LoRa Tracker 73!");
      }
    } else if (menuDisplay == 132) {
      show_display(" APRS Thu.", "", "   Unsubscribe", "   from APRS Thursday", 2000);
      MSG_Utils::sendMessage(0, "ANSRVR", "U HOTG");
    } else if (menuDisplay == 133) {
      show_display(" APRS Thu.", "", "  Keep Subscribed" ,"  for 12hours more", 2000);
      MSG_Utils::sendMessage(0, "ANSRVR", "K HOTG");
    }
    
    else if (menuDisplay == 210) {
      if (!displayEcoMode) {
        displayEcoMode = true;
        show_display("_DISPLAY__", "", "   ECO MODE -> ON", 1000);
      } else {
        displayEcoMode = false;
        show_display("_DISPLAY__", "", "   ECO MODE -> OFF", 1000);
      }
    } else if (menuDisplay == 211) {
      if (screenBrightness ==1) {
        show_display("_SCREEN___", "", "SCREEN BRIGHTNESS MAX", 1000);
        screenBrightness = 255;   
      } else {
        show_display("_SCREEN___", "", "SCREEN BRIGHTNESS MIN", 1000);
        screenBrightness = 1;
      }
    } else if (menuDisplay == 230) {
      show_display("_STATUS___", "", "WRITE STATUS","STILL IN DEVELOPMENT!", 2000); /////////////////////////
    } else if (menuDisplay == 231) {
      show_display("_STATUS___", "", "SELECT STATUS","STILL IN DEVELOPMENT!", 2000); /////////////////////////
    } else if (menuDisplay == 240) {
      show_display("_NOTIFIC__", "", "NOTIFICATIONS","STILL IN DEVELOPMENT!", 2000); /////////////////////////
    } 

    else if (menuDisplay == 4) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Loop", "%s", "wrl");
      MSG_Utils::sendMessage(0, "CA2RXU-15", "wrl");
    }

    else if (menuDisplay == 5) {
      menuDisplay = 50;
    } else if (menuDisplay == 50) {
      WINLINK_Utils::login();
      menuDisplay = 500;
    } else if (menuDisplay == 51) {
      show_display("_WINLINK_>", "", "READ MSG/MAIL", "", 1000);
    } else if (menuDisplay == 52) {
      show_display("_WINLINK_>", "", "DELETE MSG/MAIL" ,"", 1000);
    }

    else if (menuDisplay==5000) {
      MSG_Utils::sendMessage(1, "WLNK-1", "L");
    } else if (menuDisplay==5010) {
      show_display("_WINLINK_>", "", "DOWNLOADED MAILS" ,"", 1000);
    } else if (menuDisplay==5020) {
      menuDisplay = 5021;
    } else if (menuDisplay==5030) {
      show_display("_WINLINK_>", "", "REPLY MAIL" ,"", 1000);
    } else if (menuDisplay==5040) {
      show_display("_WINLINK_>", "", "FORWARD MAIL" ,"", 1000);
    } else if (menuDisplay==5050) {
      menuDisplay = 5051;
    } else if (menuDisplay==5060) {
      show_display("_WINLINK_>", "", "ALIAS MENU" ,"", 1000);
    } else if (menuDisplay==5070) {
      MSG_Utils::sendMessage(1, "WLNK-1", "B");
      menuDisplay = 5;
    } else if (menuDisplay==5080) {
      show_display("_WINLINK_>", "", "WRITE MAIL" ,"", 1000);
    } 

    else if (menuDisplay == 6) {
      menuDisplay = 60;
    } else if (menuDisplay == 60) {
      if (Config.notification.ledFlashlight) {
        if (flashlight) {
          show_display("__EXTRAS__", "","     Flashlight","   Status --> OFF","", 2000);
          flashlight = false;
        } else {
          show_display("__EXTRAS__", "","     Flashlight","   Status --> ON","", 2000);
          flashlight = true;
        }
      } else {
        show_display("__EXTRAS__", "","     Flashlight","NOT ACTIVE IN CONFIG!","", 2000);
      }
    } else if (menuDisplay == 61) {
      if (digirepeaterActive) {
        show_display("__EXTRAS__", "","   DigiRepeater","   Status --> OFF","", 2000);
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "%s", "DigiRepeater OFF");
        digirepeaterActive = false;
      } else {
        show_display("__EXTRAS__", "","   DigiRepeater","   Status --> ON","", 2000);
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "%s", "DigiRepeater ON");
        digirepeaterActive = true;
      }
    } else if (menuDisplay == 62) {
      if (sosActive) {
        show_display("__EXTRAS__", "","       S.O.S.","   Status --> OFF","", 2000);
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "%s", "S.O.S Mode OFF");
        sosActive = false;
      } else {
        show_display("__EXTRAS__", "","       S.O.S.","   Status --> ON","", 2000);
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "%s", "S.O.S Mode ON");
        sosActive = true;
      }
    }    
  }

  void processPressedKey(char key) {
    keyDetected = true;
    menuTime = millis();
    /*  181 -> up / 182 -> down / 180 <- back / 183 -> forward / 8 Delete / 13 Enter / 32 Space  / 27 Esc */
    if (!displayState) {
        display_toggle(true);
        displayTime = millis();   
        displayState = true;
    }
    if (menuDisplay == 0 && key == 33) {              // Main Menu
      menuDisplay = 1;
    } else if (key == 27) {                           // ESC = return to Main Menu
      menuDisplay = 0;
      messagesIterator = 0;
      messageCallsign = "";
      messageText = "";
    } else if (menuDisplay >= 1 && menuDisplay <= 6 && key >=49 && key <= 55) { // Menu number select
      menuDisplay = key - 48;
    } else if (menuDisplay == 110 && key != 180) {    // Writing Callsign of Message
      if (messageCallsign.length() == 1) {
        messageCallsign.trim();
      }
      if ((key >= 48 && key <= 57) || (key >= 65 && key <= 90) || (key >= 97 && key <= 122) || key == 45) { //only letters + numbers + "-"
        messageCallsign += key;
      } else if (key == 13) {                         // Return Pressed
        messageCallsign.trim();
        if (menuDisplay == 110) {
          menuDisplay = 111;
        }
      } else if (key == 8) {                          // Delete Last Key
        messageCallsign = messageCallsign.substring(0, messageCallsign.length()-1);
      }
      messageCallsign.toUpperCase();
    } else if ((menuDisplay==111 || menuDisplay==1300 || menuDisplay==1310) && key!= 180) {     // Writting Text of Message
      if (messageText.length() == 1) {
        messageText.trim();
      }
      if (key >= 32 && key <= 126) {
        messageText += key;
      } else if (key == 13) {                         // Return Pressed: SENDING MESSAGE
        messageText.trim();
        if (messageText.length() > 67){
          messageText = messageText.substring(0,67);
        }
        if (menuDisplay==111) {
          MSG_Utils::sendMessage(0, messageCallsign, messageText);
          menuDisplay = 11;
        } else if (menuDisplay==1300) {
          messageCallsign = "ANSRVR";
          MSG_Utils::sendMessage(0, messageCallsign, "CQ HOTG " + messageText);
          menuDisplay = 130;
        } else if (menuDisplay==1310) {
          messageCallsign = "APRSPH";
          MSG_Utils::sendMessage(0, messageCallsign, "HOTG " + messageText);
          menuDisplay = 131;
        }
        messageCallsign = "";
        messageText = "";
      } else if (key == 8) {                          // Delete Last Key
        messageText = messageText.substring(0, messageText.length()-1);
      }
    } else if ((menuDisplay== 5021 || menuDisplay==5051) && key >= 48 && key <= 57) { // numeros exactos???
      winlinkMailNumber = key;
    } else if ((menuDisplay== 5021 || menuDisplay==5051) && key == 8) { // numeros exactos???
      winlinkMailNumber = "_?";
    } else if (menuDisplay == 5021 && key == 13 && winlinkMailNumber!="?") {
      Serial.println("1, WLNK-1, R " + winlinkMailNumber);
      //MSG_Utils::sendMessage(1, "WLNK-1", "R" + winlinkMailNumber);
      winlinkMailNumber = "_?";
      menuDisplay = 5020;
    } else if (menuDisplay == 5051 && key == 13 && winlinkMailNumber!="?") {
      Serial.println("1, WLNK-1, K " + winlinkMailNumber);
      //MSG_Utils::sendMessage(1, "WLNK-1", "K" + winlinkMailNumber);
      winlinkMailNumber = "_?";
      menuDisplay = 5050;
    } /*else if (menuDisplay == 5051 && key == 13) {
      Serial.println("1, WLNK-1, Y " + winlinkMailNumber);
      //MSG_Utils::sendMessage(1, "WLNK-1", "Y" + winlinkMailNumber);
      winlinkMailNumber = "";
      menuDisplay = 5030;


    }*/ else if (key==13) {
      if (menuDisplay==200) {
        if(myBeaconsIndex >= (myBeaconsSize - 1)) {
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
        menuDisplay = 0;
      } else if (menuDisplay==250) {
        show_display("", "", "    REBOOTING ...", 2000);
        ESP.restart();
      } else if (menuDisplay==260) {
        show_display("", "", "    POWER OFF ...", 2000);
        POWER_Utils::shutdown();
      }
    }
    else if (key == 181) {  // Arrow Up
      upArrow();
    }
    else if (key == 182) {  // Arrow Down
      downArrow();
    }
    else if (key == 180) {  // Arrow Left
      leftArrow();
    }
    else if (key == 183) {  // Arrow Right
      rightArrow();
    }
  }
      
  void read() {
    uint32_t lastKey = millis() - keyboardTime;
    if (lastKey > 30*1000) {
      keyDetected = false;
    }
    Wire.requestFrom(CARDKB_ADDR, 1);
    while(Wire.available()) {
      char c = Wire.read();
      if (c != 0) {

        // just for debugging
        //Serial.print(c, DEC); Serial.print(" "); Serial.print(c, HEX); Serial.print(" "); Serial.println(char(c));

        keyboardTime = millis();
        processPressedKey(c);      
      }
    }
  }

  void setup() {
    Wire.beginTransmission(CARDKB_ADDR);
    if (Wire.endTransmission() == 0) {
      keyboardConnected = true;
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Keyboard Connected to I2C");
    } else {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "No Keyboard Connected to I2C");
    }
  }

}