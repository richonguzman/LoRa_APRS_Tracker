#include <TinyGPS++.h>
#include <vector>
#include "notification_utils.h"
#include "custom_characters.h"
#include "station_utils.h"
#include "configuration.h"
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
extern PowerManagement      powerManagement;
extern std::vector<String>  loadedAPRSMessages;
extern int                  messagesIterator;
extern uint32_t             menuTime;
extern bool                 symbolAvailable;
extern int                  lowBatteryPercent;
extern bool                 keyDetected;
extern String               messageCallsign;
extern String               messageText;
extern bool                 digirepeaterActive;
extern bool                 sosActive;
extern bool                 bluetoothActive;
extern bool                 displayEcoMode;
extern bool                 screenBrightness;

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

    String checkScreenBrightness(int bright) {
        if (bright == 255) {
            return "MAX";
        } else {
            return "MIN";
        }
    }

    void showOnScreen() {
        String lastLine;
        uint32_t lastMenuTime = millis() - menuTime;
        if (!(menuDisplay==0) && !(menuDisplay==30) && !(menuDisplay==40) && lastMenuTime > 30*1000) {
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
                show_display("__MENU____","  6.Emergency", "> 1.Messages", "  2.Configuration", "  3.Stations", lastLine);
                break;
            case 2:     // 2. Configuration
                show_display("__MENU____", "  1.Messages", "> 2.Configuration", "  3.Stations", "  4.Weather Report", lastLine);
                break;
            case 3:     //3. Stations
                show_display("__MENU____", "  2.Configuration", "> 3.Stations", "  4.Weather Report", "  5.Winlink/Mail", lastLine);
                break;
            case 4:     //4. Weather
                show_display("__MENU____", "  3.Stations", "> 4.Weather Report", "  5.Winlink/Mail", "  6.Emergency", lastLine);
                break;
            case 5:     //5. Winlink
                show_display("__MENU____", "  4.Weather Report", "> 5.Winlink/Mail", "  6.Emergency", "  1.Messages", lastLine);
                break;
            case 6:     //6. Emergency
                show_display("__MENU____", "  5.Winlink/Mail", "> 6.Emergency", "  1.Messages", "  2.Configuration", lastLine);
                break;


            case 10:    // 1.Messages ---> Messages Read
                show_display("_MESSAGES_", "> Read (" + String(MSG_Utils::getNumAPRSMessages()) + ")", "  Write", "  Delete", "", lastLine);
                break;
            case 100:   // 1.Messages ---> Messages Read ---> Display Received/Saved APRS Messages
                {
                    String msgSender      = loadedAPRSMessages[messagesIterator].substring(0,loadedAPRSMessages[messagesIterator].indexOf(","));
                    String restOfMessage  = loadedAPRSMessages[messagesIterator].substring(loadedAPRSMessages[messagesIterator].indexOf(",")+1);
                    String msgGate        = restOfMessage.substring(0,restOfMessage.indexOf(","));
                    String msgText        = restOfMessage.substring(restOfMessage.indexOf(",")+1);
                    show_display("MSG_APRS>", msgSender + "-->" + msgGate, msgText, "", "", "               Next>");
                }
                break;
            case 11:    // 1.Messages ---> Messages Write
                show_display("_MESSAGES_", "  Read (" + String(MSG_Utils::getNumAPRSMessages()) + ")", "> Write", "  Delete", "", lastLine);
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
                show_display("_MESSAGES_", "  Read (" + String(MSG_Utils::getNumAPRSMessages()) + ")", "  Write", "> Delete", "", lastLine);
                break;
            case 120:   // 1.Messages ---> Messages Delete ---> Delete: ALL
                show_display("DELETE_MSG", "", "     DELETE ALL?", "", "", " Confirm = LP or '>'");
                break;

            
            case 20:    // 2.Configuration ---> Callsign
                show_display("_CONFIG___", "  Power Off", "> Callsign Change","  Display", "  " + checkBTType() + " (" + checkProcessActive(bluetoothActive) + ")",lastLine);
                break;
            case 21:    // 2.Configuration ---> Display
                show_display("_CONFIG___", "  Callsign Change", "> Display", "  " + checkBTType() + " ("+ checkProcessActive(bluetoothActive) + ")", "  Status",lastLine);
                break;
            case 22:    // 2.Configuration ---> Bluetooth
                show_display("_CONFIG___", "  Display",  "> " + checkBTType() + " (" + checkProcessActive(bluetoothActive) + ")", "  Status", "  Notifications", lastLine);
                break;
            case 23:    // 2.Configuration ---> Status
                show_display("_CONFIG___", "  " + checkBTType() + " (" + checkProcessActive(bluetoothActive) + ")", "> Status","  Notifications", "  Reboot",lastLine);
                break;
            case 24:    // 2.Configuration ---> Notifications
                show_display("_CONFIG___", "  Status", "> Notifications", "  Reboot", "  Power Off",lastLine);
                break;
            case 25:    // 2.Configuration ---> Reboot
                show_display("_CONFIG___", "  Notifications", "> Reboot", "  Power Off", "  Callsign Change",lastLine);
                break;
            case 26:    // 2.Configuration ---> Power Off
                show_display("_CONFIG___", "  Reboot", "> Power Off", "  Callsign Change", "  Display",lastLine);
                break;

            case 200:   // 2.Configuration ---> Callsign
                show_display("_CALLSIGN_", "","  Confirm Change?","","","<Back   Enter=Confirm");
                break;

            case 210:   // 2.Configuration ---> Display ---> ECO Mode
                show_display("_DISPLAY__", "", "> ECO Mode    (" + checkProcessActive(displayEcoMode) + ")","  Brightness  (" + checkScreenBrightness(screenBrightness) + ")","",lastLine);
                break;
            case 211:   // 2.Configuration ---> Display ---> Brightness
                show_display("_DISPLAY__", "", "  ECO Mode    (" + checkProcessActive(displayEcoMode) + ")","> Brightness  (" + checkScreenBrightness(screenBrightness) + ")","",lastLine);
                break;

            case 220:
                if (bluetoothActive) {
                    bluetoothActive = false;
                    show_display("BLUETOOTH", "", " Bluetooth --> OFF", 1000);
                } else {
                    bluetoothActive = true;
                    show_display("BLUETOOTH", "", " Bluetooth --> ON", 1000);
                }
                menuDisplay = 22;
                break;

            case 230:    // 2.Configuration ---> Status
                show_display("_STATUS___", "", "> Write","  Select","",lastLine);
                break;
            case 231:    // 2.Configuration ---> Status
                show_display("_STATUS___", "", "  Write","> Select","",lastLine);
                break;

            case 240:    // 2.Configuration ---> Notifications
                show_display("_NOTIFIC__", "> Turn Off Sound/Led","","","",lastLine);
                break;

            case 250:   // 2.Configuration ---> Reboot
                if (keyDetected) {
                    show_display("_REBOOT?__", "","Confirm Reboot...","","","<Back   Enter=Confirm");
                } else {
                    show_display("_REBOOT?__", "no Keyboard Detected"," Use RST Button to","Reboot Tracker","",lastLine);
                }
                break;
            case 260:   // 2.Configuration ---> Power Off
                if (keyDetected) {
                    show_display("POWER_OFF?", "","Confirm Power Off...","","","<Back   Enter=Confirm");
                } else {
                    show_display("POWER_OFF?", "no Keyboard Detected"," Use PWR Button to","Power Off Tracker","",lastLine);
                }
                break;


            case 30:    //3.Stations ---> Display Heared Tracker/Stations
                show_display("LISTENING>", STATION_Utils::getFirstNearTracker(), STATION_Utils::getSecondNearTracker(), STATION_Utils::getThirdNearTracker(), STATION_Utils::getFourthNearTracker(), "<Back");
                break;


            case 40:
                // waiting for Weather Report
                break;

            case 60:    // 6. Emergency ---> Digirepeater
                /*if (digirepeaterActive) {
                    digi = "ON";
                } else {
                    digi = "OFF";
                }
                if (sosActive) {
                    sos = "ON";
                } else {
                    sos = "OFF";
                }*/
                show_display("EMERGENCY_", "", "> DigiRepeater  (" + checkProcessActive(digirepeaterActive) + ")", "  S.O.S.        (" + checkProcessActive(sosActive) + ")","",lastLine);
                break;
            case 61:    // 6. Emergency ---> S.O.S.
                /*if (digirepeaterActive) {
                    digi = "ON";
                } else {
                    digi = "OFF";
                }
                if (sosActive) {
                    sos = "ON";
                } else {
                    sos = "OFF";
                }*/
                show_display("EMERGENCY_", "", "  DigiRepeater  (" + checkProcessActive(digirepeaterActive) + ")", "> S.O.S.        (" + checkProcessActive(sosActive) + ")","",lastLine);
                break;

            case 0:       ///////////// MAIN MENU //////////////
                String hdopState, firstRowMainMenu, secondRowMainMenu, thirdRowMainMenu, fourthRowMainMenu, fifthRowMainMenu, sixthRowMainMenu;

                firstRowMainMenu = currentBeacon->callsign;
                if (Config.showSymbolOnScreen) {
                    for (int j=firstRowMainMenu.length();j<9;j++) {
                        firstRowMainMenu += " ";
                    }
                    if (!symbolAvailable) {
                        firstRowMainMenu += currentBeacon->symbol;
                    }
                }

                const auto time_now = now();
                secondRowMainMenu = utils::createDateString(time_now) + "   " + utils::createTimeString(time_now);

                if (time_now % 10 < 5) {
                    thirdRowMainMenu = String(gps.location.lat(), 4) + " " + String(gps.location.lng(), 4);
                } else {
                    thirdRowMainMenu = String(utils::getMaidenheadLocator(gps.location.lat(), gps.location.lng(), 8));
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
                for (int a=fourthRowAlt.length();a<4;a++) {
                    fourthRowAlt = "0" + fourthRowAlt;
                }
                String fourthRowSpeed = String(gps.speed.kmph(),0);
                fourthRowSpeed.trim();
                for (int b=fourthRowSpeed.length(); b<3;b++) {
                    fourthRowSpeed = " " + fourthRowSpeed;
                }
                String fourthRowCourse = String(gps.course.deg(),0);
                if (fourthRowSpeed == "  0") {
                    fourthRowCourse = "---";
                } else {
                    fourthRowCourse.trim();
                    for(int c=fourthRowCourse.length();c<3;c++) {
                        fourthRowCourse = "0" + fourthRowCourse;
                    }
                }
                if (Config.bme.active) {
                    if (time_now % 10 < 5) {
                        fourthRowMainMenu = "A=" + fourthRowAlt + "m  " + fourthRowSpeed + "km/h  " + fourthRowCourse;
                    } else {
                        fourthRowMainMenu = BME_Utils::readDataSensor("OLED");
                    }
                } else {
                    fourthRowMainMenu = "A=" + fourthRowAlt + "m  " + fourthRowSpeed + "km/h  " + fourthRowCourse;
                }               
                if (MSG_Utils::getNumAPRSMessages() > 0){
                    fourthRowMainMenu = "*** MESSAGES: " + String(MSG_Utils::getNumAPRSMessages()) + " ***";
                }

                fifthRowMainMenu  = "LAST Rx = " + MSG_Utils::getLastHeardTracker();

                if (powerManagement.getBatteryInfoIsConnected()) {
                    String batteryVoltage = powerManagement.getBatteryInfoVoltage();
                    String batteryCharge = powerManagement.getBatteryInfoCurrent();
                    #ifdef TTGO_T_Beam_V0_7
					    sixthRowMainMenu = "Bat: " + batteryVoltage + "V";
                    #endif
                    #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_LORA_V2_1) || defined(TTGO_T_Beam_V1_0_SX1268)
                    if (batteryCharge.toInt() == 0) {
                        sixthRowMainMenu = "Battery Charged " + batteryVoltage + "V";
                    } else if (batteryCharge.toInt() > 0) {
                        sixthRowMainMenu = "Bat: " + batteryVoltage + "V (charging)";
                    } else {
                        sixthRowMainMenu = "Battery " + batteryVoltage + "V " + batteryCharge + "mA";
                    }
                    #endif
                    #ifdef TTGO_T_Beam_V1_2
                        if (Config.notification.lowBatteryBeep && !powerManagement.isChargeing() && batteryCharge.toInt() < lowBatteryPercent) {
                            lowBatteryPercent = batteryCharge.toInt();
                            NOTIFICATION_Utils::lowBatteryBeep();
                            if (batteryCharge.toInt() < 6) {
                                NOTIFICATION_Utils::lowBatteryBeep();
                            }
                        } 
                        if (powerManagement.isChargeing()) {
                            lowBatteryPercent = 21;
                        }
                        batteryVoltage = batteryVoltage.toFloat()/1000;
                        if (powerManagement.isChargeing() && batteryCharge!="100") {
                            sixthRowMainMenu = "Bat: " + String(batteryVoltage) + "V (charging)";
                        } else if (!powerManagement.isChargeing() && batteryCharge=="100") {
                            sixthRowMainMenu = "Battery Charged " + String(batteryVoltage) + "V";
                        } else {
                            sixthRowMainMenu = "Battery  " + String(batteryVoltage) + "V   " + batteryCharge + "%";
                        }
                    #endif
                } else {
                    sixthRowMainMenu = "No Battery Connected" ;
                }
                show_display(String(firstRowMainMenu),
                            String(secondRowMainMenu),
                            String(thirdRowMainMenu),
                            String(fourthRowMainMenu),
                            String(fifthRowMainMenu),
                            String(sixthRowMainMenu));
                break;
        }

    }

}