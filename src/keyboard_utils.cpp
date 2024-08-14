#include <TinyGPS++.h>
#include <logger.h>
#include <Wire.h>
#include "keyboard_utils.h"
#include "APRSPacketLib.h"
#include "winlink_utils.h"
#include "station_utils.h"
#include "configuration.h"
#include "boards_pinout.h"
#include "power_utils.h"
#include "sleep_utils.h"
#include "msg_utils.h"
#include "display.h"

#ifdef TTGO_T_DECK_GPS
    #define KB_ADDR     0x55    // T-Deck internal keyboard (Keyboard Backlight On = ALT + B)
#else
    #define KB_ADDR     0x5F    // CARDKB from m5stack.com (YEL - SDA / WTH SCL)
#endif


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
extern bool             digirepeaterActive;
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

extern std::vector<String>  outputMessagesBuffer;

bool        keyboardConnected       = false;
bool        keyDetected             = false;
uint32_t    keyboardTime            = millis();

String      messageCallsign         = "";
String      messageText             = "";

int         messagesIterator        = 0;

bool        showHumanHeading        = false;

bool        mouseUpState            = 0;
bool        mouseDownState          = 0;
bool        mouseLeftState          = 0;
bool        mouseRightState         = 0;
int         debounceInterval        = 50;
uint32_t    lastDebounceTime        = millis();
int         upCounter               = 0;
int         downCounter             = 0;
int         leftCounter             = 0;
int         rightCounter            = 0;
int         trackBallSensitivity    = 5;


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

        else if (menuDisplay >= 20 && menuDisplay <= 27) {
            menuDisplay--;
            if (menuDisplay < 20) {
                menuDisplay = 27;
            }
        } else if (menuDisplay >= 220 && menuDisplay <= 221) {
            menuDisplay--;
            if (menuDisplay < 220) {
                menuDisplay = 221;
            }
        } else if (menuDisplay >= 240 && menuDisplay <= 241) {
            menuDisplay--;
            if (menuDisplay < 240) {
                menuDisplay = 241;
            }
        } 
        
        else if (menuDisplay >= 30 && menuDisplay <= 31) {
            menuDisplay--;
            if (menuDisplay < 30) {
                menuDisplay = 31;
            }
        } 
        
        else if (menuDisplay >= 50 && menuDisplay <= 53) {
            menuDisplay--;
            if (menuDisplay < 50) {
                menuDisplay = 53;
            }
        } else if (menuDisplay == 5000 || menuDisplay == 5010 || menuDisplay == 5020 || menuDisplay == 5030 || menuDisplay == 5040 || menuDisplay == 5050 || menuDisplay == 5060 || menuDisplay == 5070 || menuDisplay == 5080) {
            menuDisplay = menuDisplay - 10;
            if (menuDisplay < 5000) {
                menuDisplay = 5080;
            }
        } else if (menuDisplay == 50100 || menuDisplay == 50110) {
            menuDisplay = menuDisplay - 10;
            if (menuDisplay < 50100) {
                menuDisplay = 50110;
            }
        } else if (menuDisplay >= 5061 && menuDisplay <= 5063) {
            menuDisplay--;
            if (menuDisplay < 5061) {
                menuDisplay = 5063;
            }
        } else if (menuDisplay >= 5084 && menuDisplay <= 5085) {
            menuDisplay--;
            if (menuDisplay < 5084) {
                menuDisplay = 5085;
            }
        } 
        
        else if (menuDisplay >= 60 && menuDisplay <= 63) {
            menuDisplay--;
            if (menuDisplay < 60) {
                menuDisplay = 63;
            }
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
                if (Config.notification.ledMessage) {
                    messageLed = false;
                }
            } else {
                menuDisplay = 100;
            }
        } else if (menuDisplay == 110) {
            menuDisplay = 11;
        } 
        
        else if (menuDisplay >= 20 && menuDisplay <= 27) {
        menuDisplay++;
        if (menuDisplay > 27) {
            menuDisplay = 20;
        }
        } else if (menuDisplay >= 220 && menuDisplay <= 221) {
            menuDisplay++;
            if (menuDisplay > 221) {
                menuDisplay = 220;
            } 
        } else if (menuDisplay >= 240 && menuDisplay <= 241) {
            menuDisplay++;
            if (menuDisplay > 241) {
                menuDisplay = 240;
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

        else if (menuDisplay >= 50 && menuDisplay <= 53) {
        menuDisplay++;  
        if (menuDisplay > 53) {
            menuDisplay = 50;
        }
        } else if (menuDisplay == 5000 || menuDisplay == 5010 || menuDisplay == 5020 || menuDisplay == 5030 || menuDisplay == 5040 || menuDisplay == 5050 || menuDisplay == 5060 || menuDisplay == 5070 || menuDisplay == 5080) {
            menuDisplay = menuDisplay + 10;
            if (menuDisplay > 5080) {
                menuDisplay = 5000;
            }
        } else if (menuDisplay == 50100 || menuDisplay == 50110) {
            menuDisplay = menuDisplay + 10;
            if (menuDisplay > 50110) {
                menuDisplay = 50100;
            }
        } else if (menuDisplay == 50101) {
            messagesIterator++;
            if (messagesIterator == MSG_Utils::getNumWLNKMails()) {
                if (winlinkStatus == 0) {
                    menuDisplay = 51;
                } else {
                    menuDisplay = 50100;
                }
                messagesIterator = 0;
                if (Config.notification.ledMessage) {
                    messageLed = false;
                }
            } else {
                menuDisplay = 50101;
            }
        } else if (menuDisplay >= 5061 && menuDisplay <= 5063) {
            menuDisplay++;
            if (menuDisplay > 5063) {
                menuDisplay = 5061;
            }
        } else if (menuDisplay >= 5084 && menuDisplay <= 5085) {
            menuDisplay++;
            if (menuDisplay > 5085) {
                menuDisplay = 5084;
            }
        } 

        else if (menuDisplay >= 60 && menuDisplay <= 63) {
            menuDisplay++;
            if (menuDisplay > 63) {
                menuDisplay = 60;
            } 
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
        } else if ((menuDisplay>=10 && menuDisplay<=13) || (menuDisplay>=20 && menuDisplay<=29) || (menuDisplay == 120) || (menuDisplay>=130 && menuDisplay<=133) || (menuDisplay>=50 && menuDisplay<=53) || (menuDisplay>=200 && menuDisplay<=290) || (menuDisplay>=60 && menuDisplay<=63) || (menuDisplay>=30 && menuDisplay<=31) || (menuDisplay>=300 && menuDisplay<=310) || (menuDisplay == 40)) {
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
            if(myBeaconsIndex >= (myBeaconsSize-1)) {
                myBeaconsIndex = 0;
            } else {
                myBeaconsIndex++;
            }
            displayToggle(true);
            displayTime = millis();
            statusState  = true;
            statusTime = millis();
            winlinkCommentState = false;
            displayShow("__ INFO __", "", "  CHANGING CALLSIGN!", "", "-----> " + Config.beacons[myBeaconsIndex].callsign, "", 2000);
            STATION_Utils::saveIndex(0, myBeaconsIndex);
            if (menuDisplay == 200) {
                menuDisplay = 20;
            }
        } else if ((menuDisplay >= 1 && menuDisplay <= 3) || (menuDisplay >= 11 &&menuDisplay <= 13) || (menuDisplay >= 20 && menuDisplay <= 27) || (menuDisplay >= 30 && menuDisplay <= 31)) {
            menuDisplay = menuDisplay * 10;
        } else if (menuDisplay == 10) {
            MSG_Utils::loadMessagesFromMemory(0);
            if (MSG_Utils::warnNoAPRSMessages()) {
                menuDisplay = 10;
            } else {
                menuDisplay = 100;
            }
        } else if (menuDisplay == 120) {
            MSG_Utils::deleteFile(0);
            displayShow("___INFO___", "", "ALL MESSAGES DELETED!", 2000);
            MSG_Utils::loadNumMessages();
            menuDisplay = 12;
        } else if (menuDisplay == 130) {
            if (keyDetected) {
                menuDisplay = 1300;
            } else {
                displayShow(" APRS Thu.", "Sending:", "Happy #APRSThursday", "from LoRa Tracker 73!", "", "", 2000);
                MSG_Utils::addToOutputBuffer(0, "ANSRVR", "CQ HOTG Happy #APRSThursday from LoRa Tracker 73!");
            }
        } else if (menuDisplay == 131) {
            if (keyDetected) {
                menuDisplay = 1310;
            } else {
                displayShow(" APRS Thu.", "Sending:", "Happy #APRSThursday", "from LoRa Tracker 73!", "", "", 2000);
                MSG_Utils::addToOutputBuffer(0, "APRSPH" , "HOTG Happy #APRSThursday from LoRa Tracker 73!");
            }
        } else if (menuDisplay == 132) {
            displayShow(" APRS Thu.", "", "   Unsubscribe", "   from APRS Thursday", "", "", 2000);
            MSG_Utils::addToOutputBuffer(0, "ANSRVR", "U HOTG");
        } else if (menuDisplay == 133) {
            displayShow(" APRS Thu.", "", "  Keep Subscribed" ,"  for 12hours more", "", "", 2000);
            MSG_Utils::addToOutputBuffer(0, "ANSRVR", "K HOTG");
        }

        else if (menuDisplay == 210) {
            LoRa_Utils::changeFreq();
            STATION_Utils::saveIndex(1, loraIndex);
            menuDisplay = 21;
        } else if (menuDisplay == 220) {
            if (!displayEcoMode) {
                displayEcoMode = true;
                displayShow("_DISPLAY__", "", "   ECO MODE -> ON", 1000);
            } else {
                displayEcoMode = false;
                displayShow("_DISPLAY__", "", "   ECO MODE -> OFF", 1000);
            }
        } else if (menuDisplay == 221) {
            if (screenBrightness ==1) {
                displayShow("_SCREEN___", "", "SCREEN BRIGHTNESS MAX", 1000);
                screenBrightness = 255;   
            } else {
                displayShow("_SCREEN___", "", "SCREEN BRIGHTNESS MIN", 1000);
                screenBrightness = 1;
            }
        } else if (menuDisplay == 240) {
            displayShow("_STATUS___", "", "WRITE STATUS","STILL IN DEVELOPMENT!", "", "", 2000); /////////////////////////
        } else if (menuDisplay == 241) {
            displayShow("_STATUS___", "", "SELECT STATUS","STILL IN DEVELOPMENT!", "", "", 2000); /////////////////////////
        } else if (menuDisplay == 250) {
            displayShow("_NOTIFIC__", "", "NOTIFICATIONS","STILL IN DEVELOPMENT!", "", "", 2000); /////////////////////////
        } 

        else if (menuDisplay == 4) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Loop", "%s", "wrl");
            MSG_Utils::addToOutputBuffer(0, "CA2RXU-15", "wrl");
        }

        else if (menuDisplay == 5) {
            menuDisplay = 50;
        } else if (menuDisplay == 50) {
            WINLINK_Utils::login();
            menuDisplay = 500;
        } else if (menuDisplay == 51) {
            MSG_Utils::loadMessagesFromMemory(1);
            if (MSG_Utils::warnNoWLNKMails()) {
                menuDisplay = 51;
            } else {
                menuDisplay = 50101;
            }
        } else if (menuDisplay == 52) {
            menuDisplay = 50111;
        } else if (menuDisplay == 53) {
            if (winlinkCommentState) {
                winlinkCommentState = false;
                displayShow("_WINLINK_>", "", "  WLNK COMMENTs OFF!", 2000);
            } else {
                winlinkCommentState = true;
                displayShow("_WINLINK_>", "", "  WLNK COMMENTs ON!", 2000);
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
            displayShow("___INFO___", "", " ALL MAILS DELETED!", 2000);
            MSG_Utils::loadNumMessages();
            if (winlinkStatus == 0) {
                menuDisplay = 52;
            } else {
                menuDisplay = 50110;
            }
        } else if (menuDisplay == 5020) {
            menuDisplay = 5021;
        } else if (menuDisplay == 5030) {
            menuDisplay = 5031;
        } else if (menuDisplay == 5040) {
            menuDisplay = 5041;
        } else if (menuDisplay == 5050) {
            menuDisplay = 5051;
        } else if (menuDisplay == 5060) {
            menuDisplay = 5061;
        } else if (menuDisplay == 5061) {
            menuDisplay = 50610;
        } else if (menuDisplay == 5061) {
            menuDisplay = 50610;
        } else if (menuDisplay == 5062) {
            menuDisplay = 50620;
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

        else if (menuDisplay == 6) {
            menuDisplay = 60;
        } else if (menuDisplay == 60) {
            if (Config.notification.ledFlashlight) {
                if (flashlight) {
                    displayShow("__EXTRAS__", "","     Flashlight","   Status --> OFF", "", "", 2000);
                    flashlight = false;
                } else {
                    displayShow("__EXTRAS__", "","     Flashlight","   Status --> ON", "", "", 2000);
                    flashlight = true;
                }
            } else {
                displayShow("__EXTRAS__", "","     Flashlight","NOT ACTIVE IN CONFIG!", "", "", 2000);
            }
        } else if (menuDisplay == 61) {
            if (digirepeaterActive) {
                displayShow("__EXTRAS__", "","   DigiRepeater","   Status --> OFF", "", "", 2000);
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "Main", "%s", "DigiRepeater OFF");
                digirepeaterActive = false;
            } else {
                displayShow("__EXTRAS__", "","   DigiRepeater","   Status --> ON","", "", 2000);
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "Main", "%s", "DigiRepeater ON");
                digirepeaterActive = true;
            }
        } else if (menuDisplay == 62) {
            if (sosActive) {
                displayShow("__EXTRAS__", "","       S.O.S.","   Status --> OFF", "", "", 2000);
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "Main", "%s", "S.O.S Mode OFF");
                sosActive = false;
            } else {
                displayShow("__EXTRAS__", "","       S.O.S.","   Status --> ON", "", "", 2000);
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "Main", "%s", "S.O.S Mode ON");
                sosActive = true;
            }
        } else if (menuDisplay == 63) {
            menuDisplay = 630;
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
                messageCallsign = messageCallsign.substring(0, messageCallsign.length() - 1);
            }
            messageCallsign.toUpperCase();
        } else if ((menuDisplay == 111 || menuDisplay == 1300 || menuDisplay == 1310) && key!= 180) {     // Writting Text of Message
            if (messageText.length() == 1) {
                messageText.trim();
            }
            if (key >= 32 && key <= 126) {
                messageText += key;
            } else if (key == 13 && messageText.length() > 0) {                         // Return Pressed: SENDING MESSAGE
                messageText.trim();
                if (messageText.length() > 67) {
                    messageText = messageText.substring(0, 67);
                }
                if (menuDisplay == 111) {
                    MSG_Utils::addToOutputBuffer(0, messageCallsign, messageText);
                    menuDisplay = 11;
                } else if (menuDisplay == 1300) {
                    messageCallsign = "ANSRVR";
                    MSG_Utils::addToOutputBuffer(0, messageCallsign, "CQ HOTG " + messageText);
                    menuDisplay = 130;
                } else if (menuDisplay == 1310) {
                    messageCallsign = "APRSPH";
                    MSG_Utils::addToOutputBuffer(0, messageCallsign, "HOTG " + messageText);
                    menuDisplay = 131;
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
            POWER_Utils::shutdown();
            #else
            displayShow("", "", "ESP32 CAN'T POWER OFF", 2000);
            #endif
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
            if (winlinkAddressee.length() == 1) {
                winlinkAddressee.trim();
            }
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
            if (winlinkAlias.length() == 1) {
                winlinkAlias.trim();
            }
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
            if (winlinkAliasComplete.length() == 1) {
                winlinkAliasComplete.trim();
            }
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
            if (winlinkAlias.length() == 1) {
                winlinkAlias.trim();
            }
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
            if (winlinkAddressee.length() == 1) {
                winlinkAddressee.trim();
            }
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
            if (winlinkSubject.length() == 1) {
                winlinkSubject.trim();
            }
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
            if (winlinkBody.length() == 1) {
                winlinkBody.trim();
            }
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
            if (messageText.length() == 1) {
                messageText.trim();
            }
            if (key >= 32 && key <= 126) {
                messageText += key;
            } else if (key == 13 && messageText.length() > 0) {
                messageText.trim();
                if (messageText.length() > 67) {
                    messageText = messageText.substring(0, 67);
                }
                String packet = APRSPacketLib::generateGPSBeaconPacket(currentBeacon->callsign, "APLRT1", Config.path, currentBeacon->overlay, APRSPacketLib::encodeGPS(gps.location.lat(),gps.location.lng(), gps.course.deg(), gps.speed.knots(), currentBeacon->symbol, Config.sendAltitude, gps.altitude.feet(), sendStandingUpdate, "GPS"));
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

    void clearTrackballCounter() {
        upCounter       = 0;
        downCounter     = 0;
        leftCounter     = 0;
        rightCounter    = 0;
    }

    void mouseRead() {
        #ifdef TTGO_T_DECK_GPS
            int ballUp      = digitalRead(TrackBallUp);
            int ballDown    = digitalRead(TrackBallDown);
            int ballLeft    = digitalRead(TrackBallLeft);
            int ballRight   = digitalRead(TrackBallRight);

            if (!digitalRead(TrackBallCenter)) {
                processPressedKey(13);
            } else if (ballUp != mouseUpState && ballDown == mouseDownState && ballLeft == mouseLeftState && ballRight == mouseRightState) {
                if (millis() - lastDebounceTime > debounceInterval) {
                    lastDebounceTime = millis();
                    mouseUpState = ballUp;
                    upCounter++;
                }
            } else if (ballDown != mouseDownState && ballUp == mouseUpState && ballLeft == mouseLeftState && ballRight == mouseRightState) {
                if (millis() - lastDebounceTime > debounceInterval) {
                    lastDebounceTime = millis();
                    mouseDownState = ballDown;
                    downCounter++;
                }
            } else if (ballLeft != mouseLeftState && ballUp == mouseUpState && ballDown == mouseDownState && ballRight == mouseRightState) {
                if (millis() - lastDebounceTime > debounceInterval) {
                    lastDebounceTime = millis();
                    mouseLeftState = ballLeft;
                    leftCounter++;
                }
            } else if (ballRight != mouseRightState && ballUp == mouseUpState && ballDown == mouseDownState && ballLeft == mouseLeftState) {
                if (millis() - lastDebounceTime > debounceInterval) {
                    lastDebounceTime = millis();
                    mouseRightState = ballRight;
                    rightCounter++;
                }
            }
            if (upCounter == trackBallSensitivity) {
                clearTrackballCounter();
                upArrow();
            } else if (downCounter == trackBallSensitivity) {
                clearTrackballCounter();
                downArrow();
            } else if (leftCounter == trackBallSensitivity) {
                clearTrackballCounter();
                leftArrow();
            } else if (rightCounter == trackBallSensitivity) {
                clearTrackballCounter();
                rightArrow();
            }
        #endif
    }

    void read() {
        if (keyboardConnected) {
            uint32_t lastKey = millis() - keyboardTime;
            if (lastKey > 30*1000) {
                keyDetected = false;
            }
            Wire.requestFrom(KB_ADDR, 1);
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
    }

    void setup() {
        Wire.beginTransmission(KB_ADDR);
        if (Wire.endTransmission() == 0) {
            keyboardConnected = true;
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Keyboard Connected to I2C");
        } else {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Main", "No Keyboard Connected to I2C");
        }
    }

}