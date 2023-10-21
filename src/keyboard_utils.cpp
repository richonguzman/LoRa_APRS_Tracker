#include "keyboard_utils.h"
#include <Wire.h>

#define CARDKB_ADDR 0x5F // yes , this is for CARDKB from m5stack.com

extern bool keyboardDetected;
extern int  menuDisplay;

namespace KEYBOARD_Utils {

  void processPressedKey(char key) {
    keyboardDetected = true;
    Serial.print("menuDisplay = "); Serial.print(menuDisplay);
    Serial.print(" --> keyboard: "); Serial.println(key);
    /*  181 -> up / 182 -> down / 180 <- back / 183 -> forward / 8 Delete / 13 Enter / 32 Space  / 27 Esc */

    /*if (menuDisplay == 0 && key == 33) {                                  // Main Menu
      menuDisplay = 1;
    } else if (key == 27) {     // ESC = volver a menu base
      menuDisplay = 0;
      messagesIterator = 0;
      messageText = "";
      messageCallsign = "";
    } else if (menuDisplay >= 1 && menuDisplay <= 9 && key >=49 && key <= 57) { // Menu number select
      menuDisplay = key - 48;
    } else if (menuDisplay == 0 && key == 37) {
      handleNextGpsLocation(myGpsLocationsSize);
    } else if (menuDisplay == 0 && key == 41) {
      if (showCompass == false) {
        showCompass = true;
      } else {
        showCompass = false;
      }
    }

    else if ((menuDisplay == 1100 && key != 180) || (menuDisplay == 1110 && key != 180)) {        // Writing Callsign of Message
      if (messageCallsign.length() == 1) {
        messageCallsign.trim();
      }
      if ((key >= 48 && key <= 57) || (key >= 65 && key <= 90) || (key >= 97 && key <= 122) || (key == 45)) { //only letters + numbers + "-"
        messageCallsign += key;
      } else if (key == 13) {     // Return Pressed
        messageCallsign.trim();
        if (menuDisplay == 1100) {
          menuDisplay = 1101;
        } else if (menuDisplay == 1110) {
          menuDisplay = 1111;
        }
      } else if (key == 8) {      // Delete Last Key
        messageCallsign = messageCallsign.substring(0, messageCallsign.length()-1);
      }
      messageCallsign.toUpperCase();
      Serial.println(messageCallsign);
    } else if ((menuDisplay == 1101 && key!= 180) || (menuDisplay == 1111 && key!= 180)) {        // Writting Text of Message
      if (messageText.length() == 1) {
        messageText.trim();
      }
      if (key >= 32 && key <= 126) {
        messageText += key;
      } else if (key == 13) {     // Return Pressed: SENDING MESSAGE
        messageText.trim();
        Serial.println(messageText.length());
        if (messageText.length() > 67){
          Serial.println(messageText.length());
          messageText = messageText.substring(0,67);
        }
        Serial.println(messageText.length());
        if (menuDisplay == 1101) {
          sendMessage("APRS", messageCallsign, messageText);
        } else if (menuDisplay == 1111) {
          sendMessage("LoRa", messageCallsign, messageText);
        }
        menuDisplay = 11;
        messageCallsign = "";
        messageText = "";
      } else if (key == 8) {      // Delete Last Key
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
    }

    else if (key == 181) {                        // Arrow Up
      if (menuDisplay >= 1 && menuDisplay <= 9) {
        menuDisplay--;
        if (menuDisplay == 0) {
          menuDisplay = 9;
        }
      } else if (menuDisplay == 11) {     // Menu Messages
        menuDisplay = 10;
      } else if (menuDisplay == 12) {
        menuDisplay = 11;
      } else if (menuDisplay == 101) {
        menuDisplay = 100;
      } else if (menuDisplay == 102) {
        menuDisplay = 101;
      } else if (menuDisplay == 103) {
        menuDisplay = 102;
      } else if (menuDisplay == 111) {
        menuDisplay = 110;
      } else if (menuDisplay == 121) {
        menuDisplay = 120;
      } else if (menuDisplay == 122) {
        menuDisplay = 121;
      }

      else if (menuDisplay == 21) {     // Menu Status
        menuDisplay = 20;
      } else if (menuDisplay == 210) { 
        menuDisplay = 214;
      } else if (menuDisplay == 214) { 
        menuDisplay = 213;
      } else if (menuDisplay == 213) { 
        menuDisplay = 212;
      } else if (menuDisplay == 212) { 
        menuDisplay = 211;
      } else if (menuDisplay == 211) { 
        menuDisplay = 210;
      } 
      
      else if (menuDisplay == 31) {     // Menu Stations
        menuDisplay = 30;
      } else if (menuDisplay == 301) {
        menuDisplay = 300;
      } else if (menuDisplay == 302) {
        menuDisplay = 301;
      } else if (menuDisplay == 311) {
        menuDisplay = 310;
      } else if (menuDisplay == 312) {
        menuDisplay = 311;
      } else if (menuDisplay == 313) {
        menuDisplay = 312;
      }

      else if (menuDisplay == 41) {     // Menu Waypoints
        menuDisplay = 40;
      } else if (menuDisplay == 42) {
        menuDisplay = 41;
      } else if (menuDisplay == 4111) {
        menuDisplay = 4110;
      } else if (menuDisplay == 4110) {
        menuDisplay = 4109;
      } else if (menuDisplay == 4109) {
        menuDisplay = 4108;
      } else if (menuDisplay == 4108) {
        menuDisplay = 4107;
      } else if (menuDisplay == 4107) {
        menuDisplay = 4106;
      } else if (menuDisplay == 4106) {
        menuDisplay = 4105;
      } else if (menuDisplay == 4105) {
        menuDisplay = 4104;
      } else if (menuDisplay == 4104) {
        menuDisplay = 4103;
      } else if (menuDisplay == 4103) {
        menuDisplay = 4102;
      } else if (menuDisplay == 4102) {
        menuDisplay = 4101;
      } else if (menuDisplay == 4101) {
        menuDisplay = 4100;
      } else if (menuDisplay == 4100) {
        menuDisplay = 4111;
      }

      else if (menuDisplay >= 51 && menuDisplay <=53) {
        menuDisplay--;
      } else if (menuDisplay == 50) {
        menuDisplay = 53;
      }

      else if (menuDisplay >= 71 && menuDisplay <=72) {
        menuDisplay--;
      }

      else if (menuDisplay >= 800 && menuDisplay <= 808) {
        menuDisplay--;
        if (menuDisplay == 799) {
          menuDisplay = 808;
        }
      } else if (menuDisplay == 8011) {
        menuDisplay = 8010;
      } else if (menuDisplay >= 8061 && menuDisplay <= 8062) {
        menuDisplay--;
      } else if (menuDisplay == 8084) { 
        menuDisplay = 8083;
      }
    } 
    
    else if (key == 182) {                        // Arrow Down
      if (menuDisplay >= 1 && menuDisplay <= 9) {       
        menuDisplay++;
        if (menuDisplay == 10) {
          menuDisplay = 1;
        }
      } else if (menuDisplay == 10) {     // Menu Messages
        menuDisplay = 11;
      } else if (menuDisplay == 11) {
        menuDisplay = 12;
      } else if (menuDisplay == 100) {
        menuDisplay = 101;
      } else if (menuDisplay == 101) {
        menuDisplay = 102;
      } else if (menuDisplay == 102) {
        menuDisplay = 103;
      } else if (menuDisplay == 110) {
        menuDisplay = 111;
      } else if (menuDisplay == 120) {
        menuDisplay = 121;
      } else if (menuDisplay == 121) {
        menuDisplay = 122;
      } 

      else if (menuDisplay == 20) {     // Menu Status
        menuDisplay = 21;
      } else if (menuDisplay == 210) { 
        menuDisplay = 211;
      } else if (menuDisplay == 211) { 
        menuDisplay = 212;
      } else if (menuDisplay == 212) { 
        menuDisplay = 213;
      } else if (menuDisplay == 213) { 
        menuDisplay = 214;
      } else if (menuDisplay == 214) { 
        menuDisplay = 210;
      } 
      
      else if (menuDisplay == 30) {   // Menu Stations
        menuDisplay = 31;
      } else if (menuDisplay == 300) {
        menuDisplay = 301;
      } else if (menuDisplay == 301) {
        menuDisplay = 302;
      } else if (menuDisplay == 310) {
        menuDisplay = 311;
      } else if (menuDisplay == 311) {
        menuDisplay = 312;
      } else if (menuDisplay == 312) {
        menuDisplay = 313;
      }

      else if (menuDisplay == 40) {     // Menu Waypoints
        menuDisplay = 41;
      } else if (menuDisplay == 41 && !recordingWaypoints) {
        menuDisplay = 42;
      } else if (menuDisplay == 4100) {
        menuDisplay = 4101;
      } else if (menuDisplay == 4101) {
        menuDisplay = 4102;
      } else if (menuDisplay == 4102) {
        menuDisplay = 4103;
      } else if (menuDisplay == 4103) {
        menuDisplay = 4104;
      } else if (menuDisplay == 4104) {
        menuDisplay = 4105;
      } else if (menuDisplay == 4105) {
        menuDisplay = 4106;
      } else if (menuDisplay == 4106) {
        menuDisplay = 4107;
      } else if (menuDisplay == 4107) {
        menuDisplay = 4108;
      } else if (menuDisplay == 4108) {
        menuDisplay = 4109;
      } else if (menuDisplay == 4109) {
        menuDisplay = 4110;
      } else if (menuDisplay == 4110) {
        menuDisplay = 4111;
      } else if (menuDisplay == 4111) {
        menuDisplay = 4100;
      }

      else if (menuDisplay >= 50 && menuDisplay <=52) {
        menuDisplay++;
      } else if (menuDisplay == 53) {
        menuDisplay = 50;
      }

      else if (menuDisplay >= 70 && menuDisplay <=71) {
        menuDisplay++;
      }

      else if (menuDisplay >= 800 && menuDisplay <= 808) {       
        menuDisplay++;
        if (menuDisplay == 809) {
          menuDisplay = 800;
        }
      } else if (menuDisplay == 8010) {
        menuDisplay = 8011;
      } else if (menuDisplay >= 8060 && menuDisplay <= 8061) {
        menuDisplay++;
      } else if (menuDisplay == 8083) { 
        menuDisplay = 8084;
      }
    } 

    else if (key == 180) {                        // Arrow Left
      if (menuDisplay >= 1 && menuDisplay <= 9) {   
        menuDisplay = 0;    
      } else if (menuDisplay >= 10 && menuDisplay <= 12) {      // Return to Menu : Messages
        menuDisplay = 1;
      } else if (menuDisplay >= 100 && menuDisplay <= 109) {
        menuDisplay = 10;
      } else if (menuDisplay >= 110 && menuDisplay <= 119) {
        menuDisplay = 11;
      } else if (menuDisplay >= 120 && menuDisplay <= 129) {
        menuDisplay = 12;
      } else if (menuDisplay >= 1200 && menuDisplay <= 1299) {
        menuDisplay = 120;
      } else if (menuDisplay == 1100) { 
        menuDisplay = 110;
        messageCallsign = "";
      } else if (menuDisplay == 1101) {
        messageText = "";
        menuDisplay = 1100;
      }

      else if (menuDisplay >= 20 && menuDisplay <= 21) {        // Return to Menu : Status
        menuDisplay = 2;
      } else if (menuDisplay >= 210 && menuDisplay <= 219) {
        menuDisplay = 21;
      }
    
      else if (menuDisplay >= 30 && menuDisplay <= 31) {        // Return to Menu : Stations
        menuDisplay = 3;
      } else if (menuDisplay >= 300 && menuDisplay <= 309) {
        menuDisplay = 30;
      } else if (menuDisplay >= 310 && menuDisplay <= 319) {
        menuDisplay = 31;
      } else if (menuDisplay >= 3000 && menuDisplay <= 3009) {
        menuDisplay = 300;
      }
    
      else if (menuDisplay >= 40 && menuDisplay <= 42) {        // Return to Menu: Waypoints
        menuDisplay = 4;
      } else if (menuDisplay >= 400 && menuDisplay <= 409) {
        menuDisplay = 40;
      } else if (menuDisplay >= 410 && menuDisplay <= 419) {
        menuDisplay = 41;
      } else if (menuDisplay >= 420 && menuDisplay <= 429) {
        menuDisplay = 42;
      } else if (menuDisplay >= 4100 && menuDisplay <= 4199) {
        menuDisplay = 41;
      }

      else if (menuDisplay >= 50 && menuDisplay <= 59) {      // Return to Menu : Weather
        menuDisplay = 5;
      } else if (menuDisplay == 530) {
        menuDisplay = 53;
        weatherOtherPlace = "";
      } 
      
      else if (menuDisplay == 60) {
        menuDisplay = 6;
      } else if (menuDisplay >= 70 && menuDisplay <= 72) {
        menuDisplay = 7;
      } else if (menuDisplay == 700) {
        messageText = "";
        menuDisplay = 70;
      }
      
      else if (menuDisplay >= 80 && menuDisplay <= 84) {
        menuDisplay = 8;
      } else if (menuDisplay >= 800 && menuDisplay <= 808) {
        menuDisplay = 8;
      } else if (menuDisplay >= 8010 && menuDisplay <= 8080) {
        if (menuDisplay >= 8020 && menuDisplay <= 8050) {
          winlinkMailNumber = "";
        }
        menuDisplay = int(menuDisplay/10);
      } else if (menuDisplay == 90) {
        menuDisplay = 9;
      } 
    }

    else if (key == 183) {                        // Arrow Right
      if (menuDisplay == 1) { 
        menuDisplay = 10;       // Menu : Messages
      } else if (menuDisplay == 10) { 
        menuDisplay = 100;      // Menu : Messages READ
      } else if (menuDisplay == 100) {
        loadMessagesFromMemory("APRS");
        if (noMessageWarning) {
          menuDisplay = 100;
        } else {
          menuDisplay = 1000;
        }
      }  else if (menuDisplay == 1000) {
        messagesIterator++;
        if (messagesIterator == numAPRSMessages) {
          menuDisplay = 100;
          messagesIterator = 0;
        } else {
          menuDisplay = 1000;
        }
      } else if (menuDisplay == 101) {
        Serial.println("--> Messages : READ : APRS SUMMARY");
      } else if (menuDisplay == 102) {
        loadMessagesFromMemory("LoRa");
        if (noMessageWarning) {
          menuDisplay = 102;
        } else {
          menuDisplay = 1020;
        }
      } else if (menuDisplay == 1020) {
        messagesIterator++;
        if (messagesIterator == numLoraMessages) {
          menuDisplay = 100;
          messagesIterator = 0;
        } else {
          menuDisplay = 1020;
        }
      } else if (menuDisplay == 103) {
        Serial.println("--> Messages : READ : DIRECT SUMMARY");
      } else if (menuDisplay == 110) {
        menuDisplay = 1100;
      } 
    
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
      }
    }*/
  }


  void read() {
    Wire.requestFrom(CARDKB_ADDR, 1);
    while(Wire.available()) {
      char c = Wire.read();
      if (c != 0) {
        // just for debugging
        Serial.print(c, DEC); Serial.print(" "); Serial.print(c, HEX); Serial.print(" "); Serial.println(char(c));
        //
        processPressedKey(c);      
      }
    }
  }

}