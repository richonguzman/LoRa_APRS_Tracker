#ifndef WINLINK_UTILS_H_
#define WINLINK_UTILS_H_

#include <Arduino.h>

namespace WINLINK_Utils {

    void processWinlinkChallenge(String winlinkInteger);
    void login();

}

#endif