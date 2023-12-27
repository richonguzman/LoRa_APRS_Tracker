#ifndef LORA_UTILS_H_
#define LORA_UTILS_H_

#include <Arduino.h>

struct ReceivedLoRaPacket {
    String  text;
    int     rssi;
    float   snr;
    int     freqError;
};

namespace LoRa_Utils {

    void setFlag();
    void setup();
    void sendNewPacket(const String &newPacket);
    ReceivedLoRaPacket receivePacket();

}
#endif