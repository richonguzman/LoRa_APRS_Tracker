#include "winlink_utils.h"
#include "configuration.h"
#include "msg_utils.h"
#include "display.h"
#include "logger.h"

extern Configuration    Config;
extern int              winlinkStatus;
extern int              menuDisplay;
extern logging::Logger  logger;

namespace WINLINK_Utils {

    void processWinlinkChallenge(String winlinkInteger) {
        String challengeAnswer;
        for (int i=0; i<winlinkInteger.length(); i++) {
            String number = String(winlinkInteger[i]);
            int digit = number.toInt();
            challengeAnswer += Config.winlink.password[digit-1];
        }
        challengeAnswer += "AZ6";
        delay(500);
        //Serial.println("el challenge creado es " + challengeAnswer);
        MSG_Utils::sendMessage(1, "WLNK-1", challengeAnswer);
    }

    void login() {
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Winlink","---> Start Login");
        show_display("__WINLINK_", "" , "Login Initiation ...", "", "" , "<Back");
        if (winlinkStatus == 5) {
            menuDisplay = 5000;
        } else {
            winlinkStatus = 1;
            MSG_Utils::sendMessage(1, "WLNK-1", "L");
            menuDisplay = 500;
        }
    }

}