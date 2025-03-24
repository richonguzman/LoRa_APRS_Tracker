#include <APRSPacketLib.h>
#include <TinyGPS++.h>
#include <vector>
#include "notification_utils.h"
#include "custom_characters.h"
#include "station_utils.h"
#include "configuration.h"
#include "battery_utils.h"
#include "board_pinout.h"
#include "power_utils.h"
#include "menu_utils.h"
#include "msg_utils.h"
#include "gps_utils.h"
#include "wx_utils.h"
#include "display.h"
#include "utils.h"


extern int                  menuDisplay;
extern Beacon               *currentBeacon;
extern Configuration        Config;
extern TinyGPSPlus          gps;
extern std::vector<String>  loadedAPRSMessages;
extern std::vector<String>  loadedWLNKMails;
extern int                  messagesIterator;
extern uint8_t              loraIndex;
extern uint32_t             menuTime;
extern bool                 symbolAvailable;
extern bool                 keyDetected;
extern String               messageCallsign;
extern String               messageText;
extern bool                 flashlight;
extern bool                 digipeaterActive;
extern bool                 sosActive;
extern bool                 bluetoothActive;
extern bool                 displayEcoMode;
extern bool                 screenBrightness;
extern bool                 disableGPS;
extern bool                 showHumanHeading;
extern APRSPacket           lastReceivedPacket;

extern uint8_t              winlinkStatus;
extern String               winlinkMailNumber;
extern String               winlinkAddressee;
extern String               winlinkSubject;
extern String               winlinkBody;
extern String               winlinkAlias;
extern String               winlinkAliasComplete;
extern bool                 winlinkCommentState;
extern int                  wxModuleType;
extern bool                 gpsIsActive;

String      freqChangeWarning;
uint8_t     lowBatteryPercent       = 21;

#if defined(TTGO_T_DECK_PLUS) || defined(TTGO_T_DECK_GPS)
    String topHeader1   = "";
    String topHeader1_1 = "";
    String topHeader1_2 = "";
    String topHeader1_3 = "";
    String topHeader2   = "";
#endif


namespace MENU_Utils {

    const String checkBTType() {
        if (Config.bluetooth.useBLE) {
            return "BLE";
        } else {
            return "BT Classic";
        }
    }

    const String checkProcessActive(const bool process) {
        if (process) {
            return "ON";
        } else {
            return "OFF";
        }
    }

    const String screenBrightnessAsString(const uint8_t bright) {
        #ifdef HAS_TFT
            if (bright == 255) {
                return "Max";
            } else if (bright == 70) {
                return "Low";
            } else {
                return "Mid";
            }
        #else
            if (bright == 255) {
                return "Max";
            } else if (bright == 1) {
                return "Low";
            } else {
                return "Mid";
            }
        #endif
    }

