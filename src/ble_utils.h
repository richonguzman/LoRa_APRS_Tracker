#ifndef BLE_UTILS_H_
#define BLE_UTILS_H_

#include <Arduino.h>

/*enum KissSpecialCharacter {
    Fend = 0xc0,
    Fesc = 0xdb,
    Tfend = 0xdc,
    Tfesc = 0xdd
  };

enum KissCommandCode {
    Data = 0x00,
    TxDelay = 0x01,
    P = 0x02,
    SlotTime = 0x03,
    TxTail = 0x04,
    SetHardware = 0x06,
    SignalReport = 0x07,
    RebootRequested = 0x08,
    Telemetry = 0x09,
    NoCmd = 0x80
  };*/

namespace BLE_Utils {

    void setup();
    void sendToLoRa();
    void txBLE(uint8_t p);
    void txToPhoneOverBLE(String frame);
    void sendToPhone(const String& packet);

}

#endif