#ifndef BLE_UTILS_H_
#define BLE_UTILS_H_

#include <Arduino.h>

namespace BLE_Utils {

    void stop();
    void setup();
    void sendToLoRa();
    void sendToPhone(const String& packet);

}

#endif