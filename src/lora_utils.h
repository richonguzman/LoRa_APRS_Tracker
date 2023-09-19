#ifndef LORA_UTILS_H_
#define LORA_UTILS_H_

namespace LoRa_Utils {

void setFlag();
void setup();
void sendNewPacket(const String &newPacket);
String receivePacket();

}
#endif