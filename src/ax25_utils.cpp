#include "ax25_utils.h"

namespace AX25_Utils {

  AX25Frame decodedFrame;

  String decodeFrame(char frame[]) {
    String packet = "";
    for (int a=0;a<6;a++) {
      uint16_t shiftedValue = frame[a] >> 1;
      if (shiftedValue == 32) { // space or null
        a=10;
      } else {
        Serial.print(char(shiftedValue));
        packet += char(shiftedValue);
      }
    }
    uint16_t ssid = frame[6] >> 1;
    if (isdigit(char(ssid))) {
      Serial.print("-");
      Serial.print(char(ssid));
      packet += "-";
      packet += char(ssid);
    }
    return packet;
  }

  bool decodeAX25(byte* frame, int frameSize, AX25Frame* decodedFrame) {
    if (frameSize <14) {          // not a AX.25 frame
      return false;
    }
    if (frame[0] != KissSpecialCharacter::Fend && frame[1] != KissCommandCode::Data && frame[frameSize-1] == KissSpecialCharacter::Fend) { // not a kiss encapsulated packet
      return false;
    }
    int payloadFrameStart = 0;
    for (int i=0;i<frameSize;i++) {                   // where is CONTROL y PID ?
      if (frame[i] == 0x03 && frame[i+1] == 0xf0) {
        payloadFrameStart = i+1;
      }
    }
    memcpy(decodedFrame->tocall, frame + 2, 7);        // Extract destination address
    memcpy(decodedFrame->sender, frame + 9, 7);        // Extract source address
    if (payloadFrameStart >= 21) {                     // is there path1?
      memcpy(decodedFrame->path1, frame + 16, 7);   
    }
    if (payloadFrameStart >= 28) {                     // is there path2?
      memcpy(decodedFrame->path2, frame + 23, 7);   
    }
    decodedFrame->control = frame[payloadFrameStart-1];                                            // Extract control information  // 0x03
    decodedFrame->pid = frame[payloadFrameStart];                                                  // Extract pid information      // 0xF0                                
    memcpy(decodedFrame->payload, frame + payloadFrameStart + 1, frameSize-payloadFrameStart-2);   // Extract payload
    return true;      // Successfully decoded
  }

  String processAX25(byte* frame) {
    if (decodeAX25(frame, sizeof(frame), &decodedFrame)) {
      String packetToLoRa = "";
      packetToLoRa = decodeFrame(decodedFrame.sender) + ">" + decodeFrame(decodedFrame.tocall);

      if (decodedFrame.path1[0] != 0) {
        packetToLoRa += "," + decodeFrame(decodedFrame.path1);
      }
      if (decodedFrame.path2[0] != 0) {
        packetToLoRa += "," + decodeFrame(decodedFrame.path2);
      }
      packetToLoRa += ":";
      packetToLoRa += decodedFrame.payload;
      return packetToLoRa;
    } else {
      return "";
    }
  }

}
