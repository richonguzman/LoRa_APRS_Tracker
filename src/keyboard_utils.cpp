#include <APRSPacketLib.h>
#include <TinyGPS++.h>
#include <logger.h>
#include <Wire.h>
#include "keyboard_utils.h"
#include "winlink_utils.h"
#include "station_utils.h"
#include "configuration.h"
#include "board_pinout.h"
#include "power_utils.h"
#include "sleep_utils.h"
#include "menu_utils.h"
#include "msg_utils.h"
#include "display.h"
#include "utils.h"


extern Configuration    Config;
extern Beacon           *currentBeacon;
extern TinyGPSPlus      gps;
extern logging::Logger  logger;
extern bool             sendUpdate;
extern int              menuDisplay;
extern uint32_t         menuTime;
extern uint8_t          myBeaconsIndex;
extern int              myBeaconsSize;
extern uint8_t          loraIndex;
extern bool             displayState;
extern uint32_t         displayTime;
extern bool             displayEcoMode;
extern uint8_t          screenBrightness;
extern bool             statusState;
extern uint32_t         statusTime;
extern int              messagesIterator;
extern bool             messageLed;
extern String           messageCallsign;
extern String           messageText;
extern bool             sendStandingUpdate;
extern bool             flashlight;
extern bool             digipeaterActive;
extern bool             sosActive;
extern uint8_t          winlinkStatus;
extern String           winlinkMailNumber;
extern String           winlinkAddressee;
extern String           winlinkSubject;
extern String           winlinkBody;
extern String           winlinkAlias;
extern String           winlinkAliasComplete;
extern bool             winlinkCommentState;
extern bool             gpsIsActive;
extern bool             sendStartTelemetry;
extern uint8_t          keyboardAddress;

extern std::vector<String>  outputMessagesBuffer;

bool        keyboardConnected       = false;
bool        keyDetected             = false;
uint32_t    keyboardTime            = millis();

String      messageCallsign         = "";
String      messageText             = "";

int         messagesIterator        = 0;

bool        showHumanHeading        = false;


namespace KEYBOARD_Utils {

    void upArrow() {
        if (menuDisplay >= 1 && menuDisplay <= 6) {
            menuDisplay--;
            if (menuDisplay < 1) menuDisplay = 6;
        } else if (menuDisplay >= 10 && menuDisplay <= 13) {
            menuDisplay--;
            if (menuDisplay < 10) menuDisplay = 13;
        } else if (menuDisplay >= 130 && menuDisplay <= 133) {
            menuDisplay--;
            if (menuDisplay < 130) menuDisplay = 133;
        }

        else if (menuDisplay >= 20 && menuDisplay <= 27) {
            menuDisplay--;
            if (menuDisplay < 20) menuDisplay = 27;
        } else if (menuDisplay >= 220 && menuDisplay <= 221) {
            menuDisplay--;
            if (menuDisplay < 220) menuDisplay = 221;
        } else if (menuDisplay >= 2210 && menuDisplay <= 2212) {
            menuDisplay--;
            if (menuDisplay < 2210) menuDisplay = 2212;
        } else if (menuDisplay >= 240 && menuDisplay <= 241) {
            menuDisplay--;
            if (menuDisplay < 240) menuDisplay = 241;
        }
        
        else if (menuDisplay >= 30 && menuDisplay <= 33) {
            menuDisplay--;
            if (menuDisplay < 30) menuDisplay = 33;
        }

        else if (menuDisplay >= 40 && menuDisplay <= 41) {
            menuDisplay--;
            if (menuDisplay < 40) menuDisplay = 41;
        }
        
        else if (menuDisplay >= 50 && menuDisplay <= 53) {
            menuDisplay--;
            if (menuDisplay < 50) menuDisplay = 53;
        } else if (menuDisplay == 5000 || menuDisplay == 5010 || menuDisplay == 5020 || menuDisplay == 5030 || menuDisplay == 5040 || menuDisplay == 5050 || menuDisplay == 5060 || menuDisplay == 5070 || menuDisplay == 5080) {
            menuDisplay = menuDisplay - 10;
            if (menuDisplay < 5000) menuDisplay = 5080;
        } else if (menuDisplay == 50100 || menuDisplay == 50110) {
            menuDisplay = menuDisplay - 10;
            if (menuDisplay < 50100) menuDisplay = 50110;
        } else if (menuDisplay >= 5061 && menuDisplay <= 5063) {
            menuDisplay--;
            if (menuDisplay < 5061) menuDisplay = 5063;
        } else if (menuDisplay >= 5084 && menuDisplay <= 5085) {
            menuDisplay--;
            if (menuDisplay < 5084) menuDisplay = 5085;
        }
        
        else if (menuDisplay >= 60 && menuDisplay <= 64) {
            menuDisplay--;
            if (menuDisplay < 60) menuDisplay = 64;
        }
        
        else if (menuDisplay >= 9000 && menuDisplay <= 9001) {
            menuDisplay--;
            if (menuDisplay < 9000) menuDisplay = 9001;
        }
    }

