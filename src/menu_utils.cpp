#include <TinyGPS++.h>
#include <vector>
#include "custom_characters.h"
#include "station_utils.h"
#include "configuration.h"
#include "menu_utils.h"
#include "power_utils.h"
#include "msg_utils.h"
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


namespace MENU_Utils {

void showOnScreen() {
    uint32_t lastMenuTime = millis() - menuTime;
    if (!(menuDisplay==0) && !(menuDisplay==20) && !(menuDisplay==21) && lastMenuTime > 30*1000) {
        menuDisplay = 0;
    }
    switch (menuDisplay) { // Graphic Menu is in here!!!!
        case 1:
            show_display("__MENU_1__", "", "1P -> Read Msg (" + String(MSG_Utils::getNumAPRSMessages()) + ")", "LP -> Delete Msg", "2P -> Menu 2");
            break;
        case 2:
            show_display("__MENU_2__", "", "1P -> Weather Report", "LP -> Listen Trackers", "2P -> Menu 3");
            break;
        case 3:
            show_display("__MENU_3__", "", "1P -> NOTHING YET", "LP -> Display EcoMode", "2P -> (Back) Tracking");
            break;

        case 10:            // Display Received/Saved APRS Messages
            {
                String msgSender      = loadedAPRSMessages[messagesIterator].substring(0,loadedAPRSMessages[messagesIterator].indexOf(","));
                String restOfMessage  = loadedAPRSMessages[messagesIterator].substring(loadedAPRSMessages[messagesIterator].indexOf(",")+1);
                String msgGate        = restOfMessage.substring(0,restOfMessage.indexOf(","));
                String msgText        = restOfMessage.substring(restOfMessage.indexOf(",")+1);
                show_display("MSG_APRS>", msgSender + "-->" + msgGate, msgText, "", "", "               Next>");
            }
            break;

        case 20:            // Display Heared Tracker/Stations
            show_display("LISTENING>", STATION_Utils::getFirstNearTracker(), STATION_Utils::getSecondNearTracker(), STATION_Utils::getThirdNearTracker(), STATION_Utils::getFourthNearTracker(), "<Back");
            break;
        case 21:
            // waiting for Weather Report
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
            
            secondRowMainMenu = utils::createDateString(now()) + "   " + utils::createTimeString(now());
            
            if (gps.hdop.hdop() > 5) {
                hdopState = "X";
            } else if (gps.hdop.hdop() > 2 && gps.hdop.hdop() < 5) {
                hdopState = "-";
            } else if (gps.hdop.hdop() <= 2) {
                hdopState = "+";
            }
            thirdRowMainMenu = String(gps.location.lat(), 4) + " " + String(gps.location.lng(), 4);
            for(int i = thirdRowMainMenu.length(); i < 18; i++) {
                thirdRowMainMenu += " ";
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
            fourthRowMainMenu = "A=" + fourthRowAlt + "m  " + fourthRowSpeed + "km/h  " + fourthRowCourse;
            if (MSG_Utils::getNumAPRSMessages() > 0){
                fourthRowMainMenu = "*** MESSAGES: " + String(MSG_Utils::getNumAPRSMessages()) + " ***";
            }
                    
            fifthRowMainMenu  = "LAST Rx = " + MSG_Utils::getLastHeardTracker();
                
            if (powerManagement.getBatteryInfoIsConnected()) {
                String batteryVoltage = powerManagement.getBatteryInfoVoltage();
                String batteryChargeCurrent = powerManagement.getBatteryInfoCurrent();
                #ifdef TTGO_T_Beam_V1_0
                if (batteryChargeCurrent.toInt() == 0) {
                    sixthRowMainMenu = "Battery Charged " + batteryVoltage + "V";
                } else if (batteryChargeCurrent.toInt() > 0) {
                    sixthRowMainMenu = "Bat: " + batteryVoltage + "V (charging)";
                } else {
                    sixthRowMainMenu = "Battery " + batteryVoltage + "V " + batteryChargeCurrent + "mA";
                }
                #endif
                #ifdef TTGO_T_Beam_V1_2
                    batteryVoltage = batteryVoltage.toFloat()/1000;
                    if (powerManagement.isChargeing() && batteryChargeCurrent!="100") { 
                        sixthRowMainMenu = "Bat: " + String(batteryVoltage) + "V (charging)";
                    } else if (!powerManagement.isChargeing() && batteryChargeCurrent=="100") {
                        sixthRowMainMenu = "Battery Charged " + String(batteryVoltage) + "V";
                    } else {
                        sixthRowMainMenu = "Battery  " + String(batteryVoltage) + "V   " + batteryChargeCurrent + "%";
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