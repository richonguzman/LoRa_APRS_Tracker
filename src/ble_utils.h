#ifndef BLE_UTILS_H_
#define BLE_UTILS_H_

#include <Arduino.h>

namespace BLE_Utils {

    void setup();
    void sendToLoRa();
    void txBLE(uint8_t p);
    void txToPhoneOverBLE(String frame);
    void sendToPhone(const String& packet);

}

#endif