    void downArrow() {
        if (menuDisplay == 0) {
            if (displayState) {
                sendUpdate = true;
                if (!gpsIsActive) SLEEP_Utils::gpsWakeUp();
            } else {
                displayToggle(true);
                displayTime = millis();   
                displayState = true;  
            }
        }
        if (menuDisplay >= 1 && menuDisplay <= 6) {
            menuDisplay++;
            if (menuDisplay > 6) menuDisplay = 1;
        }
        else if (menuDisplay >= 10 && menuDisplay <= 13) {
            menuDisplay++;
            if (menuDisplay > 13) menuDisplay = 10;
        } else if (menuDisplay >= 130 && menuDisplay <= 133) {
            menuDisplay++;
            if (menuDisplay > 133) menuDisplay = 130;
        } else if (menuDisplay == 100) {
            messagesIterator++;
            if (messagesIterator == MSG_Utils::getNumAPRSMessages()) {
                menuDisplay = 10;
                messagesIterator = 0;
                if (Config.notification.ledMessage) messageLed = false;
            } else {
                menuDisplay = 100;
            }
        } else if (menuDisplay == 110) {
            menuDisplay = 11;
        }
        
        else if (menuDisplay >= 20 && menuDisplay <= 27) {
        menuDisplay++;
        if (menuDisplay > 27) menuDisplay = 20;
        } else if (menuDisplay >= 220 && menuDisplay <= 221) {
            menuDisplay++;
            if (menuDisplay > 221) menuDisplay = 220;
        } else if (menuDisplay >= 2210 && menuDisplay <= 2212) {
            menuDisplay++;
            if (menuDisplay > 2212) menuDisplay = 2210;
        } else if (menuDisplay >= 240 && menuDisplay <= 241) {
            menuDisplay++;
            if (menuDisplay > 241) menuDisplay = 240;
        }

        else if (menuDisplay >= 30 && menuDisplay <= 33) {
            menuDisplay++;  
            if (menuDisplay > 33) menuDisplay = 30;
        }

        else if (menuDisplay >= 40 && menuDisplay <= 41) {
            menuDisplay++;  
            if (menuDisplay > 41) menuDisplay = 40;
        }
        
        else if (menuDisplay >= 50 && menuDisplay <= 53) {
        menuDisplay++;  
        if (menuDisplay > 53) menuDisplay = 50;
        } else if (menuDisplay == 5000 || menuDisplay == 5010 || menuDisplay == 5020 || menuDisplay == 5030 || menuDisplay == 5040 || menuDisplay == 5050 || menuDisplay == 5060 || menuDisplay == 5070 || menuDisplay == 5080) {
            menuDisplay = menuDisplay + 10;
            if (menuDisplay > 5080) menuDisplay = 5000;
        } else if (menuDisplay == 50100 || menuDisplay == 50110) {
            menuDisplay = menuDisplay + 10;
            if (menuDisplay > 50110) menuDisplay = 50100;
        } else if (menuDisplay == 50101) {
            messagesIterator++;
            if (messagesIterator == MSG_Utils::getNumWLNKMails()) {
                if (winlinkStatus == 0) {
                    menuDisplay = 51;
                } else {
                    menuDisplay = 50100;
                }
                messagesIterator = 0;
                if (Config.notification.ledMessage) messageLed = false;
            } else {
                menuDisplay = 50101;
            }
        } else if (menuDisplay >= 5061 && menuDisplay <= 5063) {
            menuDisplay++;
            if (menuDisplay > 5063) menuDisplay = 5061;
        } else if (menuDisplay >= 5084 && menuDisplay <= 5085) {
            menuDisplay++;
            if (menuDisplay > 5085) menuDisplay = 5084;
        } 

        else if (menuDisplay >= 60 && menuDisplay <= 64) {
            menuDisplay++;
            if (menuDisplay > 64) menuDisplay = 60;
        }

        else if (menuDisplay >= 9000 && menuDisplay <= 9001) {
            menuDisplay++;
            if (menuDisplay > 9001) menuDisplay = 9000;
        }
    }