    void showOnScreen() {
        String lastLine;
        uint32_t lastMenuTime = millis() - menuTime;
        if (!(menuDisplay==0) && !(menuDisplay==400) && !(menuDisplay==410) && !(menuDisplay==300) && !(menuDisplay>=500 && menuDisplay<=5100) && lastMenuTime > 30*1000) {
            menuDisplay     = 0;
            messageCallsign = "";
            messageText     = "";
        }
        if (keyDetected) {
            lastLine = "<Back Up/Down Select>";
        } else {
            lastLine = "1P=Down 2P=Back LP=Go";
        }

        #if defined(TTGO_T_DECK_PLUS) || defined(TTGO_T_DECK_GPS)
            topHeader1      = currentBeacon->callsign;
            const auto time_now = now();
            topHeader1_1    = Utils::createDateString(time_now);
            topHeader1_2    = Utils::createTimeString(time_now);
            topHeader1_3    = "";
            topHeader2      = String(gps.location.lat(), 4);
            topHeader2      += " ";
            topHeader2      += String(gps.location.lng(), 4);

            for (int i = topHeader2.length(); i < 19; i++) {
                topHeader2 += " ";
            }
            if (gps.satellites.value() <= 9) topHeader2 += " ";
            topHeader2 += "SAT:";
            topHeader2 += String(gps.satellites.value());
            if (gps.hdop.hdop() > 5) {
                topHeader2 += "X";
            } else if (gps.hdop.hdop() > 2 && gps.hdop.hdop() < 5) {
                topHeader2 += "-";
            } else if (gps.hdop.hdop() <= 2) {
                topHeader2 += "+";
            }
        #endif

        switch (menuDisplay) { // Graphic Menu is in here!!!!
            case 1:     // 1. Messages
                displayShow("<< MENU >>","  6.Extras", "> 1.Messages", "  2.Configuration", "  3.Reports", lastLine);
                break;
            case 2:     // 2. Configuration
                displayShow("<< MENU >>", "  1.Messages", "> 2.Configuration", "  3.Reports", "  4.Stations", lastLine);
                break;
            case 3:     //3. Reports
                displayShow("<< MENU >>", "  2.Configuration", "> 3.Reports", "  4.Stations", "  5.Winlink/Mail", lastLine);
                break;
            case 4:     //4. Stations
                displayShow("<< MENU >>", "  3.Reports", "> 4.Stations", "  5.Winlink/Mail", "  6.Extras", lastLine);
                break;
            case 5:     //5. Winlink
                displayShow("<< MENU >>", "  4.Stations", "> 5.Winlink/Mail", "  6.Extras", "  1.Messages", lastLine);
                break;
            case 6:     //6. Extras
                displayShow("<< MENU >>", "  5.Winlink/Mail", "> 6.Extras", "  1.Messages", "  2.Configuration", lastLine);
                break;

//////////
            case 10:    // 1.Messages ---> Messages Read
                displayShow(" MESSAGES>", "> Read (" + String(MSG_Utils::getNumAPRSMessages()) + ")", "  Write", "  Delete", "  APRSThursday", lastLine);
                break;
            case 100:   // 1.Messages ---> Messages Read ---> Display Received/Saved APRS Messages
                {
                    String msgSender    = loadedAPRSMessages[messagesIterator].substring(0, loadedAPRSMessages[messagesIterator].indexOf(","));
                    String msgText      = loadedAPRSMessages[messagesIterator].substring(loadedAPRSMessages[messagesIterator].indexOf(",") + 1);

                    #ifdef HAS_TFT
                        displayMessage(msgSender, msgText, true);
                    #else
                        displayShow(" MSG APRS>", "From --> " + msgSender, msgText, "", "", "           Next=Down");
                    #endif
                }
                break;
            case 11:    // 1.Messages ---> Messages Write
                displayShow(" MESSAGES>", "  Read (" + String(MSG_Utils::getNumAPRSMessages()) + ")", "> Write", "  Delete", "  APRSThursday", lastLine);
                break;
            case 110:   // 1.Messages ---> Messages Write ---> Write
                if (keyDetected) {
                    displayShow("WRITE MSG>", "", "CALLSIGN = " + String(messageCallsign), "", "", "<Back          Enter>");
                } else {
                    displayShow("WRITE MSG>", "", "No Keyboard Detected", "Can't write Message", "", "1P = Back");           
                }     
                break;
            case 111:
                if (messageText.length() <= 67) {
                    if (messageText.length() < 10) {
                        displayShow("WRITE MSG>", "CALLSIGN -> " + messageCallsign, "MSG -> " + messageText, "", "", "<Back   (0" + String(messageText.length()) + ")   Enter>");
                    } else {
                        displayShow("WRITE MSG>", "CALLSIGN -> " + messageCallsign, "MSG -> " + messageText, "", "", "<Back   (" + String(messageText.length()) + ")   Enter>");
                    }     
                } else {
                    displayShow("WRITE MSG>", "--- MSG TOO LONG! ---", " -> " + messageText, "", "", "<Back   (" + String(messageText.length()) + ")");
                }
                break;
            case 12:    // 1.Messages ---> Messages Delete
                displayShow(" MESSAGES>", "  Read (" + String(MSG_Utils::getNumAPRSMessages()) + ")", "  Write", "> Delete", "  APRSThursday", lastLine);
                break;
            case 120:   // 1.Messages ---> Messages Delete ---> Delete: ALL
                displayShow("DELETE MSG", "", "  DELETE APRS MSG?", "", "", " Confirm = LP or '>'");
                break;
            case 13:    // 1.Messages ---> APRSThursday
                displayShow(" MESSAGES>", "  Read (" + String(MSG_Utils::getNumAPRSMessages()) + ")", "  Write", "  Delete", "> APRSThursday", lastLine);
                break;
            case 130:   // 1.Messages ---> APRSThursday ---> Delete: ALL
                displayShow(" APRS Thu.", "> Check In", "  Join", "  Unsubscribe", "  KeepSubscribed+12h", lastLine);
                break;
            case 1300:
                if (messageText.length() <= 67) {
                    if (messageText.length() < 10) {
                        displayShow("WRITE MSG>", "  - APRSThursday -", "MSG -> " + messageText, "", "", "<Back   (0" + String(messageText.length()) + ")   Enter>");
                    } else {
                        displayShow("WRITE MSG>", "  - APRSThursday -", "MSG -> " + messageText, "", "", "<Back   (" + String(messageText.length()) + ")   Enter>");
                    }     
                } else {
                    displayShow("WRITE MSG>", "--- MSG TOO LONG! ---", " -> " + messageText, "", "", "<Back   (" + String(messageText.length()) + ")");
                }
                break;
            case 131:   // 1.Messages ---> APRSThursday ---> Delete: ALL
                displayShow(" APRS Thu.", "  Check In", "> Join", "  Unsubscribe", "  KeepSubscribed+12h", lastLine);
                break;
            case 1310:
                if (messageText.length() <= 67) {
                    if (messageText.length() < 10) {
                        displayShow("WRITE MSG>", "  - APRSThursday -", "MSG -> " + messageText, "", "", "<Back   (0" + String(messageText.length()) + ")   Enter>");
                    } else {
                        displayShow("WRITE MSG>", "  - APRSThursday -", "MSG -> " + messageText, "", "", "<Back   (" + String(messageText.length()) + ")   Enter>");
                    }     
                } else {
                    displayShow("WRITE MSG>", "--- MSG TOO LONG! ---", " -> " + messageText, "", "", "<Back   (" + String(messageText.length()) + ")");
                }
                break;
            case 132:   // 1.Messages ---> APRSThursday ---> Delete: ALL
                displayShow(" APRS Thu.", "  Check In", "  Join", "> Unsubscribe", "  KeepSubscribed+12h", lastLine);
                break;
            case 133:   // 1.Messages ---> APRSThursday ---> Delete: ALL
                displayShow(" APRS Thu.", "  Check In", "  Join", "  Unsubscribe", "> KeepSubscribed+12h", lastLine);
                break;

//////////            
            case 20:    // 2.Configuration ---> Callsign
                displayShow(" CONFIG>", "  Power Off", "> Change Callsign ", "  Change Frequency", "  Display",lastLine);
                break;
            case 21:    // 2.Configuration ---> Change Freq
                displayShow(" CONFIG>", "  Change Callsign ", "> Change Frequency", "  Display", "  " + checkBTType() + " (" + checkProcessActive(bluetoothActive) + ")",lastLine);
                break;
            case 22:    // 2.Configuration ---> Display
                displayShow(" CONFIG>", "  Change Frequency", "> Display", "  " + checkBTType() + " (" + checkProcessActive(bluetoothActive) + ")", "  Status",lastLine);
                break;
            case 23:    // 2.Configuration ---> Bluetooth
                displayShow(" CONFIG>", "  Display",  "> " + checkBTType() + " (" + checkProcessActive(bluetoothActive) + ")", "  Status", "  Notifications", lastLine);
                break;
            case 24:    // 2.Configuration ---> Status
                displayShow(" CONFIG>", "  " + checkBTType() + " (" + checkProcessActive(bluetoothActive) + ")", "> Status","  Notifications", "  Reboot",lastLine);
                break;
            case 25:    // 2.Configuration ---> Notifications
                displayShow(" CONFIG>", "  Status", "> Notifications", "  Reboot", "  Power Off",lastLine);
                break;
            case 26:    // 2.Configuration ---> Reboot
                displayShow(" CONFIG>", "  Notifications", "> Reboot", "  Power Off", "  Change Callsign",lastLine);
                break;
            case 27:    // 2.Configuration ---> Power Off
                displayShow(" CONFIG>", "  Reboot", "> Power Off", "  Change Callsign", "  Change Frequency",lastLine);
                break;


            case 200:   // 2.Configuration ---> Change Callsign
                displayShow(" CALLSIGN>", "","  Confirm Change?","","","<Back         Select>");
                break;

            case 210:   // 2.Configuration ---> Change Frequency
                switch (loraIndex) {
                    case 0: freqChangeWarning = "      Eu --> PL"; break;
                    case 1: freqChangeWarning = "      PL --> UK"; break;
                    case 2: freqChangeWarning = "      UK --> Eu"; break;
                }
                displayShow("LORA FREQ>", "","   Confirm Change?", freqChangeWarning, "", "<Back         Select>");
                break;

            case 220:   // 2.Configuration ---> Display ---> ECO Mode
                displayShow(" DISPLAY>", "", "> ECO Mode    (" + checkProcessActive(displayEcoMode) + ")", "  Brightness  (" + screenBrightnessAsString(screenBrightness) + ")","",lastLine);
                break;

            case 221:   // 2.Configuration ---> Display ---> Brightness
                displayShow(" DISPLAY>", "", "  ECO Mode    (" + checkProcessActive(displayEcoMode) + ")", "> Brightness  (" + screenBrightnessAsString(screenBrightness) + ")","",lastLine);
                break;
            case 2210:   // 2.Configuration ---> Display ---> Brightness: MIN
                displayShow("BRIGHTNESS", "", "> Low", "  Mid","  Max",lastLine);
                break;
            case 2211:   // 2.Configuration ---> Display ---> Brightness: MID
                displayShow("BRIGHTNESS", "", "  Low", "> Mid","  Max",lastLine);
                break;
            case 2212:   // 2.Configuration ---> Display ---> Brightness: MAX
                displayShow("BRIGHTNESS", "", "  Low", "  Mid","> Max",lastLine);
                break;

            case 230:
                if (bluetoothActive) {
                    bluetoothActive = false;
                    displayShow("BLUETOOTH>", "", " Bluetooth --> OFF", 1000);
                } else {
                    bluetoothActive = true;
                    displayShow("BLUETOOTH>", "", " Bluetooth --> ON", 1000);
                }
                menuDisplay = 23;
                break;

            case 240:    // 2.Configuration ---> Status
                displayShow(" STATUS>", "", "> Write","  Select","",lastLine);
                break;
            case 241:    // 2.Configuration ---> Status
                displayShow(" STATUS>", "", "  Write","> Select","",lastLine);
                break;

            case 250:    // 2.Configuration ---> Notifications
                displayShow(" NOTIFIC>", "> Turn Off Sound/Led","","","",lastLine);
                break;

            case 260:   // 2.Configuration ---> Reboot
                if (keyDetected) {
                    displayShow(" REBOOT?", "","Confirm Reboot...","","","<Back   Enter=Confirm");
                } else {
                    displayShow(" REBOOT?", "no Keyboard Detected"," Use RST Button to","Reboot Tracker","",lastLine);
                }
                break;
            case 270:   // 2.Configuration ---> Power Off
                if (keyDetected) {
                    displayShow("POWER OFF?", "","Confirm Power Off...","","","<Back   Enter=Confirm");
                } else {
                    displayShow("POWER OFF?", "no Keyboard Detected"," Use PWR Button to","Power Off Tracker","",lastLine);
                }
                break;

//////////
            case 30:     // 3. Reports : Wx Report
                displayShow(" REPORTS >","> 1.Wx Report", "  2.Hospital QTH", "  3.Police QTH", "  4.Fire Station QTH", lastLine);
                break;
            case 31:     // 3. Reports : Nearest Hospital
                displayShow(" REPORTS >","  1.Wx Report", "> 2.Hospital QTH", "  3.Police QTH", "  4.Fire Station QTH", lastLine);
                break;
            case 32:     // 3. Reports : Nearest Police Station
                displayShow(" REPORTS >","  1.Wx Report", "  2.Hospital QTH", "> 3.Police QTH", "  4.Fire Station QTH", lastLine);
                break;
            case 33:     // 3. Reports : Nearest Fire Station
                displayShow(" REPORTS >","  1.Wx Report", "  2.Hospital QTH", "  3.Police QTH", "> 4.Fire Station QTH", lastLine);
                break;
            
            case 300:
                // waiting for Report
                break;

//////////
            case 40:    //3.Stations ---> Packet Decoder
                displayShow(" STATIONS>", "", "> Packet Decoder", "  Near By Stations", "", "<Back");
                break;
            case 41:    //3.Stations ---> Near By Stations
                displayShow(" STATIONS>", "", "  Packet Decoder", "> Near By Stations", "", "<Back");
                break;

            case 400:   //3.Stations ---> Packet Decoder
                if (lastReceivedPacket.sender != currentBeacon->callsign) {
                    String firstLineDecoder = lastReceivedPacket.sender;
                    for (int i = firstLineDecoder.length(); i < 9; i++) {
                        firstLineDecoder += ' ';
                    }
                    firstLineDecoder += lastReceivedPacket.symbol;

                    if (lastReceivedPacket.type == 0 || lastReceivedPacket.type == 4) {      // gps and Mic-E gps

                        char bufferCourseSpeedAltitude[24];
                        sprintf(bufferCourseSpeedAltitude, "A=%04dm %3dkm/h %3d", lastReceivedPacket.altitude, lastReceivedPacket.speed, lastReceivedPacket.course);
                        String courseSpeedAltitude = String(bufferCourseSpeedAltitude);

                        double distanceKm = TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(), lastReceivedPacket.latitude, lastReceivedPacket.longitude) / 1000.0;
                        double courseTo   = TinyGPSPlus::courseTo(gps.location.lat(), gps.location.lng(), lastReceivedPacket.latitude, lastReceivedPacket.longitude);
                        
                        String pathDec = (lastReceivedPacket.path.length() > 14) ? "P:" : "PATH:  ";
                        pathDec += lastReceivedPacket.path;

                        displayShow(firstLineDecoder, "GPS " + String(lastReceivedPacket.latitude,3) + " " + String(lastReceivedPacket.longitude,3), courseSpeedAltitude, "D:" + String(distanceKm) + "km    " + String(courseTo,0), pathDec, "< RSSI:" + String(lastReceivedPacket.rssi) + " SNR:" + String(lastReceivedPacket.snr));
                    } else if (lastReceivedPacket.type == 1) {    // message
                        displayShow(firstLineDecoder, "ADDRESSEE: " + lastReceivedPacket.addressee, "MSG:  " + lastReceivedPacket.payload, "", "", "< RSSI:" + String(lastReceivedPacket.rssi) + " SNR:" + String(lastReceivedPacket.snr));
                    } else if (lastReceivedPacket.type == 2) {    // status
                        displayShow(firstLineDecoder, "-------STATUS-------", lastReceivedPacket.payload, "", "", "< RSSI:" + String(lastReceivedPacket.rssi) + " SNR:" + String(lastReceivedPacket.snr));
                    } else if (lastReceivedPacket.type == 3) {    // telemetry
                        displayShow(firstLineDecoder, "------TELEMETRY------", "", "", "", "< RSSI:" + String(lastReceivedPacket.rssi) + " SNR:" + String(lastReceivedPacket.snr));
                    } else if (lastReceivedPacket.type == 5) {    // object
                        displayShow(firstLineDecoder, "-------OBJECT-------", "", "", "", "< RSSI:" + String(lastReceivedPacket.rssi) + " SNR:" + String(lastReceivedPacket.snr));
                    }
                }
                break;
            case 410:    //3.Stations ---> Near By Stations
                displayShow(" NEAR BY>", STATION_Utils::getNearTracker(0), STATION_Utils::getNearTracker(1), STATION_Utils::getNearTracker(2), STATION_Utils::getNearTracker(3), "<Back");
                break;

//////////
            case 50:    // 5.Winlink MENU
                if (winlinkStatus == 5) {
                    menuDisplay = 5000;
                } else {
                    displayShow(" WINLINK>", "> Login" , "  Read SavedMails(" + String(MSG_Utils::getNumWLNKMails()) + ")", "  Delete SavedMails", "  Wnlk Comment (" + checkProcessActive(winlinkCommentState) + ")" , lastLine);
                }
                break;
            case 51:    // 5.Winlink
                displayShow(" WINLINK>", "  Login" , "> Read SavedMails(" + String(MSG_Utils::getNumWLNKMails()) + ")", "  Delete SavedMails", "  Wnlk Comment (" + checkProcessActive(winlinkCommentState) + ")" , lastLine);
                break;
            case 52:    // 5.Winlink
                displayShow(" WINLINK>", "  Login" , "  Read SavedMails(" + String(MSG_Utils::getNumWLNKMails()) + ")", "> Delete SavedMails", "  Wnlk Comment (" + checkProcessActive(winlinkCommentState) + ")" , lastLine);
                break;
            case 53:    // 5.Winlink
                displayShow(" WINLINK>", "  Login" , "  Read SavedMails(" + String(MSG_Utils::getNumWLNKMails()) + ")", "  Delete SavedMails", "> Wnlk Comment (" + checkProcessActive(winlinkCommentState) + ")" , lastLine);
                break;

            case 500:    // 5.Winlink ---> Login
                displayShow(" WINLINK>", "" , "Login Initiation ...", "Challenge -> waiting", "" , "");
                break;
            case 501:    // 5.Winlink ---> Login
                displayShow(" WINLINK>", "" , "Login Initiation ...", "Challenge -> sent", "" , "");
                break;
            case 502:    // 5.Winlink ---> Login
                displayShow(" WINLINK>", "" , "Login Initiation ...", "Challenge -> ack ...", "" , "");
                break;

            case 5000:   // WINLINK: List Pend. Mail //
                displayShow("WLNK MENU>", "  Write Mail" , "> List Pend. Mails", "  Downloaded Mails", "  Read Mail    (R#)", lastLine);
                break;

            case 5010:    // WINLINK: Downloaded Mails //
                displayShow("WLNK MENU>", "  List Pend. Mails", "> Downloaded Mails", "  Read Mail    (R#)", "  Reply Mail   (Y#)", lastLine);
                break;
            case 50100:    // WINLINK: Downloaded Mails //
                displayShow(" WINLINK>", "" , "> Read SavedMails(" + String(MSG_Utils::getNumWLNKMails()) + ")", "  Delete SavedMails", "" , lastLine);
                break;
            case 50101:    // WINLINK: Downloaded Mails //
                {
                    String mailText = loadedWLNKMails[messagesIterator];

                    #ifdef HAS_TFT
                        displayMessage("WLNK MAIL>", mailText, true);
                    #else
                        displayShow("WLNK MAIL>", "", mailText, "", "", "           Next=Down");
                    #endif
                }
                break;
            case 50110:    // WINLINK: Downloaded Mails //
                displayShow(" WINLINK>", "" , "  Read SavedMails(" + String(MSG_Utils::getNumWLNKMails()) + ")", "> Delete SavedMails", "" , lastLine);
                break;
            case 50111:    // WINLINK: Downloaded Mails //
                displayShow("WLNK DEL>", "", "  DELETE ALL MAILS?", "", "", " Confirm = LP or '>'");
                break;

            case 5020:    // WINLINK: Read Mail //
                displayShow("WLNK MENU>", "  Downloaded Mails", "> Read Mail    (R#)", "  Reply Mail   (Y#)", "  Forward Mail (F#)", lastLine);
                break;
            case 5021:
                displayShow("WLNK READ>", "", "    READ MAIL N." + winlinkMailNumber, "", "", "<Back          Enter>");
                break;

            case 5030:    // WINLINK: Reply Mail //
                displayShow("WLNK MENU>", "  Read Mail    (R#)", "> Reply Mail   (Y#)", "  Forward Mail (F#)", "  Delete Mail  (K#)", lastLine);
                break;
            case 5031:
                displayShow("WLNK REPLY", "", "   REPLY MAIL N." + winlinkMailNumber , "", "", "<Back          Enter>");
                break;

            case 5040:    // WINLINK: Foward Mail //
                displayShow("WLNK MENU>", "  Reply Mail   (Y#)", "> Forward Mail (F#)", "  Delete Mail  (K#)", "  Alias Menu", lastLine);
                break;
            case 5041:    // WINLINK: Forward Mail //
                displayShow("WLNK FORW>", "", "  FORWARD MAIL N." + winlinkMailNumber , "", "", "<Back          Enter>");
                break;
            case 5042:    // WINLINK: Forward Mail //
                displayShow("WLNK FORW>", "  FORWARD MAIL N." + winlinkMailNumber , "To = " + winlinkAddressee, "", "", "<Back          Enter>");
                break;

            case 5050:    // WINLINK: Delete Mail //
                displayShow("WLNK MENU>", "  Forward Mail (F#)", "> Delete Mail  (K#)", "  Alias Menu", "  Log Out", lastLine);
                break;
            case 5051:    // WINLINK: Delete Mail //
                displayShow("WLNK DEL>", "", "   DELETE MAIL N."  + winlinkMailNumber, "", "", "<Back          Enter>");
                break;
            
            case 5060:    // WINLINK: Alias Menu //
                displayShow("WLNK MENU>", "  Delete Mail  (K#)", "> Alias Menu", "  Log Out", "  Write Mail", lastLine);
                break;
            case 5061:    // WINLINK: Alias Menu : Create Alias //
                displayShow("WLNK ALIAS", "> Create Alias" , "  Delete Alias ", "  List All Alias", "", lastLine);
                break;
            case 50610:   // WINLINK: Alias Menu : Create Alias //
                displayShow("WLNK ALIAS", "", "Write Alias to Create", "     -> " + winlinkAlias, "", "<Back          Enter>");
                break;
            case 50611:   // WINLINK: Alias Menu : Create Alias //
                displayShow("WLNK ALIAS", "", "      " + winlinkAlias + " =", winlinkAliasComplete, "", "<Back          Enter>");
                break;
            case 5062:    // WINLINK: Alias Menu : Delete Alias //
                displayShow("WLNK ALIAS", "  Create Alias" , "> Delete Alias ", "  List All Alias", "", lastLine);
                break;
            case 50620:   // WINLINK: Alias Menu : Delete Alias //
                displayShow("WLNK ALIAS", "Write Alias to Delete", "", "     -> " + winlinkAlias, "", "<Back          Enter>");
                break;
            case 5063:    // WINLINK: Alias Menu : List Alias//
                displayShow("WLNK ALIAS", "  Create Alias" , "  Delete Alias ", "> List All Alias", "", lastLine);
                break;

            case 5070:    // WINLINK: Log Out MAIL //
                displayShow("WLNK MENU>", "  Alias Menu", "> Log Out", "  Write Mail", "  List Pend. Mails", lastLine);
                break;

            case 5080:    // WINLINK: WRITE MAIL //
                displayShow("WLNK MENU>", "  Log Out", "> Write Mail", "  List Pend. Mails", "  Downloaded Mails", lastLine);
                break;
            case 5081:    // WINLINK: WRITE MAIL: Addressee //
                displayShow("WLNK MAIL>", "--- Send Mail to ---", "", "-> " + winlinkAddressee, "", "<Back          Enter>");
                break;
            case 5082:    // WINLINK: WRITE MAIL: Subject //
                displayShow("WLNK MAIL>", "--- Write Subject ---", "", "-> " + winlinkSubject, "", "<Back          Enter>");
                break;
            case 5083:    // WINLINK: WRITE MAIL: Body //
                if (winlinkBody.length() <= 67) {
                    displayShow("WLNK MAIL>", "-- Body (lenght=" + String(winlinkBody.length()) + ")", "-> " + winlinkBody, "", "", "<Clear Body    Enter>");
                } else {
                    displayShow("WLNK MAIL>", "-- Body Too Long = " + String(winlinkBody.length()), "-> " + winlinkBody, "", "", "<Clear Body");
                }
                break;
            case 5084:    // WINLINK: WRITE MAIL: End Mail? //
                displayShow("WLNK MAIL>", "", "> End Mail", "  1 More Line", "", "      Up/Down Select>");
                break;
            case 5085:    // WINLINK: WRITE MAIL: One More Line(Body) //
                displayShow("WLNK MAIL>", "", "  End Mail", "> 1 More Line", "", "      Up/Down Select>");
                break;

                // validar winlinkStatus = 0
                // check si no esta logeado o si

//////////
            case 60:    // 6. Extras ---> Send Email with GPS info
                displayShow(" EXTRAS>", "  Flashlight    (" + checkProcessActive(flashlight) + ")", "> Send Email(GPS)", "  Digipeater    (" + checkProcessActive(digipeaterActive) + ")", "  S.O.S.        (" + checkProcessActive(sosActive) + ")", lastLine);
                break;
            case 61:    // 6. Extras ---> Digipeater
                displayShow(" EXTRAS>", "  Send Email(GPS)", "> Digipeater    (" + checkProcessActive(digipeaterActive) + ")", "  S.O.S.        (" + checkProcessActive(sosActive) + ")", "  Beacon(GPS) + Comment", lastLine);
                break;
            case 62:    // 6. Extras ---> S.O.S.
                displayShow(" EXTRAS>", "  Digipeater    (" + checkProcessActive(digipeaterActive) + ")", "> S.O.S.        (" + checkProcessActive(sosActive) + ")", "  Beacon(GPS)+Comment", "  Flashlight    (" + checkProcessActive(flashlight) + ")", lastLine);
                break;
            case 63:    // 6. Extras ---> Beacon(GPS) + Comment
                displayShow(" EXTRAS>", "  S.O.S.        (" + checkProcessActive(sosActive) + ")", "> Beacon(GPS)+Comment", "  Flashlight    (" + checkProcessActive(flashlight) + ")", "  Send Email(GPS)", lastLine);
                break;
            case 64:    // 6. Extras ---> Flashlight
                displayShow(" EXTRAS>", "  Beacon(GPS)+Comment", "> Flashlight    (" + checkProcessActive(flashlight) + ")", "  Send Email(GPS)", "  Digipeater    (" + checkProcessActive(digipeaterActive) + ")", lastLine);
                break;

            case 630:
                if (keyDetected) {
                    if (messageText.length() <= 67) {
                        String lengthStr = (messageText.length() < 10) ? "0" + String(messageText.length()) : String(messageText.length());
                        displayShow(" COMMENT>", "Send this Comment in", "the next GPS Beacon :", messageText, "", "<Back   (" + lengthStr + ")   Enter>");
                    } else {
                        displayShow(" COMMENT>", "Comment is too long! ", "Comment too long:" + messageText, "", "", "<Back   (" + String(messageText.length()) + ")>");
                    }
                } else {
                    displayShow(" COMMENT>", "No Keyboard Detected", "", "", "", lastLine);
                }
                break;

//////////
            case 9000:  //  9. multiPress Menu ---> Turn ON WiFi AP
                displayShow(" CONFIG>", "> Turn Tracker Off","  Config. WiFi AP",  "","",lastLine);
                break;
            case 9001:  //  9. multiPress Menu
                displayShow(" CONFIG>", "  Turn Tracker Off","> Config. WiFi AP",  "","",lastLine);
                break;


//////////
            case 0:       ///////////// MAIN MENU //////////////
                String hdopState, firstRowMainMenu, secondRowMainMenu, thirdRowMainMenu, fourthRowMainMenu, fifthRowMainMenu, sixthRowMainMenu;

                firstRowMainMenu = currentBeacon->callsign;
                if (Config.display.showSymbol) {
                    for (int j = firstRowMainMenu.length(); j < 9; j++) {
                        firstRowMainMenu += " ";
                    }
                    if (!symbolAvailable) {
                        firstRowMainMenu += currentBeacon->symbol;
                    }
                }

                if (disableGPS) {
                    secondRowMainMenu = "";
                    thirdRowMainMenu = "    LoRa APRS TNC";
                    fourthRowMainMenu = "";
                } else {
                    const auto time_now = now();
                    secondRowMainMenu = Utils::createDateString(time_now) + "   " + Utils::createTimeString(time_now);
                    if (time_now % 10 < 5) {
                        thirdRowMainMenu = String(gps.location.lat(), 4);
                        thirdRowMainMenu += " ";
                        thirdRowMainMenu += String(gps.location.lng(), 4);
                    } else {
                        thirdRowMainMenu = String(Utils::getMaidenheadLocator(gps.location.lat(), gps.location.lng(), 8));
                        thirdRowMainMenu += " LoRa[";
                        switch (loraIndex) {
                            case 0: thirdRowMainMenu += "Eu]"; break;
                            case 1: thirdRowMainMenu += "PL]"; break;
                            case 2: thirdRowMainMenu += "UK]"; break;
                        }
                    }
                    
                    for (int i = thirdRowMainMenu.length(); i < 18; i++) {
                        thirdRowMainMenu += " ";
                    }

                    if (gps.hdop.hdop() > 5) {
                        hdopState = "X";
                    } else if (gps.hdop.hdop() > 2 && gps.hdop.hdop() < 5) {
                        hdopState = "-";
                    } else if (gps.hdop.hdop() <= 2) {
                        hdopState = "+";
                    }

                    if (gps.satellites.value() <= 9) thirdRowMainMenu += " ";
                    if (gpsIsActive) {
                        thirdRowMainMenu += String(gps.satellites.value());
                        thirdRowMainMenu += hdopState;
                    } else {
                        thirdRowMainMenu += "--";
                    }

                    String fourthRowAlt = String(gps.altitude.meters(),0);
                    fourthRowAlt.trim();
                    for (int a = fourthRowAlt.length(); a < 4; a++) {
                        fourthRowAlt = "0" + fourthRowAlt;
                    }
                    String fourthRowSpeed = String(gps.speed.kmph(),0);
                    fourthRowSpeed.trim();
                    for (int b = fourthRowSpeed.length(); b < 3; b++) {
                        fourthRowSpeed = " " + fourthRowSpeed;
                    }
                    String fourthRowCourse = String(gps.course.deg(),0);
                    if (fourthRowSpeed == "  0") {
                        fourthRowCourse = "---";
                    } else {
                        fourthRowCourse.trim();
                        for (int c = fourthRowCourse.length(); c < 3; c++) {
                            fourthRowCourse = "0" + fourthRowCourse;
                        }
                    }
                    fourthRowMainMenu = "A=";
                    fourthRowMainMenu += fourthRowAlt;
                    fourthRowMainMenu += "m  ";
                    fourthRowMainMenu += fourthRowSpeed;
                    fourthRowMainMenu += "km/h  ";
                    fourthRowMainMenu += fourthRowCourse;
                    if (Config.wxsensor.active && (time_now % 10 < 5) && wxModuleType != 0) {
                        fourthRowMainMenu = WX_Utils::readDataSensor(1);
                    }
                    if (MSG_Utils::getNumWLNKMails() > 0) {
                        fourthRowMainMenu = "** WLNK MAIL: ";
                        fourthRowMainMenu += String(MSG_Utils::getNumWLNKMails());
                        fourthRowMainMenu += " **";
                    }
                    if (MSG_Utils::getNumAPRSMessages() > 0) {
                        fourthRowMainMenu = "*** MESSAGES: ";
                        fourthRowMainMenu += String(MSG_Utils::getNumAPRSMessages());
                        fourthRowMainMenu += " ***";
                    }
                    if (!gpsIsActive) {
                        fourthRowMainMenu = "*** GPS  SLEEPING ***";
                    }
                }

                if (showHumanHeading) {
                    fifthRowMainMenu = GPS_Utils::getCardinalDirection(gps.course.deg());
                } else {
                    fifthRowMainMenu = "LAST Rx = ";
                    fifthRowMainMenu += MSG_Utils::getLastHeardTracker();
                }

                if (POWER_Utils::getBatteryInfoIsConnected()) {
                    String batteryVoltage = POWER_Utils::getBatteryInfoVoltage();
                    String batteryCharge = POWER_Utils::getBatteryInfoCurrent();
                    #if defined(TTGO_T_Beam_V0_7) || defined(TTGO_T_LORA32_V2_1_GPS) || defined(TTGO_T_LORA32_V2_1_GPS_915) || defined(TTGO_T_LORA32_V2_1_TNC) || defined(TTGO_T_LORA32_V2_1_TNC_915) || defined(HELTEC_V3_GPS) || defined(HELTEC_V3_TNC) || defined(HELTEC_V3_2_GPS) || defined(HELTEC_V3_2_TNC) || defined(HELTEC_WIRELESS_TRACKER) || defined(HELTEC_WSL_V3_GPS_DISPLAY) || defined(TTGO_T_DECK_GPS) || defined(TTGO_T_DECK_PLUS) || defined(LIGHTTRACKER_PLUS_1_0)
                        sixthRowMainMenu = "Battery: ";
                        sixthRowMainMenu += batteryVoltage;
                        sixthRowMainMenu += "V   ";
                        sixthRowMainMenu += BATTERY_Utils::getPercentVoltageBattery(batteryVoltage.toFloat());
                        sixthRowMainMenu += "%";
                    #endif
                    #ifdef HAS_AXP192
                        if (batteryCharge.toInt() == 0) {
                            sixthRowMainMenu = "Battery Charged ";
                            sixthRowMainMenu += batteryVoltage;
                            sixthRowMainMenu += "V";
                        } else if (batteryCharge.toInt() > 0) {
                            sixthRowMainMenu = "Bat: ";
                            sixthRowMainMenu += batteryVoltage;
                            sixthRowMainMenu += "V (charging)";
                        } else {
                            sixthRowMainMenu = "Battery ";
                            sixthRowMainMenu += batteryVoltage;
                            sixthRowMainMenu += "V ";
                            sixthRowMainMenu += batteryCharge;
                            sixthRowMainMenu += "mA";
                        }
                    #endif
                    #ifdef HAS_AXP2101
                        if (Config.notification.lowBatteryBeep && !POWER_Utils::isCharging() && batteryCharge.toInt() < lowBatteryPercent) {
                            lowBatteryPercent = batteryCharge.toInt();
                            NOTIFICATION_Utils::lowBatteryBeep();
                            if (batteryCharge.toInt() < 6) {
                                NOTIFICATION_Utils::lowBatteryBeep();
                            }
                        } 
                        if (POWER_Utils::isCharging()) {
                            lowBatteryPercent = 21;
                        }
                        if (POWER_Utils::isCharging() && batteryCharge != "100") {
                            sixthRowMainMenu = "Bat: ";
                            sixthRowMainMenu += String(batteryVoltage);
                            sixthRowMainMenu += "V (charging)";
                        } else if (!POWER_Utils::isCharging() && batteryCharge == "100") {
                            sixthRowMainMenu = "Battery Charged ";
                            sixthRowMainMenu += String(batteryVoltage);
                            sixthRowMainMenu += "V";
                        } else {
                            sixthRowMainMenu = "Battery  ";
                            sixthRowMainMenu += String(batteryVoltage);
                            sixthRowMainMenu += "V   ";
                            sixthRowMainMenu += batteryCharge;
                            sixthRowMainMenu += "%";
                        }
                    #endif
                } else {
                    sixthRowMainMenu = "No Battery Connected" ;
                }
                displayShow(firstRowMainMenu,
                            secondRowMainMenu,
                            thirdRowMainMenu,
                            fourthRowMainMenu,
                            fifthRowMainMenu,
                            sixthRowMainMenu);
                break;
        }
    }

}