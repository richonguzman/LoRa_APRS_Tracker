#ifndef BLE_UTILS_H_
#define BLE_UTILS_H_

#include <Arduino.h>

namespace BLE_Utils {

    void stop();
    void setup();
    void sendToLoRa();
    void txBLE(uint8_t p);
    void txToPhoneOverBLE(const String& frame);
    void sendToPhone(const String& packet);

}

#endif