    void leftArrow() {
        if (menuDisplay >= 1 && menuDisplay <= 6) {
            menuDisplay = 0;
        } else if (menuDisplay == 100) {
            messagesIterator = 0;
            menuDisplay = 10;
        } else if (menuDisplay == 110) {
            messageCallsign = "";
            menuDisplay = 11;
        } else if (menuDisplay == 111) {
            messageText = "";
            menuDisplay = 110;
        } else if (menuDisplay == 1300 ||  menuDisplay == 1310) {
            messageText = "";
            menuDisplay = menuDisplay/10;
        } else if ((menuDisplay>=10 && menuDisplay<=13) || (menuDisplay>=20 && menuDisplay<=29) || (menuDisplay == 120) || (menuDisplay>=130 && menuDisplay<=133) || (menuDisplay>=50 && menuDisplay<=53) || (menuDisplay>=200 && menuDisplay<=290) || (menuDisplay>=2210 && menuDisplay<=2212) || (menuDisplay>=60 && menuDisplay<=64) || (menuDisplay>=30 && menuDisplay<=33) || (menuDisplay>=40 && menuDisplay<=41) || (menuDisplay>=400 && menuDisplay<=410)) {
            menuDisplay = int(menuDisplay/10);
        } else if (menuDisplay == 5000 || menuDisplay == 5010 || menuDisplay == 5020 || menuDisplay == 5030 || menuDisplay == 5040 || menuDisplay == 5050 || menuDisplay == 5060 || menuDisplay == 5070 || menuDisplay == 5080) {
            menuDisplay = 5;
        } else if (menuDisplay == 5021 || menuDisplay == 5041 || menuDisplay == 5051) {
            winlinkMailNumber = "_?";
            menuDisplay--;
        } else if (menuDisplay >= 5061 && menuDisplay <= 5063) {
            menuDisplay = 5060;
        } else if (menuDisplay == 50100 || menuDisplay == 50110) {
            menuDisplay = 5010;
        } else if (menuDisplay == 50101) {
            messagesIterator = 0;
            if (winlinkStatus == 0) {
                menuDisplay = 51;
            } else {
                menuDisplay = 50100;
            }
        } else if (menuDisplay == 50111) {
            if (winlinkStatus == 0) {
                menuDisplay = 52;
            } else {
                menuDisplay = 50110;
            }
        } else if (menuDisplay == 630) {
            messageText = "";
            menuDisplay = 63;
        }
    }

