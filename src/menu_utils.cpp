#include <TinyGPS++.h>
#include <vector>
#include "notification_utils.h"
#include "custom_characters.h"
#include "station_utils.h"
#include "configuration.h"
#include "menu_utils.h"
#include "power_utils.h"
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

namespace MENU_Utils {

    void showOnScreen() {
        uint32_t lastMenuTime = millis() - menuTime;
        if (!(menuDisplay==0) && !(menuDisplay==30) && !(menuDisplay==40) && lastMenuTime > 30*1000) {
            menuDisplay = 0;
        }
        switch (menuDisplay) { // Graphic Menu is in here!!!!
            case 1:     // 1.Messages
                show_display("__MENU____","  7.Emergency", "> 1.Messages", "  2.Configuration", "  3.Stations", "1P=Down 2P=Back LP=Go");
                break;
            case 10:    // 1.Messages ---> Messages Read
                show_display("_MESSAGES_", "> Read (" + String(MSG_Utils::getNumAPRSMessages()) + ")", "  Write", "  Delete", "", "1P=Down 2P=Back LP=Go");
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
                show_display("_MESSAGES_", "  Read (" + String(MSG_Utils::getNumAPRSMessages()) + ")", "> Write", "  Delete", "", "1P=Down 2P=Back LP=Go");
                break;
            case 110:   // 1.Messages ---> Messages Write ---> Write
                show_display("WRITE_MSG>", "", " aqui se escribe", "", "", "<Back Up/Down Select>");
                break;
            case 12:    // 1.Messages ---> Messages Delete
                show_display("_MESSAGES_", "  Read (" + String(MSG_Utils::getNumAPRSMessages()) + ")", "  Write", "> Delete", "", "1P=Down 2P=Back LP=Go");
                break;
            case 120:   // 1.Messages ---> Messages Delete ---> Delete: ALL
                show_display("DELETE_MSG", "", "     DELETE ALL?", "", "", " Confirm = LP or '>'");
                break;


            case 2:     // 2.Configuration
                show_display("__MENU____", "  1.Messages", "> 2.Configuration", "  3.Stations", "  4.Weather Report", "1P=Down 2P=Back LP=Go");
                break;
            case 20:    // 2.Configuration ---> Display
                show_display("__CONFIG__", "> Display","  Notifications","","","1P=Down 2P=Back LP=Go");
                break;
            case 200:   // 2.Configuration ---> Display ---> ECO Mode
                show_display("_DISPLAY_", "> ECO Mode","  Brightness","","","1P=Down 2P=Back LP=Go");
                break;
            case 201:   // 2.Configuration ---> Display ---> Brightness
                show_display("_DISPLAY_", "  ECO Mode","> Brightness","","","1P=Down 2P=Back LP=Go");
                break;
            case 21:    // 2.Configuration ---> Notifications
                show_display("__CONFIG__", "  Display","> Notifications","","","1P=Down 2P=Back LP=Go");
                break;

            case 3:     //3.Stations
                show_display("__MENU____", "  2.Configuration", "> 3.Stations", "  4.Weather Report", "  5.Status", "1P=Down 2P=Back LP=Go");
                break;
            case 30:    //3.Stations ---> Display Heared Tracker/Stations
                show_display("LISTENING>", STATION_Utils::getFirstNearTracker(), STATION_Utils::getSecondNearTracker(), STATION_Utils::getThirdNearTracker(), STATION_Utils::getFourthNearTracker(), "<Back");
                break;


            case 4:     //4.Weather
                show_display("__MENU____", "  3.Stations", "> 4.Weather Report", "  5.Status", "  6.Winlink/Mail", "1P=Down 2P=Back LP=Go");
                break;
            case 40:
                // waiting for Weather Report
                break;

            case 5:     //5.Status
                show_display("__MENU____", "  4.Weather Report", "> 5.Status", "  6.Winlink/Mail", "  7.Emergency", "1P=Down 2P=Back LP=Go");
                break;

            case 6:     //6.Winlink
                show_display("__MENU____", "  5.Status", "> 6.Winlink/Mail", "  7.Emergency", "  1.Messages", "1P=Down 2P=Back LP=Go");
                break;

            case 7:     //7.Emergency
                show_display("__MENU____", "  6.Winlink/Mail", "> 7.Emergency", "  1.Messages", "  2.Configuration", "1P=Down 2P=Back LP=Go");
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