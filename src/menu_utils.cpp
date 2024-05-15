#include <TinyGPS++.h>
#include <vector>
#include "notification_utils.h"
#include "custom_characters.h"
#include "station_utils.h"
#include "configuration.h"
#include "APRSPacketLib.h"
#include "power_utils.h"
#include "menu_utils.h"
#include "msg_utils.h"
#include "bme_utils.h"
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
extern bool                 digirepeaterActive;
extern bool                 sosActive;
extern bool                 bluetoothActive;
extern bool                 displayEcoMode;
extern bool                 screenBrightness;
extern bool                 disableGPS;
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
//extern bool                 bmeSensorFound;

String      freqChangeWarning;
uint8_t     lowBatteryPercent       = 21;

namespace MENU_Utils {

    String checkBTType() {
        if (Config.bluetoothType == 0) {
            return "BLE iPhone";
        } else {
            return "BT Android";
        }
    }

    String checkProcessActive(bool process) {
        if (process) {
            return "ON";
        } else {
            return "OFF";
        }
    }

    String checkScreenBrightness(uint8_t bright) {
        if (bright == 255) {
            return "MAX";
        } else {
            return "MIN";
        }
    }

    void showOnScreen() {
        String lastLine, firstLineDecoder, courseSpeedAltitude, speedPacketDec, coursePacketDec, pathDec;
        uint32_t lastMenuTime = millis() - menuTime;
        if (!(menuDisplay==0) && !(menuDisplay==300) && !(menuDisplay==310) && !(menuDisplay==40) && !(menuDisplay>=500 && menuDisplay<=5100) && lastMenuTime > 30*1000) {
            menuDisplay = 0;
            messageCallsign = "";
            messageText = "";
        }
        if (keyDetected) {
            lastLine = "<Back Up/Down Select>";
        } else {
            lastLine = "1P=Down 2P=Back LP=Go";
        }
        switch (menuDisplay) { // Graphic Menu is in here!!!!
            case 1:     // 1. Messages
                show_display("__MENU____","  6.Extras", "> 1.Messages", "  2.Configuration", "  3.Stations", lastLine);
                break;
            case 2:     // 2. Configuration
                show_display("__MENU____", "  1.Messages", "> 2.Configuration", "  3.Stations", "  4.Weather Report", lastLine);
                break;
            case 3:     //3. Stations
                show_display("__MENU____", "  2.Configuration", "> 3.Stations", "  4.Weather Report", "  5.Winlink/Mail", lastLine);
                break;
            case 4:     //4. Weather
                show_display("__MENU____", "  3.Stations", "> 4.Weather Report", "  5.Winlink/Mail", "  6.Extras", lastLine);
                break;
            case 5:     //5. Winlink
                show_display("__MENU____", "  4.Weather Report", "> 5.Winlink/Mail", "  6.Extras", "  1.Messages", lastLine);
                break;
            case 6:     //6. Extras
                show_display("__MENU____", "  5.Winlink/Mail", "> 6.Extras", "  1.Messages", "  2.Configuration", lastLine);
                break;

//////////
            case 10:    // 1.Messages ---> Messages Read
                show_display("_MESSAGES_", "> Read (" + String(MSG_Utils::getNumAPRSMessages()) + ")", "  Write", "  Delete", "  APRSThursday", lastLine);
                break;
            case 100:   // 1.Messages ---> Messages Read ---> Display Received/Saved APRS Messages
                {
                    String msgSender    = loadedAPRSMessages[messagesIterator].substring(0, loadedAPRSMessages[messagesIterator].indexOf(","));
                    String msgText      = loadedAPRSMessages[messagesIterator].substring(loadedAPRSMessages[messagesIterator].indexOf(",") + 1);
                    show_display("MSG_APRS>", "From --> " + msgSender, msgText, "", "", "           Next=Down");
                }
                break;
            case 11:    // 1.Messages ---> Messages Write
                show_display("_MESSAGES_", "  Read (" + String(MSG_Utils::getNumAPRSMessages()) + ")", "> Write", "  Delete", "  APRSThursday", lastLine);
                break;
            case 110:   // 1.Messages ---> Messages Write ---> Write
                if (keyDetected) {
                    show_display("WRITE_MSG>", "", "CALLSIGN = " + String(messageCallsign), "", "", "<Back          Enter>");
                } else {
                    show_display("WRITE_MSG>", "", "No Keyboard Detected", "Can't write Message", "", "1P = Back");           
                }     
                break;
            case 111:
                if (messageText.length() <= 67) {
                    if (messageText.length() < 10) {
                        show_display("WRITE_MSG>", "CALLSIGN -> " + messageCallsign, "MSG -> " + messageText, "", "", "<Back   (0" + String(messageText.length()) + ")   Enter>");
                    } else {
                        show_display("WRITE_MSG>", "CALLSIGN -> " + messageCallsign, "MSG -> " + messageText, "", "", "<Back   (" + String(messageText.length()) + ")   Enter>");
                    }     
                } else {
                    show_display("WRITE_MSG>", "---  MSG TO LONG! ---", " -> " + messageText, "", "", "<Back   (" + String(messageText.length()) + ")");
                }
                break;
            case 12:    // 1.Messages ---> Messages Delete
                show_display("_MESSAGES_", "  Read (" + String(MSG_Utils::getNumAPRSMessages()) + ")", "  Write", "> Delete", "  APRSThursday", lastLine);
                break;
            case 120:   // 1.Messages ---> Messages Delete ---> Delete: ALL
                show_display("DELETE_MSG", "", "  DELETE APRS MSG?", "", "", " Confirm = LP or '>'");
                break;
            case 13:    // 1.Messages ---> APRSThursday
                show_display("_MESSAGES_", "  Read (" + String(MSG_Utils::getNumAPRSMessages()) + ")", "  Write", "  Delete", "> APRSThursday", lastLine);
                break;
            case 130:   // 1.Messages ---> APRSThursday ---> Delete: ALL
                show_display("APRS Thu._", "> Join APRSThursday", "  Check In", "  Unsubscribe", "  KeepSubscribed+12h", lastLine);
                break;
            case 1300:
                if (messageText.length() <= 67) {
                    if (messageText.length() < 10) {
                        show_display("WRITE_MSG>", "  - APRSThursday -", "MSG -> " + messageText, "", "", "<Back   (0" + String(messageText.length()) + ")   Enter>");
                    } else {
                        show_display("WRITE_MSG>", "  - APRSThursday -", "MSG -> " + messageText, "", "", "<Back   (" + String(messageText.length()) + ")   Enter>");
                    }     
                } else {
                    show_display("WRITE_MSG>", "---  MSG TO LONG! ---", " -> " + messageText, "", "", "<Back   (" + String(messageText.length()) + ")");
                }
                break;
            case 131:   // 1.Messages ---> APRSThursday ---> Delete: ALL
                show_display("APRS Thu._", "  Join APRSThursday", "> Check In", "  Unsubscribe", "  KeepSubscribed+12h", lastLine);
                break;
            case 1310:
                if (messageText.length() <= 67) {
                    if (messageText.length() < 10) {
                        show_display("WRITE_MSG>", "  - APRSThursday -", "MSG -> " + messageText, "", "", "<Back   (0" + String(messageText.length()) + ")   Enter>");
                    } else {
                        show_display("WRITE_MSG>", "  - APRSThursday -", "MSG -> " + messageText, "", "", "<Back   (" + String(messageText.length()) + ")   Enter>");
                    }     
                } else {
                    show_display("WRITE_MSG>", "---  MSG TO LONG! ---", " -> " + messageText, "", "", "<Back   (" + String(messageText.length()) + ")");
                }
                break;
            case 132:   // 1.Messages ---> APRSThursday ---> Delete: ALL
                show_display("APRS Thu._", "  Join APRSThursday", "  Check In", "> Unsubscribe", "  KeepSubscribed+12h", lastLine);
                break;
            case 133:   // 1.Messages ---> APRSThursday ---> Delete: ALL
                show_display("APRS Thu._", "  Join APRSThursday", "  Check In", "  Unsubscribe", "> KeepSubscribed+12h", lastLine);
                break;

//////////            
            case 20:    // 2.Configuration ---> Callsign
                show_display("_CONFIG___", "  Power Off", "> Change Callsign ", "  Change Frequency", "  Display",lastLine);
                break;
            case 21:    // 2.Configuration ---> Change Freq
                show_display("_CONFIG___", "  Change Callsign ", "> Change Frequency", "  Display", "  " + checkBTType() + " (" + checkProcessActive(bluetoothActive) + ")",lastLine);
                break;
            case 22:    // 2.Configuration ---> Display
                show_display("_CONFIG___", "  Change Frequency", "> Display", "  " + checkBTType() + " (" + checkProcessActive(bluetoothActive) + ")", "  Status",lastLine);
                break;
            case 23:    // 2.Configuration ---> Bluetooth
                show_display("_CONFIG___", "  Display",  "> " + checkBTType() + " (" + checkProcessActive(bluetoothActive) + ")", "  Status", "  Notifications", lastLine);
                break;
            case 24:    // 2.Configuration ---> Status
                show_display("_CONFIG___", "  " + checkBTType() + " (" + checkProcessActive(bluetoothActive) + ")", "> Status","  Notifications", "  Reboot",lastLine);
                break;
            case 25:    // 2.Configuration ---> Notifications
                show_display("_CONFIG___", "  Status", "> Notifications", "  Reboot", "  Power Off",lastLine);
                break;
            case 26:    // 2.Configuration ---> Reboot
                show_display("_CONFIG___", "  Notifications", "> Reboot", "  Power Off", "  Change Callsign",lastLine);
                break;
            case 27:    // 2.Configuration ---> Power Off
                show_display("_CONFIG___", "  Reboot", "> Power Off", "  Change Callsign", "  Change Frequency",lastLine);
                break;


            case 200:   // 2.Configuration ---> Change Callsign
                show_display("_CALLSIGN_", "","  Confirm Change?","","","<Back         Select>");
                break;

            case 210:   // 2.Configuration ---> Change Frequency
                switch (loraIndex) {
                    case 0: freqChangeWarning = "      Eu --> PL"; break;
                    case 1: freqChangeWarning = "      PL --> UK"; break;
                    case 2: freqChangeWarning = "      UK --> Eu"; break;
                }
                show_display("LORA__FREQ", "","   Confirm Change?", freqChangeWarning, "", "<Back         Select>");
                break;

            case 220:   // 2.Configuration ---> Display ---> ECO Mode
                show_display("_DISPLAY__", "", "> ECO Mode    (" + checkProcessActive(displayEcoMode) + ")","  Brightness  (" + checkScreenBrightness(screenBrightness) + ")","",lastLine);
                break;
            case 221:   // 2.Configuration ---> Display ---> Brightness
                show_display("_DISPLAY__", "", "  ECO Mode    (" + checkProcessActive(displayEcoMode) + ")","> Brightness  (" + checkScreenBrightness(screenBrightness) + ")","",lastLine);
                break;

            case 230:
                if (bluetoothActive) {
                    bluetoothActive = false;
                    show_display("BLUETOOTH", "", " Bluetooth --> OFF", 1000);
                } else {
                    bluetoothActive = true;
                    show_display("BLUETOOTH", "", " Bluetooth --> ON", 1000);
                }
                menuDisplay = 23;
                break;

            case 240:    // 2.Configuration ---> Status
                show_display("_STATUS___", "", "> Write","  Select","",lastLine);
                break;
            case 241:    // 2.Configuration ---> Status
                show_display("_STATUS___", "", "  Write","> Select","",lastLine);
                break;

            case 250:    // 2.Configuration ---> Notifications
                show_display("_NOTIFIC__", "> Turn Off Sound/Led","","","",lastLine);
                break;

            case 260:   // 2.Configuration ---> Reboot
                if (keyDetected) {
                    show_display("_REBOOT?__", "","Confirm Reboot...","","","<Back   Enter=Confirm");
                } else {
                    show_display("_REBOOT?__", "no Keyboard Detected"," Use RST Button to","Reboot Tracker","",lastLine);
                }
                break;
            case 270:   // 2.Configuration ---> Power Off
                if (keyDetected) {
                    show_display("POWER_OFF?", "","Confirm Power Off...","","","<Back   Enter=Confirm");
                } else {
                    show_display("POWER_OFF?", "no Keyboard Detected"," Use PWR Button to","Power Off Tracker","",lastLine);
                }
                break;

//////////
            case 30:    //3.Stations ---> Packet Decoder
                show_display("STATIONS>", "", "> Packet Decoder", "  Near By Stations", "", "<Back");
                break;
            case 31:    //3.Stations ---> Near By Stations
                show_display("STATIONS>", "", "  Packet Decoder", "> Near By Stations", "", "<Back");
                break;

            case 300:   //3.Stations ---> Packet Decoder
                firstLineDecoder = lastReceivedPacket.sender;
                for(int i = firstLineDecoder.length(); i < 9; i++) {
                    firstLineDecoder += ' ';
                }
                firstLineDecoder += lastReceivedPacket.symbol;

                if (lastReceivedPacket.type==0 || lastReceivedPacket.type==4) {      // gps and Mic-E gps
                    courseSpeedAltitude = String(lastReceivedPacket.altitude);
                    for(int j = courseSpeedAltitude.length(); j < 4; j++) {
                        courseSpeedAltitude = '0' + courseSpeedAltitude;
                    }
                    courseSpeedAltitude = "A=" + courseSpeedAltitude + "m ";
                    speedPacketDec = String(lastReceivedPacket.speed);
                    for (int k = speedPacketDec.length(); k < 3; k++) {
                        speedPacketDec = ' ' + speedPacketDec;
                    }
                    courseSpeedAltitude += speedPacketDec + "km/h ";
                    for(int l = courseSpeedAltitude.length(); l < 17; l++) {
                        courseSpeedAltitude += ' ';
                    }
                    coursePacketDec = String(lastReceivedPacket.course);
                    for(int m = coursePacketDec.length(); m < 3; m++) {
                        coursePacketDec = ' ' + coursePacketDec;
                    }
                    courseSpeedAltitude += coursePacketDec;
                    
                    double distanceKm = TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(), lastReceivedPacket.latitude, lastReceivedPacket.longitude) / 1000.0;
                    double courseTo   = TinyGPSPlus::courseTo(gps.location.lat(), gps.location.lng(), lastReceivedPacket.latitude, lastReceivedPacket.longitude);
                    
                    if (lastReceivedPacket.path.length()>14) {
                        pathDec = "P:" + lastReceivedPacket.path;
                    } else {
                        pathDec = "PATH:  " +lastReceivedPacket.path;
                    }

                    show_display(firstLineDecoder, "GPS  " + String(lastReceivedPacket.latitude,2) + " " + String(lastReceivedPacket.longitude,2), courseSpeedAltitude, "D:" + String(distanceKm) + "km    " + String(courseTo,0), pathDec, "< RSSI:" + String(lastReceivedPacket.rssi) + " SNR:" + String(lastReceivedPacket.snr));
                } else if (lastReceivedPacket.type==1) {    // message
                    show_display(firstLineDecoder, "ADDRESSEE: " + lastReceivedPacket.addressee, "MSG:  " + lastReceivedPacket.message, "", "", "< RSSI:" + String(lastReceivedPacket.rssi) + " SNR:" + String(lastReceivedPacket.snr));
                } else if (lastReceivedPacket.type==2) {    // status
                    show_display(firstLineDecoder, "-------STATUS-------", lastReceivedPacket.message, "", "", "< RSSI:" + String(lastReceivedPacket.rssi) + " SNR:" + String(lastReceivedPacket.snr));
                } else if (lastReceivedPacket.type==3) {    // telemetry
                    show_display(firstLineDecoder, "------TELEMETRY------", "", "", "", "< RSSI:" + String(lastReceivedPacket.rssi) + " SNR:" + String(lastReceivedPacket.snr));
                } else if (lastReceivedPacket.type==5) {    // object
                    show_display(firstLineDecoder, "-------OBJECT-------", "", "", "", "< RSSI:" + String(lastReceivedPacket.rssi) + " SNR:" + String(lastReceivedPacket.snr));
                }
                break;
            case 310:    //3.Stations ---> Near By Stations
                show_display("NEAR BY >", STATION_Utils::getFirstNearTracker(), STATION_Utils::getSecondNearTracker(), STATION_Utils::getThirdNearTracker(), STATION_Utils::getFourthNearTracker(), "<Back");
                break;

//////////
            case 40:
                // waiting for Weather Report
                break;

//////////
            case 50:    // 5.Winlink MENU
                if (winlinkStatus == 5) {
                    menuDisplay = 5000;
                } else {
                    show_display("_WINLINK_>", "> Login" , "  Read SavedMails(" + String(MSG_Utils::getNumWLNKMails()) + ")", "  Delete SavedMails", "  Wnlk Comment (" + checkProcessActive(winlinkCommentState) + ")" , lastLine);
                }
                break;
            case 51:    // 5.Winlink
                show_display("_WINLINK_>", "  Login" , "> Read SavedMails(" + String(MSG_Utils::getNumWLNKMails()) + ")", "  Delete SavedMails", "  Wnlk Comment (" + checkProcessActive(winlinkCommentState) + ")" , lastLine);
                break;
            case 52:    // 5.Winlink
                show_display("_WINLINK_>", "  Login" , "  Read SavedMails(" + String(MSG_Utils::getNumWLNKMails()) + ")", "> Delete SavedMails", "  Wnlk Comment (" + checkProcessActive(winlinkCommentState) + ")" , lastLine);
                break;
            case 53:    // 5.Winlink
                show_display("_WINLINK_>", "  Login" , "  Read SavedMails(" + String(MSG_Utils::getNumWLNKMails()) + ")", "  Delete SavedMails", "> Wnlk Comment (" + checkProcessActive(winlinkCommentState) + ")" , lastLine);
                break;

            case 500:    // 5.Winlink ---> Login
                show_display("_WINLINK_>", "" , "Login Initiation ...", "Challenge -> waiting", "" , "");
                break;
            case 501:    // 5.Winlink ---> Login
                show_display("_WINLINK_>", "" , "Login Initiation ...", "Challenge -> sended", "" , "");
                break;
            case 502:    // 5.Winlink ---> Login
                show_display("_WINLINK_>", "" , "Login Initiation ...", "Challenge -> ack ...", "" , "");
                break;

            case 5000:   // WINLINK: List Pend. Mail //
                show_display("WLNK__MENU", "  Write Mail" , "> List Pend. Mails", "  Downloaded Mails", "  Read Mail    (R#)", lastLine);
                break;

            case 5010:    // WINLINK: Downloaded Mails //
                show_display("WLNK__MENU", "  List Pend. Mails", "> Downloaded Mails", "  Read Mail    (R#)", "  Reply Mail   (Y#)", lastLine);
                break;
            case 50100:    // WINLINK: Downloaded Mails //
                show_display("_WINLINK_>", "" , "> Read SavedMails(" + String(MSG_Utils::getNumWLNKMails()) + ")", "  Delete SavedMails", "" , lastLine);
                break;
            case 50101:    // WINLINK: Downloaded Mails //
                {
                    String mailText = loadedWLNKMails[messagesIterator];
                    show_display("WLNK__MAIL", "", mailText, "", "", "           Next=Down");
                }
                break;
            case 50110:    // WINLINK: Downloaded Mails //
                show_display("_WINLINK_>", "" , "  Read SavedMails(" + String(MSG_Utils::getNumWLNKMails()) + ")", "> Delete SavedMails", "" , lastLine);
                break;
            case 50111:    // WINLINK: Downloaded Mails //
                show_display("WLNK__DEL", "", "  DELETE ALL MAILS?", "", "", " Confirm = LP or '>'");
                break;

            case 5020:    // WINLINK: Read Mail //
                show_display("WLNK__MENU", "  Downloaded Mails", "> Read Mail    (R#)", "  Reply Mail   (Y#)", "  Forward Mail (F#)", lastLine);
                break;
            case 5021:
                show_display("WLNK__READ", "", "    READ MAIL N." + winlinkMailNumber, "", "", "<Back          Enter>");
                break;

            case 5030:    // WINLINK: Reply Mail //
                show_display("WLNK__MENU", "  Read Mail    (R#)", "> Reply Mail   (Y#)", "  Forward Mail (F#)", "  Delete Mail  (K#)", lastLine);
                break;
            case 5031:
                show_display("WLNK_REPLY", "", "   REPLY MAIL N." + winlinkMailNumber , "", "", "<Back          Enter>");
                break;

            case 5040:    // WINLINK: Foward Mail //
                show_display("WLNK__MENU", "  Reply Mail   (Y#)", "> Forward Mail (F#)", "  Delete Mail  (K#)", "  Alias Menu", lastLine);
                break;
            case 5041:    // WINLINK: Forward Mail //
                show_display("WLNK__FORW", "", "  FORWARD MAIL N." + winlinkMailNumber , "", "", "<Back          Enter>");
                break;
            case 5042:    // WINLINK: Forward Mail //
                show_display("WLNK_FORW_", "  FORWARD MAIL N." + winlinkMailNumber , "To = " + winlinkAddressee, "", "", "<Back          Enter>");
                break;

            case 5050:    // WINLINK: Delete Mail //
                show_display("WLNK__MENU", "  Forward Mail (F#)", "> Delete Mail  (K#)", "  Alias Menu", "  Log Out", lastLine);
                break;
            case 5051:    // WINLINK: Delete Mail //
                show_display("WLNK___DEL", "", "   DELETE MAIL N."  + winlinkMailNumber, "", "", "<Back          Enter>");
                break;
            
            case 5060:    // WINLINK: Alias Menu //
                show_display("WLNK__MENU", "  Delete Mail  (K#)", "> Alias Menu", "  Log Out", "  Write Mail", lastLine);
                break;
            case 5061:    // WINLINK: Alias Menu : Create Alias //
                show_display("WLNK_ALIAS", "> Create Alias" , "  Delete Alias ", "  List All Alias", "", lastLine);
                break;
            case 50610:   // WINLINK: Alias Menu : Create Alias //
                show_display("WLNK_ALIAS", "", "Write Alias to Create", "     -> " + winlinkAlias, "", "<Back          Enter>");
                break;
            case 50611:   // WINLINK: Alias Menu : Create Alias //
                show_display("WLNK_ALIAS", "", "      " + winlinkAlias + " =", winlinkAliasComplete, "", "<Back          Enter>");
                break;
            case 5062:    // WINLINK: Alias Menu : Delete Alias //
                show_display("WLNK_ALIAS", "  Create Alias" , "> Delete Alias ", "  List All Alias", "", lastLine);
                break;
            case 50620:   // WINLINK: Alias Menu : Delete Alias //
                show_display("WLNK_ALIAS", "Write Alias to Delete", "", "     -> " + winlinkAlias, "", "<Back          Enter>");
                break;
            case 5063:    // WINLINK: Alias Menu : List Alias//
                show_display("WLNK_ALIAS", "  Create Alias" , "  Delete Alias ", "> List All Alias", "", lastLine);
                break;

            case 5070:    // WINLINK: Log Out MAIL //
                show_display("WLNK__MENU", "  Alias Menu", "> Log Out", "  Write Mail", "  List Pend. Mails", lastLine);
                break;

            case 5080:    // WINLINK: WRITE MAIL //
                show_display("WLNK__MENU", "  Log Out", "> Write Mail", "  List Pend. Mails", "  Downloaded Mails", lastLine);
                break;
            case 5081:    // WINLINK: WRITE MAIL: Addressee //
                show_display("WLNK__MAIL", "--- Send Mail to ---", "", "-> " + winlinkAddressee, "", "<Back          Enter>");
                break;
            case 5082:    // WINLINK: WRITE MAIL: Subject //
                show_display("WLNK__MAIL", "--- Write Subject ---", "", "-> " + winlinkSubject, "", "<Back          Enter>");
                break;
            case 5083:    // WINLINK: WRITE MAIL: Body //
                if (winlinkBody.length() <= 67) {
                show_display("WLNK__MAIL", "-- Body (lenght=" + String(winlinkBody.length()) + ")", "-> " + winlinkBody, "", "", "<Clear Body    Enter>");
                } else {
                show_display("WLNK__MAIL", "-- Body To Long = " + String(winlinkBody.length()) + "!", "-> " + winlinkBody, "", "", "<Clear Body");
                }
                break;
            case 5084:    // WINLINK: WRITE MAIL: End Mail? //
                show_display("WLNK__MAIL", "", "> End Mail", "  1 More Line", "", "      Up/Down Select>");
                break;
            case 5085:    // WINLINK: WRITE MAIL: One More Line(Body) //
                show_display("WLNK__MAIL", "", "  End Mail", "> 1 More Line", "", "      Up/Down Select>");
                break;

                // validar winlinkStatus = 0
                // check si no esta logeado o si

//////////
            case 60:    // 6. Extras ---> Flashlight
                show_display("__EXTRAS__", "> Flashlight    (" + checkProcessActive(flashlight) + ")", "  DigiRepeater  (" + checkProcessActive(digirepeaterActive) + ")", "  S.O.S.        (" + checkProcessActive(sosActive) + ")","  Send GPS + Comment",lastLine);
                break;
            case 61:    // 6. Extras ---> Digirepeater
                show_display("__EXTRAS__", "  Flashlight    (" + checkProcessActive(flashlight) + ")", "> DigiRepeater  (" + checkProcessActive(digirepeaterActive) + ")", "  S.O.S.        (" + checkProcessActive(sosActive) + ")","  Send GPS + Comment",lastLine);
                break;
            case 62:    // 6. Extras ---> S.O.S.
                show_display("__EXTRAS__", "  Flashlight    (" + checkProcessActive(flashlight) + ")", "  DigiRepeater  (" + checkProcessActive(digirepeaterActive) + ")", "> S.O.S.        (" + checkProcessActive(sosActive) + ")","  Send GPS + Comment",lastLine);
                break;
            case 63:    // 6. Extras ---> Extra Comment.
                show_display("__EXTRAS__", "  Flashlight    (" + checkProcessActive(flashlight) + ")", "  DigiRepeater  (" + checkProcessActive(digirepeaterActive) + ")", "  S.O.S.        (" + checkProcessActive(sosActive) + ")","> Send GPS + Comment",lastLine);
                break;
            case 630:
                if (messageText.length() <= 67) {
                    if (messageText.length() < 10) {
                        show_display("_COMMENT_>", "Send this Comment in","the next GPS Beacon :", messageText, "", "<Back   (0" + String(messageText.length()) + ")   Enter>");
                    } else {
                        show_display("_COMMENT_>", "Send this Comment in","the next GPS Beacon :", messageText, "", "<Back   (" + String(messageText.length()) + ")   Enter>");
                    }     
                } else {
                    show_display("_COMMENT_>", " Comment is to long! ", " -> " + messageText, "", "", "<Back   (" + String(messageText.length()) + ")");
                }
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

                #ifdef TTGO_T_LORA32_V2_1_TNC
                secondRowMainMenu = "";
                thirdRowMainMenu = "    LoRa APRS TNC";
                fourthRowMainMenu = "";
                #else
                if (disableGPS) {
                    secondRowMainMenu = "";
                    thirdRowMainMenu = "    LoRa APRS TNC";
                    fourthRowMainMenu = "";
                } else {
                    const auto time_now = now();
                    secondRowMainMenu = Utils::createDateString(time_now) + "   " + Utils::createTimeString(time_now);
                    if (time_now % 10 < 5) {
                        thirdRowMainMenu = String(gps.location.lat(), 4) + " " + String(gps.location.lng(), 4);
                    } else {
                        thirdRowMainMenu = String(Utils::getMaidenheadLocator(gps.location.lat(), gps.location.lng(), 8));
                        thirdRowMainMenu += " LoRa[";
                        switch (loraIndex) {
                            case 0: thirdRowMainMenu += "Eu]"; break;
                            case 1: thirdRowMainMenu += "PL]"; break;
                            case 2: thirdRowMainMenu += "UK]"; break;
                        }
                    }
                    
                    for(int i = thirdRowMainMenu.length(); i < 18; i++) {
                        thirdRowMainMenu += " ";
                    }

                    if (gps.hdop.hdop() > 5) {
                        hdopState = "X";
                    } else if (gps.hdop.hdop() > 2 && gps.hdop.hdop() < 5) {
                        hdopState = "-";
                    } else if (gps.hdop.hdop() <= 2) {
                        hdopState = "+";
                    }

                    if (gps.satellites.value() > 9) {
                        thirdRowMainMenu += String(gps.satellites.value()) + hdopState;
                    } else {
                        thirdRowMainMenu += " " + String(gps.satellites.value()) + hdopState;
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
                        for(int c = fourthRowCourse.length(); c < 3; c++) {
                            fourthRowCourse = "0" + fourthRowCourse;
                        }
                    }
                    if (Config.bme.active) {
                        if (time_now % 10 < 5) {
                            fourthRowMainMenu = "A=" + fourthRowAlt + "m  " + fourthRowSpeed + "km/h  " + fourthRowCourse;
                        } else {
                            if (wxModuleType != 0) {
                                fourthRowMainMenu = BME_Utils::readDataSensor(1);
                            } else {
                                fourthRowMainMenu = "A=" + fourthRowAlt + "m  " + fourthRowSpeed + "km/h  " + fourthRowCourse;
                            }
                        }
                    } else {
                        fourthRowMainMenu = "A=" + fourthRowAlt + "m  " + fourthRowSpeed + "km/h  " + fourthRowCourse;
                    }
                    if (MSG_Utils::getNumWLNKMails() > 0) {
                        fourthRowMainMenu = "** WLNK MAIL: " + String(MSG_Utils::getNumWLNKMails()) + " **";
                    }
                    if (MSG_Utils::getNumAPRSMessages() > 0) {
                        fourthRowMainMenu = "*** MESSAGES: " + String(MSG_Utils::getNumAPRSMessages()) + " ***";
                    }
                }
                #endif

                fifthRowMainMenu  = "LAST Rx = " + MSG_Utils::getLastHeardTracker();

                if (POWER_Utils::getBatteryInfoIsConnected()) {
                    String batteryVoltage = POWER_Utils::getBatteryInfoVoltage();
                    String batteryCharge = POWER_Utils::getBatteryInfoCurrent();
                    #if defined(TTGO_T_Beam_V0_7) || defined(TTGO_T_LORA32_V2_1_GPS) || defined(TTGO_T_LORA32_V2_1_TNC) || defined(HELTEC_V3_GPS) || defined(HELTEC_WIRELESS_TRACKER) || defined(TTGO_T_DECK_GPS)
					    sixthRowMainMenu = "Bat: " + batteryVoltage + "V";
                    #endif
                    #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_0_SX1268)
                        if (batteryCharge.toInt() == 0) {
                            sixthRowMainMenu = "Battery Charged " + batteryVoltage + "V";
                        } else if (batteryCharge.toInt() > 0) {
                            sixthRowMainMenu = "Bat: " + batteryVoltage + "V (charging)";
                        } else {
                            sixthRowMainMenu = "Battery " + batteryVoltage + "V " + batteryCharge + "mA";
                        }
                    #endif
                    #if defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3)
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
                        batteryVoltage = batteryVoltage.toFloat()/1000;
                        if (POWER_Utils::isCharging() && batteryCharge!="100") {
                            sixthRowMainMenu = "Bat: " + String(batteryVoltage) + "V (charging)";
                        } else if (!POWER_Utils::isCharging() && batteryCharge=="100") {
                            sixthRowMainMenu = "Battery Charged " + String(batteryVoltage) + "V";
                        } else {
                            sixthRowMainMenu = "Battery  " + String(batteryVoltage) + "V   " + batteryCharge + "%";
                        }
                    #endif
                } else {
                    sixthRowMainMenu = "No Battery Connected" ;
                }
                show_display(firstRowMainMenu,
                            secondRowMainMenu,
                            thirdRowMainMenu,
                            fourthRowMainMenu,
                            fifthRowMainMenu,
                            sixthRowMainMenu);
                break;
        }
    }

}