    void rightArrow() {
        if (menuDisplay == 0 || menuDisplay == 200) {
            if (myBeaconsIndex >= (myBeaconsSize - 1)) {
                myBeaconsIndex = 0;
            } else {
                myBeaconsIndex++;
            }
            displayToggle(true);
            displayTime = millis();
            statusState  = true;
            statusTime = millis();
            winlinkCommentState = false;
            displayShow("   INFO", "", "  CHANGING CALLSIGN!", "", "-----> " + Config.beacons[myBeaconsIndex].callsign, "", 2000);
            STATION_Utils::saveIndex(0, myBeaconsIndex);
            sendStartTelemetry = true;
            if (menuDisplay == 200) menuDisplay = 20;
        } else if ((menuDisplay >= 1 && menuDisplay <= 6) || (menuDisplay >= 11 &&menuDisplay <= 13) || (menuDisplay >= 20 && menuDisplay <= 27) || (menuDisplay >= 40 && menuDisplay <= 41)) {
            menuDisplay = menuDisplay * 10;
        } else if (menuDisplay == 10) {
            MSG_Utils::loadMessagesFromMemory(0);
            if (MSG_Utils::warnNoAPRSMessages()) {
                #ifdef HAS_JOYSTICK
                    menuDisplay = 11;
                #else
                    menuDisplay = 10;
                #endif
            } else {
                menuDisplay = 100;
            }
        } else if (menuDisplay == 120) {
            MSG_Utils::deleteFile(0);
            displayShow("   INFO", "", "ALL MESSAGES DELETED!", 2000);
            MSG_Utils::loadNumMessages();
            menuDisplay = 12;
        } else if (menuDisplay == 130 || menuDisplay == 131) {
            if (keyDetected) {
                menuDisplay *= 10;
            } else {
                displayShow(" APRS Thu.", "Sending:", "Happy #APRSThursday", "from LoRa Tracker 73!", "", "", 2000);
                MSG_Utils::addToOutputBuffer(0, (menuDisplay == 130) ? "APRSPH" : "ANSRVR" , (menuDisplay == 130) ? "HOTG Happy #APRSThursday from LoRa Tracker 73!" : "CQ HOTG Happy #APRSThursday from LoRa Tracker 73!"); 
            }
        } else if (menuDisplay == 132 || menuDisplay == 133) {
            displayShow(" APRS Thu.", "", (menuDisplay == 132) ? "   Unsubscribe" : "  Keep Subscribed", (menuDisplay == 132) ? "   from APRS Thursday" : "  for 12hours more", "", "", 2000);
            MSG_Utils::addToOutputBuffer(0, "ANSRVR", (menuDisplay == 132) ? "U HOTG" : "K HOTG");
        }

        else if (menuDisplay == 210) {
            LoRa_Utils::changeFreq();
            STATION_Utils::saveIndex(1, loraIndex);
            menuDisplay = 21;
        } else if (menuDisplay == 220) {
            displayEcoMode = !displayEcoMode;
            displayShow(" DISPLAY", "", displayEcoMode ? "   ECO MODE -> ON" : "   ECO MODE -> OFF", 1000);
        } else if (menuDisplay == 221) {
            menuDisplay = 2210;
        } else if (menuDisplay >= 2210 && menuDisplay <= 2212) {
            switch (menuDisplay) {
                case 2210:  // low
                    #ifdef HAS_TFT
                        screenBrightness = 70;
                    #else
                        screenBrightness = 1;
                    #endif
                    break;
                case 2211:  // mid
                    #ifdef HAS_TFT
                        screenBrightness = 150;
                    #else
                        screenBrightness = 40;
                    #endif
                    break;
                case 2212:  // max
                    screenBrightness = 255;
                    break;
                default:
                    #ifdef HAS_TFT
                        screenBrightness = 255;
                    #else
                        screenBrightness = 1;
                    #endif
                    break;
            }
            #ifdef HAS_TFT
                analogWrite(TFT_BL, screenBrightness);
            #endif
            displayShow("  SCREEN", "", "SCREEN BRIGHTNESS " + MENU_Utils::screenBrightnessAsString(screenBrightness), 1000);
            STATION_Utils::saveIndex(2, screenBrightness);
            #ifdef HAS_JOYSTICK
                menuDisplay = 221;
            #endif
        }
        
        else if (menuDisplay == 240) {
            displayShow("  STATUS", "", "WRITE STATUS","STILL IN DEVELOPMENT!", "", "", 2000); /////////////////////////
        } else if (menuDisplay == 241) {
            displayShow("  STATUS", "", "SELECT STATUS","STILL IN DEVELOPMENT!", "", "", 2000); /////////////////////////
        } else if (menuDisplay == 250) {
            displayShow(" NOTIFIC", "", "NOTIFICATIONS","STILL IN DEVELOPMENT!", "", "", 2000); /////////////////////////
        } 

        else if (menuDisplay == 30) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Loop", "%s", "wrl");
            MSG_Utils::addToOutputBuffer(0, "CA2RXU-15", "wrl");
        } else if (menuDisplay == 31) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Loop", "%s", "9M2PJU-4: Hospital");
            MSG_Utils::addToOutputBuffer(0, "9M2PJU-4", "hospital");
        } else if (menuDisplay == 32) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Loop", "%s", "9M2PJU-4 : Police");
            MSG_Utils::addToOutputBuffer(0, "9M2PJU-4", "police");
        } else if (menuDisplay == 33) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Loop", "%s", "9M2PJU-4: Fire Station");
            MSG_Utils::addToOutputBuffer(0, "9M2PJU-4", "fire_station");
        }

        else if (menuDisplay == 50) {
            WINLINK_Utils::login();
            menuDisplay = 500;
        } else if (menuDisplay == 51) {
            MSG_Utils::loadMessagesFromMemory(1);
            if (MSG_Utils::warnNoWLNKMails()) {
                #ifdef HAS_JOYSTICK
                    menuDisplay = 50;
                #else
                    menuDisplay = 51;
                #endif
            } else {
                menuDisplay = 50101;
            }
        } else if (menuDisplay == 52) {
            menuDisplay = 50111;
        } else if (menuDisplay == 53) {
            if (winlinkCommentState) {
                winlinkCommentState = false;
                displayShow(" WINLINK>", "", "  WLNK COMMENTs OFF!", 2000);
            } else {
                winlinkCommentState = true;
                displayShow(" WINLINK>", "", "  WLNK COMMENTs ON!", 2000);
            }
        } else if (menuDisplay == 5000) {
            MSG_Utils::addToOutputBuffer(1, "WLNK-1", "L");
        } else if (menuDisplay == 5010) {
            menuDisplay = 50100;
        } else if (menuDisplay == 50100) {
            MSG_Utils::loadMessagesFromMemory(1);
            if (MSG_Utils::warnNoWLNKMails()) {
                menuDisplay = 50100;
            } else {
                menuDisplay = 50101;
            }
        } else if (menuDisplay == 50110) {
            menuDisplay = 50111;
        } else if (menuDisplay == 50111) {
            MSG_Utils::deleteFile(1);
            displayShow("   INFO", "", " ALL MAILS DELETED!", 2000);
            MSG_Utils::loadNumMessages();
            if (winlinkStatus == 0) {
                #ifdef HAS_JOYSTICK
                    menuDisplay = 50;
                #else
                    menuDisplay = 52;
                #endif
            } else {
                menuDisplay = 50110;
            }

        } else if (menuDisplay >= 5020 && menuDisplay <= 5060 && menuDisplay % 10 == 0) {
            menuDisplay++;
        } else if (menuDisplay >= 5061 && menuDisplay <= 5062) {
            menuDisplay *= 10;
        } else if (menuDisplay == 5063) {
            MSG_Utils::addToOutputBuffer(1, "WLNK-1", "AL");
        } else if (menuDisplay == 5070) {
            MSG_Utils::addToOutputBuffer(1, "WLNK-1", "B");
            menuDisplay = 5;
        } else if (menuDisplay == 5080) {
            menuDisplay = 5081;
        } else if (menuDisplay == 5084) {
            MSG_Utils::addToOutputBuffer(1, "WLNK-1", "/EX");
            winlinkAddressee = "";
            winlinkSubject = "";
            winlinkBody = "";
            menuDisplay = 5080;
        } else if (menuDisplay == 5085) {
            winlinkBody = "";
            menuDisplay = 5083;
        }

        else if (menuDisplay == 60) {
            if (Config.email != "") MSG_Utils::addToOutputBuffer(0, "9M2PJU-4", "posmsg " + String(Config.email));
        } else if (menuDisplay == 61) {
            digipeaterActive = !digipeaterActive;
            displayShow("  EXTRAS", "", "     Digipeater", digipeaterActive ? "   Status --> ON" : "   Status --> OFF", "", "", 2000);
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "Main", "%s", digipeaterActive ? "Digipeater ON" : "Digipeater OFF");
        } else if (menuDisplay == 62) {
            sosActive = !sosActive;
            displayShow("  EXTRAS", "", "       S.O.S.", sosActive ? "   Status --> ON" : "   Status --> OFF", "", "", 2000);
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "Main", "S.O.S Mode %s", sosActive ? "ON" : "OFF");
        } else if (menuDisplay == 63) {
            menuDisplay = 630;
        } else if (menuDisplay == 64) {
            if (Config.notification.ledFlashlight) {
                flashlight = !flashlight;
                displayShow("  EXTRAS", "", "     Flashlight", flashlight ? "   Status --> ON" : "   Status --> OFF", "", "", 2000);
            } else {
                displayShow("  EXTRAS", "", "     Flashlight", "NOT ACTIVE IN CONFIG!", "", "", 2000);
            }
        } 

        else if (menuDisplay == 9000) {
            #if defined(HAS_AXP192) || defined(HAS_AXP2101)
                displayShow("", "", "    POWER OFF ...", 2000);
            #else
                displayShow("", "", "  starting DEEP SLEEP", 2000);
            #endif
            POWER_Utils::shutdown();
        } else if (menuDisplay == 9001) {
            displayShow("", "", "  STARTING WiFi AP", 2000);
            Config.wifiAP.active = true;
            Config.writeFile();
            ESP.restart();
        }
    }

    void processPressedKey(char key) {
        keyDetected = true;
        menuTime = millis();
        /*  181 -> up / 182 -> down / 180 <- back / 183 -> forward / 8 Delete / 13 Enter / 32 Space  / 27 Esc */
        if (!displayState) {
            displayToggle(true);
            displayTime = millis();   
            displayState = true;
        }
        if (menuDisplay == 0 && key == 13) {       // Main Menu
            menuDisplay = 1;      
        } else if (menuDisplay == 0 && key == 8) {
            showHumanHeading = !showHumanHeading;
        } else if (key == 27) {                           // ESC = return to Main Menu
            menuDisplay = 0;
            messagesIterator = 0;
            messageCallsign = "";
            messageText = "";
            winlinkMailNumber = "_?";
            winlinkAddressee = "";
            winlinkAlias = "";
            winlinkAliasComplete = "";
            winlinkSubject = "";
            winlinkBody = "";
        } else if (menuDisplay >= 1 && menuDisplay <= 6 && key >=49 && key <= 55) { // Menu number select
            menuDisplay = key - 48;
        } else if (menuDisplay == 110 && key != 180) {    // Writing Callsign of Message
            if (messageCallsign.length() == 1) messageCallsign.trim();
            if ((key >= 48 && key <= 57) || (key >= 65 && key <= 90) || (key >= 97 && key <= 122) || key == 45) { //only letters + numbers + "-"
                messageCallsign += key;
            } else if (key == 13) {                         // Return Pressed
                messageCallsign.trim();
                if (menuDisplay == 110) menuDisplay = 111;
            } else if (key == 8) {                          // Delete Last Key
                messageCallsign = messageCallsign.substring(0, messageCallsign.length() - 1);
            }
            messageCallsign.toUpperCase();
        } else if ((menuDisplay == 111 || menuDisplay == 1300 || menuDisplay == 1310) && key!= 180) {     // Writting Text of Message
            if (messageText.length() == 1) messageText.trim();
            if (key >= 32 && key <= 126) {
                messageText += key;
            } else if (key == 13 && messageText.length() > 0) {                         // Return Pressed: SENDING MESSAGE
                messageText.trim();
                if (messageText.length() > 67) messageText = messageText.substring(0, 67);
                if (menuDisplay == 111) {
                    MSG_Utils::addToOutputBuffer(0, messageCallsign, messageText);
                    menuDisplay = 11;
                } else if (menuDisplay == 1300 || menuDisplay == 1310) {
                    messageCallsign = (menuDisplay == 1300) ? "APRSPH" : "ANSRVR";
                    String prefix   = (menuDisplay == 1300) ? "HOTG "  : "CQ HOTG ";
                    MSG_Utils::addToOutputBuffer(0, messageCallsign, prefix + messageText);
                    menuDisplay /= 10;
                }
                messageCallsign = "";
                messageText = "";
            } else if (key == 8) {                          // Delete Last Key
                messageText = messageText.substring(0, messageText.length() - 1);
            }
        } else if (menuDisplay == 260 && key == 13) {
            displayShow("", "", "    REBOOTING ...", 2000);
            ESP.restart();
        } else if (menuDisplay == 270 && key == 13) {
            #if defined(HAS_AXP192) || defined(HAS_AXP2101)
                displayShow("", "", "    POWER OFF ...", 2000);
            #else
                displayShow("", "", " starting DEEP SLEEP", 2000);
            #endif
            POWER_Utils::shutdown();
        } else if ((menuDisplay == 5021 || menuDisplay == 5031 || menuDisplay == 5041 || menuDisplay == 5051) && key >= 48 && key <= 57) {
            winlinkMailNumber = key;
        } else if ((menuDisplay == 5021 || menuDisplay == 5031 || menuDisplay == 5041 || menuDisplay == 5051) && key == 8) {
            winlinkMailNumber = "_?";
        } else if (menuDisplay == 5021 && key == 13 && winlinkMailNumber != "_?") {
            MSG_Utils::addToOutputBuffer(1, "WLNK-1", "R" + winlinkMailNumber);
            winlinkMailNumber = "_?";
            menuDisplay = 5020;
        } else if (menuDisplay == 5031 && key == 13 && winlinkMailNumber != "_?") {
            MSG_Utils::addToOutputBuffer(1, "WLNK-1", "Y" + winlinkMailNumber);
            winlinkMailNumber = "_?";
            menuDisplay = 5083;
        } else if (menuDisplay == 5041 && key == 13 && winlinkMailNumber != "_?") {
            menuDisplay = 5042;
        } else if (menuDisplay == 5042) {
            if (winlinkAddressee.length() == 1) winlinkAddressee.trim();
            if ((key >= 65 && key <=90) || (key >= 97 && key <= 122) || (key >= 48 && key <= 57) || (key == 45) || (key == 46) || (key == 64) || (key == 95)) {
                winlinkAddressee += key;
            } else if (key == 13 && winlinkAddressee.length() > 0) {
                winlinkAddressee.trim();
                MSG_Utils::addToOutputBuffer(1, "WLNK-1", "F" + winlinkMailNumber + " " + winlinkAddressee);
                winlinkMailNumber = "_?";
                winlinkAddressee = "";
                menuDisplay = 5040;
            } else if (key == 8) {
                winlinkAddressee = winlinkAddressee.substring(0, winlinkAddressee.length() - 1);
            } else if (key == 180) { 
                menuDisplay = 5041;
                winlinkAddressee = "";
            }
        } else if (menuDisplay == 5051 && key == 13 && winlinkMailNumber !="_?") {
            MSG_Utils::addToOutputBuffer(1, "WLNK-1", "K" + winlinkMailNumber);
            winlinkMailNumber = "_?";
            menuDisplay = 5050;
        } else if (menuDisplay == 50610) {
            if (winlinkAlias.length() == 1) winlinkAlias.trim();
            if ((key >= 65 && key <=90) || (key >= 97 && key <= 122) || (key >= 48 && key <= 57)) {
                winlinkAlias += key;
            } else if (key == 13 && winlinkAlias.length()>= 1) {
                winlinkAlias.trim();
                menuDisplay = 50611;
            } else if (key == 8) {
                winlinkAlias = winlinkAlias.substring(0, winlinkAlias.length() - 1);
            } else if (key == 180) { 
                menuDisplay = 5061;
                winlinkAlias = "";
            }
        } else if (menuDisplay == 50611) {
            if (winlinkAliasComplete.length() == 1) winlinkAliasComplete.trim();
            if ((key >= 65 && key <=90) || (key >= 97 && key <= 122) || (key >= 48 && key <= 57) || (key == 45) || (key == 46) || (key == 64) || (key == 95)) {
                winlinkAliasComplete += key;
            } else if (key == 13 && winlinkAliasComplete.length()>= 1) {
                winlinkAliasComplete.trim();
                MSG_Utils::addToOutputBuffer(1, "WLNK-1", "A " + winlinkAlias + "=" + winlinkAliasComplete);
                winlinkAlias = "";
                winlinkAliasComplete = "";
                menuDisplay = 5061;
            } else if (key == 8) {
                winlinkAliasComplete = winlinkAliasComplete.substring(0, winlinkAliasComplete.length() - 1);
            } else if (key == 180) { 
                menuDisplay = 50610;
                winlinkAliasComplete = "";
            }
        } else if (menuDisplay == 50620) {
            if (winlinkAlias.length() == 1) winlinkAlias.trim();
            if ((key >= 65 && key <=90) || (key >= 97 && key <= 122) || (key >= 48 && key <= 57)) {
                winlinkAlias += key;
            } else if (key == 13 && winlinkAlias.length()>= 1) {
                winlinkAlias.trim();
                MSG_Utils::addToOutputBuffer(1, "WLNK-1", "A " + winlinkAlias + "=");
                winlinkAlias = "";
                menuDisplay = 5062;
            } else if (key == 8) {
                winlinkAlias = winlinkAlias.substring(0, winlinkAlias.length() - 1);
            } else if (key == 180) { 
                menuDisplay = 5062;
                winlinkAlias = "";
            }
        } else if (menuDisplay == 5081) {
            if (winlinkAddressee.length() == 1) winlinkAddressee.trim();
            if ((key >= 65 && key <=90) || (key >= 97 && key <= 122) || (key >= 48 && key <= 57) || (key == 45) || (key == 46) || (key == 64) || (key == 95)) {
                winlinkAddressee += key;
            } else if (key == 13 && winlinkAddressee.length() > 0) {
                winlinkAddressee.trim();
                menuDisplay = 5082;
            } else if (key == 8) {
                winlinkAddressee = winlinkAddressee.substring(0, winlinkAddressee.length() - 1);
            } else if (key == 180) { 
                menuDisplay = 5080;
                winlinkAddressee = "";
            }
        } else if (menuDisplay == 5082) {
            if (winlinkSubject.length() == 1) winlinkSubject.trim();
            if ((key >= 65 && key <=90) || (key >= 97 && key <= 122) || (key == 32) || (key >= 48 && key <= 57)) {
                winlinkSubject += key;
            } else if (key == 13 && winlinkSubject.length() > 0) {
                winlinkSubject.trim();
                MSG_Utils::addToOutputBuffer(1, "WLNK-1", "SP " + winlinkAddressee + " " + winlinkSubject);
                menuDisplay = 5083;
            } else if (key == 8) {
                winlinkSubject = winlinkSubject.substring(0, winlinkSubject.length() - 1);
            } else if (key == 180) { 
                menuDisplay = 5081;
                winlinkSubject = "";
            }
        } else if (menuDisplay == 5083) {
            if (winlinkBody.length() == 1) winlinkBody.trim();
            if ((key >= 32 && key <=122)) {
                winlinkBody += key;
            } else if (key == 13 && winlinkBody.length() <= 67) {
                winlinkBody.trim();
                MSG_Utils::addToOutputBuffer(1, "WLNK-1", winlinkBody);
                menuDisplay = 5084;
            } else if (key == 8) {
                winlinkBody = winlinkBody.substring(0, winlinkBody.length() - 1);
            } else if (key == 180) { 
                winlinkBody = "";
            }
        } else if (menuDisplay == 630 && key != 180) {
            if (messageText.length() == 1) messageText.trim();
            if (key >= 32 && key <= 126) {
                messageText += key;
            } else if (key == 13 && messageText.length() > 0) {
                messageText.trim();
                if (messageText.length() > 67) messageText = messageText.substring(0, 67);
                String packet = APRSPacketLib::generateBase91GPSBeaconPacket(currentBeacon->callsign, "APLRT1", Config.path, currentBeacon->overlay, APRSPacketLib::encodeGPSIntoBase91(gps.location.lat(),gps.location.lng(), gps.course.deg(), gps.speed.knots(), currentBeacon->symbol, Config.sendAltitude, gps.altitude.feet(), sendStandingUpdate, "GPS"));
                packet += messageText;
                displayShow("<<< TX >>>", "", packet,100);
                LoRa_Utils::sendNewPacket(packet);       
                messageText = "";
                menuDisplay = 63;
            } else if (key == 8) {
                messageText = messageText.substring(0, messageText.length() - 1);
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
        if (keyboardConnected) {
            uint32_t lastKey = millis() - keyboardTime;
            if (lastKey > 30 * 1000) keyDetected = false;
            Wire.requestFrom(keyboardAddress, static_cast<uint8_t>(1));
            while (Wire.available()) {
                char c = Wire.read();
                if (c != 0) {
                    //Serial.print(c, DEC); Serial.print(" "); Serial.print(c, HEX); Serial.print(" "); Serial.println(char(c));    // just for debugging
                    keyboardTime = millis();
                    processPressedKey(c);      
                }
            }
        }
    }

    void setup() {
        if (keyboardAddress != 0x00) keyboardConnected = true;
    }

}