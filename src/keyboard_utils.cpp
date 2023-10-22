#include "keyboard_utils.h"
#include "button_utils.h"
#include "display.h"
#include <Wire.h>
#include <logger.h>
#include "msg_utils.h"
#include "station_utils.h"
#include "configuration.h"

#define CARDKB_ADDR 0x5F    // yes , this is for CARDKB from m5stack.com

extern Configuration    Config;
extern logging::Logger  logger;
extern bool             sendUpdate;
extern int              menuDisplay;
extern uint32_t         menuTime;
extern int              myBeaconsSize;
extern int              myBeaconsIndex;
extern bool             keyboardDetected;
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

namespace KEYBOARD_Utils {

  void upArrow() {
    if (menuDisplay >= 1 && menuDisplay <= 7) {
      menuDisplay--;
      if (menuDisplay == 0) {
        menuDisplay = 7;
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
    if (menuDisplay >= 1 && menuDisplay <= 7) {       
      menuDisplay++;
      menuTime = millis();
      if (menuDisplay == 8) {
        menuDisplay = 1;
      }
    } 
    else if (menuDisplay >= 10 && menuDisplay <= 12) {
      menuDisplay++;
      menuTime = millis();
      if (menuDisplay == 13) {
        menuDisplay = 10;
      }
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
        menuDisplay = 100;
        menuTime = millis();
      }
    } else if (menuDisplay == 110) { //////
      menuDisplay = 11;
      menuTime = millis();
    } 
    
    else if (menuDisplay >= 20 && menuDisplay <= 21) {
      menuDisplay++;
      menuTime = millis();
      if (menuDisplay == 22) {
        menuDisplay = 20;
      }
    } else if (menuDisplay >= 200 && menuDisplay <= 201) {
      menuDisplay++;
      menuTime = millis();
      if (menuDisplay == 202) {
        menuDisplay = 200;
      } 
    }
    
    else if (menuDisplay == 30) {
      menuDisplay = 3;
      menuTime = millis();
    }
    
    else if (menuDisplay == 40) {
      menuDisplay = 4;
      menuTime = millis();
    }
  }

  void leftArrow() {
    if (menuDisplay >= 1 && menuDisplay <= 7) {               // Return to Main Menu
      menuDisplay = 0;
    } else if (menuDisplay >= 10 && menuDisplay <= 12) {      // Return to Menu : Messages
      menuDisplay = 1;
    } else if (menuDisplay==110) {
      messageCallsign = "";
      menuDisplay = 11;
    } else if (menuDisplay==111) {
      messageText = "";
      menuDisplay = 110;
    }

    else if (menuDisplay >= 20 && menuDisplay <= 21) {        // Return to Menu : Configuration
      menuDisplay = 2;
    } else if (menuDisplay >= 200 && menuDisplay <= 201) {
      menuDisplay = 20;
    }
    
    else if (menuDisplay == 30) {                             // Return to Menu : Stations
      menuDisplay = 3;
    }

    else if (menuDisplay == 40) {                             // Return to Menu : Weather
      menuDisplay = 4;
    }   
    /*        messageCallsign = "";
              messageText = "";
              winlinkMailNumber = "";*/
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
      if (screenBrightness <=1) {
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

  void processPressedKey(char key) {
    keyboardDetected = true;
    menuTime = millis();
    /*  181 -> up / 182 -> down / 180 <- back / 183 -> forward / 8 Delete / 13 Enter / 32 Space  / 27 Esc */

    if (!displayState) {
        display_toggle(true);
        displayTime = millis();   
        displayState = true;
    }

    if (menuDisplay == 0 && key == 33) {                                  // Main Menu
      menuDisplay = 1;
    } else if (key == 27) {     // ESC = volver a menu base
      menuDisplay = 0;
      messagesIterator = 0;
      messageCallsign = "";
      messageText = "";
    } else if (menuDisplay >= 1 && menuDisplay <= 7 && key >=49 && key <= 55) { // Menu number select
      menuDisplay = key - 48;
    } else if (menuDisplay == 110 && key != 180) {        // Writing Callsign of Message
      if (messageCallsign.length() == 1) {
        messageCallsign.trim();
      }
      if ((key >= 48 && key <= 57) || (key >= 65 && key <= 90) || (key >= 97 && key <= 122) || (key == 45)) { //only letters + numbers + "-"
        messageCallsign += key;
      } else if (key == 13) {     // Return Pressed
        messageCallsign.trim();
        if (menuDisplay == 110) {
          menuDisplay = 111;
        }
      } else if (key == 8) {      // Delete Last Key
        messageCallsign = messageCallsign.substring(0, messageCallsign.length()-1);
      }
      messageCallsign.toUpperCase();
      Serial.println(messageCallsign);
    } else if (menuDisplay == 111 && key!= 180) {        // Writting Text of Message
      if (messageText.length() == 1) {
        messageText.trim();
      }
      if (key >= 32 && key <= 126) {
        messageText += key;
      } else if (key == 13) {     // Return Pressed: SENDING MESSAGE
        messageText.trim();
        if (messageText.length() > 67){
          messageText = messageText.substring(0,67);
        }
        MSG_Utils::sendMessage(messageCallsign, messageText);
        menuDisplay = 11;
        messageCallsign = "";
        messageText = "";
      } else if (key == 8) {      // Delete Last Key
        messageText = messageText.substring(0, messageText.length()-1);
      }
    }
    
    
    
    
    
    
    
    
    
    
    
    /*else if (menuDisplay == 0 && key == 37) {
      handleNextGpsLocation(myGpsLocationsSize);
    } else if (menuDisplay == 0 && key == 41) {
      if (showCompass == false) {
        showCompass = true;
      } else {
        showCompass = false;
      }
    }

     else if (key == 8) {      // Delete Last Key
        messageText = messageText.substring(0, messageText.length()-1);
      }
      Serial.println(messageText);
    } else if (menuDisplay == 1200 && key == 13) {
      deleteFile(aprsMsgFilePath);
      deleteFile(loraMsgFilePath);
      show_display("_DELETING_", "", "ALL MESSAGES DELETED", 1500);
      loadNumMessages();
      menuDisplay = 1;
    } else if (menuDisplay == 1210 && key == 13) {
      deleteFile(aprsMsgFilePath);
      show_display("_DELETING_", "", "ALL APRS MESSAGES", "DELETED!", 1500);
      loadNumMessages();
      menuDisplay = 1;
    } else if (menuDisplay == 1220 && key == 13) {
      deleteFile(loraMsgFilePath);
      show_display("_DELETING_", "", "ALL LoRa DM DELETED", 1500);
      loadNumMessages();
      menuDisplay = 1;
    } else if (menuDisplay == 400 && key == 13) {  // start/stop saving waypoints
      if (recordingWaypoints) {
        recordingWaypoints = false;
        send_update = true;
      } else {
        recordingWaypoints = true;
        send_update = true;
      }
      menuDisplay = 40;
    } else if (menuDisplay == 420 && key == 13) {
      deleteFile(gpsMyWaypointsFilePath);
      menuDisplay = 42;
    } else if (menuDisplay == 530) {
      if (weatherOtherPlace.length() == 1) {
        weatherOtherPlace.trim();
      }
      if ((key >= 65 && key <= 90) || (key >= 97 && key <= 122) || (key == 32)) {
        weatherOtherPlace += key;
      } else if (key == 13) {     // Return Pressed: SENDING MESSAGE
        weatherOtherPlace.trim();
        sendMessage("APRS", "WRCLP", "wl " + weatherOtherPlace);
        weatherOtherPlace = "";
        menuDisplay = 53;
      } else if (key == 8) {      // Delete Last Key
        weatherOtherPlace = weatherOtherPlace.substring(0, weatherOtherPlace.length()-1);
      }
      Serial.println(weatherOtherPlace);
    } else if (menuDisplay == 700 && key != 180) {
      if (messageText.length() == 1) {
        messageText.trim();
      }
      if (key >= 32 && key <= 126) {
        messageText += key;
      } else if (key == 13) {     // Return Pressed: SENDING MESSAGE
        messageText.trim();
        Serial.println(messageText.length());
        if (messageText.length() > 58){
          Serial.println(messageText.length());
          messageText = messageText.substring(0,58);
        }
        sendMessage("APRS", "ANSRVR", "CQ HOTG " + messageText);
        menuDisplay = 70;
        messageText = "";
      } else if (key == 8) {      // Delete Last Key
        messageText = messageText.substring(0, messageText.length()-1);
      }
    } else if (menuDisplay >= 8020 && menuDisplay <= 8050 && key >= 48 && key <= 57) {
      winlinkMailNumber = key;
    } else if (menuDisplay >= 8020 && menuDisplay <= 8050 && key == 8) {
      winlinkMailNumber = "";
    } else if (menuDisplay == 8020 && key == 13) {
      show_display("__WNLK_Tx>", "", "  READ MAIL N." + winlinkMailNumber, 1500);
      sendMessage("WLNK", "WLNK-1", "R" + winlinkMailNumber);
      winlinkMailNumber = "";
      menuDisplay = int(menuDisplay/10);
    } else if (menuDisplay == 8030 && key == 13) {
      show_display("__WNLK_Tx>", "", "  REPLY MAIL N." + winlinkMailNumber, 1500);
      sendMessage("WLNK", "WLNK-1", "Y" + winlinkMailNumber);
      winlinkMailNumber = "";
      menuDisplay = 8082;
    } else if (menuDisplay == 8040 && key == 13) {
      menuDisplay = 80400;
    } else if (menuDisplay == 80400) {
      if (winlinkAddressee.length() == 1) {
        winlinkAddressee.trim();
      }
      if ((key >= 65 && key <=90) || (key >= 97 && key <= 122) || (key >= 48 && key <= 57) || (key == 45) || (key == 46) || (key == 64) || (key == 95)) {
        winlinkAddressee += key;
      } else if (key == 13 && winlinkAddressee.length() > 0) {
        winlinkAddressee.trim();
        show_display("__WNLK_Tx>", "", "FORWARD MAIL N." + winlinkMailNumber, "TO = " + winlinkAddressee, 1500);
        sendMessage("WLNK", "WLNK-1", "F" + winlinkMailNumber + " " + winlinkAddressee);
        winlinkMailNumber = "";
        winlinkAddressee = "";
        menuDisplay = 804;
      } else if (key == 8) {
        winlinkAddressee = winlinkAddressee.substring(0, winlinkAddressee.length()-1);
      } else if (key == 180) { 
        menuDisplay = 8040;
        winlinkAddressee = "";
      }
    } else if (menuDisplay == 8050 && key == 13) {
      show_display("__WNLK_Tx>", "", "  DELETE MAIL N." + winlinkMailNumber, 1500);
      sendMessage("WLNK", "WLNK-1", "K" + winlinkMailNumber);
      winlinkMailNumber = "";
      menuDisplay = 805;
    } else if (menuDisplay == 80600) {
      if (winlinkAlias.length() == 1) {
        winlinkAlias.trim();
      }
      if ((key >= 65 && key <=90) || (key >= 97 && key <= 122) || (key >= 48 && key <= 57)) {
        winlinkAlias += key;
      } else if (key == 13) {
        winlinkAlias.trim();
        menuDisplay = 80601;
      } else if (key == 8) {
        winlinkAlias = winlinkAlias.substring(0, winlinkAlias.length()-1);
      } else if (key == 180) { 
        menuDisplay = 8060;
        winlinkAlias = "";
      }
    } else if (menuDisplay == 80601) {
      if (winlinkAliasComplete.length() == 1) {
        winlinkAliasComplete.trim();
      }
      if ((key >= 65 && key <=90) || (key >= 97 && key <= 122) || (key >= 48 && key <= 57) || (key == 45) || (key == 46) || (key == 64) || (key == 95)) {
        winlinkAliasComplete += key;
      } else if (key == 13) {
        winlinkAliasComplete.trim();
        show_display("__WNLK_Tx>", "", "CREATE ALIAS -> ", winlinkAlias + " = " + winlinkAliasComplete, 3000);
        sendMessage("WLNK", "WLNK-1", "A " + winlinkAlias + "=" + winlinkAliasComplete);
        winlinkAlias = "";
        winlinkAliasComplete = "";
        menuDisplay = 8060;
      } else if (key == 8) {
        winlinkAliasComplete = winlinkAliasComplete.substring(0, winlinkAliasComplete.length()-1);
      } else if (key == 180) { 
        menuDisplay = 80600;
        winlinkAliasComplete = "";
      }
    } else if (menuDisplay == 80610) {
      if (winlinkAlias.length() == 1) {
        winlinkAlias.trim();
      }
      if ((key >= 65 && key <=90) || (key >= 97 && key <= 122) || (key >= 48 && key <= 57)) {
        winlinkAlias += key;
      } else if (key == 13) {
        winlinkAlias.trim();
        show_display("__WNLK_Tx>", "", "DELETE ALIAS-> " + winlinkAlias, 2000);
        sendMessage("WLNK", "WLNK-1", "A " + winlinkAlias + "=");
        winlinkAlias = "";
        menuDisplay = 8061;
      } else if (key == 8) {
        winlinkAlias = winlinkAlias.substring(0, winlinkAlias.length()-1);
      } else if (key == 180) { 
        menuDisplay = 8061;
        winlinkAlias = "";
      }
    } else if (menuDisplay == 8080) {
      if (winlinkAddressee.length() == 1) {
        winlinkAddressee.trim();
      }
      if ((key >= 65 && key <=90) || (key >= 97 && key <= 122) || (key >= 48 && key <= 57) || (key == 45) || (key == 46) || (key == 64) || (key == 95)) {
        winlinkAddressee += key;
      } else if (key == 13 && winlinkAddressee.length() > 0) {
        winlinkAddressee.trim();
        menuDisplay = 8081;
      } else if (key == 8) {
        winlinkAddressee = winlinkAddressee.substring(0, winlinkAddressee.length()-1);
      } else if (key == 180) { 
        menuDisplay = 808;
        winlinkAddressee = "";
      }
    } else if (menuDisplay == 8081) {
      if (winlinkSubject.length() == 1) {
        winlinkSubject.trim();
      }
      if ((key >= 65 && key <=90) || (key >= 97 && key <= 122) || (key == 32) || (key >= 48 && key <= 57)) {
        winlinkSubject += key;
      } else if (key == 13 && winlinkSubject.length() > 0) {
        winlinkSubject.trim();
        show_display("__WNLK_Tx>", "", "Mail -> " + winlinkAddressee, "Subj -> " + winlinkSubject, 2000);
        sendMessage("WLNK", "WLNK-1", "SP " + winlinkAddressee + " " + winlinkSubject);
        menuDisplay = 8082;
      } else if (key == 8) {
        winlinkSubject = winlinkSubject.substring(0, winlinkSubject.length()-1);
      } else if (key == 180) { 
        menuDisplay = 8080;
        winlinkSubject = "";
      }
    } else if (menuDisplay == 8082) {
      if (winlinkBody.length() == 1) {
        winlinkBody.trim();
      }
      if ((key >= 32 && key <=122)) {
        winlinkBody += key;
      } else if (key == 13 && winlinkBody.length() <= 67) {
        winlinkBody.trim();
        show_display("__WNLK_Tx>", "", "Body -> " + winlinkBody, 2000);
        sendMessage("WLNK", "WLNK-1", winlinkBody);
        menuDisplay = 8083;
      } else if (key == 8) {
        winlinkBody = winlinkBody.substring(0, winlinkBody.length()-1);
      } else if (key == 180) { 
        winlinkBody = "";
      }
    }*/

    else if (key == 181) {                        // Arrow Up
      upArrow();
    }

    else if (key == 182) {                        // Arrow Down
      downArrow();      
    } 

    else if (key == 180) {                        // Arrow Left
      leftArrow();
    }

    else if (key == 183) {                        // Arrow Right
      rightArrow();
      

      /*   
      else if (menuDisplay == 11) { 
        menuDisplay = 110;     // Menu : Messages WRITE
      } else if (menuDisplay == 12) { 
        menuDisplay = 120;     // Menu : Messages DELETE
      }

      else if (menuDisplay == 111) {
        //Serial.println("--> Messages : WRITE : DIRECT MSG");
        menuDisplay = 1110;
      } else if (menuDisplay == 120) {
        //Serial.println("--> Messages : DELETE : ALL");
        menuDisplay = 1200;
      } else if (menuDisplay == 121) {
        //Serial.println("--> Messages : DELETE : (ONLY) ALL APRS MSG");
        menuDisplay = 1210;
      } else if (menuDisplay == 122) {
        //Serial.println("--> Messages : DELETE : (ONLY) ALL DIRECT MSG");
        menuDisplay = 1220;
      }

      else if (menuDisplay == 2) {    // Status Menu
        menuDisplay = 20;
      } else if (menuDisplay == 20) {
        Serial.println("--> Status Write");
      } else if (menuDisplay == 21) { //Status Select from Favourites
        menuDisplay = 210;
      } else if (menuDisplay == 210) {
        sendStatus(StatusAvailable[0]);
        menuDisplay = 21;
      } else if (menuDisplay == 211) {
        sendStatus(StatusAvailable[1]);
        menuDisplay = 21;
      } else if (menuDisplay == 212) {
        sendStatus(StatusAvailable[2]);
        menuDisplay = 21;
      } else if (menuDisplay == 213) {
        sendStatus(StatusAvailable[3]);
        menuDisplay = 21;
      } else if (menuDisplay == 214) {
        sendStatus(StatusAvailable[4]);
        menuDisplay = 21;
      }

      else if (menuDisplay == 3) {  // Stations Menu
        menuDisplay = 30;
      } else if (menuDisplay == 30) {
        menuDisplay = 300;
      } else if (menuDisplay == 31) {
        menuDisplay = 310;
      } else if (menuDisplay == 300) {
        menuDisplay = 3000;
      } else if (menuDisplay == 301) {
        Serial.println("--> Menu : 3.Stations: Lora Trackers: WHOISTHERE");
      } else if (menuDisplay == 302) {
        Serial.println("--> Menu : 3.Stations: Lora Trackers: RUTHERE Callsign");
      } else if (menuDisplay == 310) {
        Serial.println("--> Menu : 3.Stations: Lora iGates: WHICHIGATE?");
      } else if (menuDisplay == 311) {
        Serial.println("--> Menu : 3.Stations: Lora iGates: RUTHERE iGate?");
      } else if (menuDisplay == 312) {
        Serial.println("--> Menu : 3.Stations: Lora iGates: Latest TRACKERS/Stations iGate?");
      } else if (menuDisplay == 313) {
        Serial.println("--> Menu : 3.Stations: Lora iGates: DidYouHear Callsign?");
      } 

      else if (menuDisplay == 4) {  // Waypoints Menu
        menuDisplay = 40;
      } else if (menuDisplay == 40) { // Menu Waypoints: Save Waypoints
        //Serial.println("Start Saving Waypoint");
        menuDisplay = 400;
      } else if (menuDisplay == 41) { // Menu Waypoints: Follow Route
        if (recordingWaypoints) {
          menuDisplay = 4100;
        } else {
          Serial.println("Follow Route");
        }//menuDisplay = 410;
      } else if (menuDisplay == 42) { // Menu Waypoints: Delete
        menuDisplay = 420;
      } else if (menuDisplay >= 4100 && menuDisplay <= 4199) {
        if (menuDisplay == 4100) {
          waypointComment = pointOfInterest[0];
        } else if (menuDisplay == 4101) {
          waypointComment = pointOfInterest[1];
        } else if (menuDisplay == 4102) {
          waypointComment = pointOfInterest[2];
        } else if (menuDisplay == 4103) {
          waypointComment = pointOfInterest[3];
        } else if (menuDisplay == 4104) {
          waypointComment = pointOfInterest[4];
        } else if (menuDisplay == 4105) {
          waypointComment = pointOfInterest[5];
        } else if (menuDisplay == 4106) {
          waypointComment = pointOfInterest[6];
        } else if (menuDisplay == 4107) {
          waypointComment = pointOfInterest[7];
        } else if (menuDisplay == 4108) {
          waypointComment = pointOfInterest[8];
        } else if (menuDisplay == 4109) {
        waypointComment = pointOfInterest[9];
        } else if (menuDisplay == 4110) {
          waypointComment = pointOfInterest[10];
        } else if (menuDisplay == 4111) {
          waypointComment = pointOfInterest[11];
        }
        send_update = true;
        menuDisplay = 41;
      }

      else if (menuDisplay == 5) {  // Weather Menu
        menuDisplay = 50;
      } else if (menuDisplay == 50) {
        sendMessage("APRS", "WRCLP", "wrl");
      } else if (menuDisplay == 51) {
        Serial.println("asking for 24 hrs Weather Forecast");
        //sendMessage("APRS", "WRCLP", "wrl 24");
      } else if (menuDisplay == 52) {
        Serial.println("asking for 5 days Weather Forecast");
        //sendMessage("APRS", "WRCLP", "wrl 5days");
      } else if (menuDisplay == 53) {
        menuDisplay = 530;
      } 

      else if (menuDisplay == 6) {    // SMS Menu
        menuDisplay = 60;
      } 
      
      else if (menuDisplay == 7) {  // News/Bulletin Menu
        menuDisplay = 70;
      } else if (menuDisplay == 70) {
        menuDisplay = 700;
      } else if (menuDisplay == 71) {
        Serial.println("CHECK NEWS");
        show_display("__INFO____", "", "NOT READY YET", "i'm coding ...", 1500);
      } else if (menuDisplay == 72) {
        Serial.println("CHECK BULLETIN");
        show_display("__INFO____", "", "NOT READY YET", "i'm coding ...", 1500);
      } 
      
      else if (menuDisplay == 8) {  // Winlink/Mail Menu Login 
        if (winlinkStatus == 5) {
          menuDisplay = 800;
        } else {
          winlinkStatus = 1;
          sendMessage("WLNK", "WLNK-1", "L");
          menuDisplay = 80;
        }
      } 

      else if (menuDisplay == 800) {      // List Pending Mails
        show_display("__WLNK_Tx>", "", "    LIST MAILS",1000);
        sendMessage("WLNK", "WLNK-1", "L");
        menuDisplay = 800;
        Serial.println(menuDisplay);
      } else if (menuDisplay == 801) {    // Downloaded Mails
        loadMessagesFromMemory("WLNK");
        if (noMessageWarning) {
          menuDisplay = 801;
        } else {
          menuDisplay = 8010;
        }
      } else if (menuDisplay == 802) {    // Read Mail n° #
        menuDisplay = 8020;
      } else if (menuDisplay == 803) {    // Reply Mail n° #
        menuDisplay = 8030;
      } else if (menuDisplay == 804) {    // Forward Mail n° #
        menuDisplay = 8040;
      } else if (menuDisplay == 805) {    // Delete Mail n° #
        menuDisplay = 8050;
      } else if (menuDisplay == 806) {    // Alias Menu
        menuDisplay = 8060;
      } else if (menuDisplay == 807) {    // Log Out
        sendMessage("WLNK", "WLNK-1", "B");
        show_display("__WLNK_Tx>", "", "    LOG OUT !!!",2000);
        menuDisplay = 8;
        winlinkStatus = 0;
      } else if (menuDisplay == 808) {    // Write Mail
        menuDisplay = 8080;
      } else if (menuDisplay == 8010) {   // Read Saved Mails
        menuDisplay = 80100;
      } else if (menuDisplay == 8011) {   // Delete Saved Mails
        deleteFile(winlinkMsgFilePath);
        show_display("_DELETING_", "", " ALL MAILS DELETED", 1500);
        loadNumMessages();
        menuDisplay = 801;
      } else if (menuDisplay == 80100) {
        messagesIterator++;
        if (messagesIterator == numWNLKMessages) {
          menuDisplay = 8010;
          messagesIterator = 0;
        } else {
          menuDisplay = 80100;
        }
      } else if (menuDisplay == 8060) {   // Create Alias
        menuDisplay = 80600;
      } else if (menuDisplay == 8061) {   // Delete Alias
        menuDisplay = 80610;
      } else if (menuDisplay == 8062) {   // List all Alias
        show_display("__WLNK_Tx>", "", "   LIST ALL ALIAS",1500);
        sendMessage("WLNK", "WLNK-1", "AL");
        menuDisplay = 8062;
      } else if (menuDisplay == 8083) {
        show_display("__WLNK_Tx>", "", "Ending Mail", 2000);
        sendMessage("WLNK", "WLNK-1", "/EX");
        winlinkAddressee = "";
        winlinkSubject = "";
        winlinkBody = "";
        menuDisplay = 808;
      } else if (menuDisplay == 8084) {
        winlinkBody = "";
        menuDisplay = 8082;
      }

      else if (menuDisplay == 9) {  // Alerts Menu
        menuDisplay = 90;
      }*/
    }
  }


  void read() {
    uint32_t lastKey = millis() - keyboardTime;
    if (lastKey > 30*1000) {
      keyboardDetected = false;
    }
    Wire.requestFrom(CARDKB_ADDR, 1);
    while(Wire.available()) {
      char c = Wire.read();
      if (c != 0) {

        // just for debugging
        Serial.print(c, DEC); Serial.print(" "); Serial.print(c, HEX); Serial.print(" "); Serial.println(char(c));
        //

        keyboardTime = millis();
        processPressedKey(c);      
      }
    }
  }

}