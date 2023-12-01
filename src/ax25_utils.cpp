#include "ax25_utils.h"

namespace AX25_Utils {

  AX25Frame decodedFrame;

  String decodeFrame(String frame) {
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
    uint16_t ssid = (frame[6] >> 1) & 0x0f;
    Serial.print("-");
    Serial.print(char(ssid));
    packet += "-";
    packet += char(ssid);
    return packet;
  }

  bool decodeAX25(String frame, int frameSize, AX25Frame* decodedFrame) {
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
    decodedFrame->tocall = frame.substring(2,9);//, frame[2], 7);        // Extract destination address
    decodedFrame->sender = frame.substring(9,16);        // Extract source address
    if (payloadFrameStart >= 21) {                     // is there path1?
      decodedFrame->path1 = frame.substring(16,23);
    }
    if (payloadFrameStart >= 28) {                     // is there path2?
      decodedFrame->path2 = frame.substring(23,30);
    }
    decodedFrame->control = frame.substring(payloadFrameStart-1,payloadFrameStart);   // Extract control information  // 0x03
    decodedFrame->pid = frame.substring(payloadFrameStart,payloadFrameStart+1);       // Extract pid information      // 0xF0                                
    decodedFrame->payload = frame.substring(payloadFrameStart+1,frameSize-1);           // Extract payload
    return true;      // Successfully decoded
  }

  String AX25FrameToLoRaPacket(String frame) {
    Serial.println(frame);
    Serial.println(frame.length());
    if (decodeAX25(frame, frame.length(), &decodedFrame)) {
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

  String frameCleaning(String frame) {
    if (frame.length()>6) {
        frame = frame.substring(0,6);
    } else if (frame.length()<6) {
      for (int i=0;frame.length()<6;i++) { 
        frame += " ";
      }
    }
    return frame;
  }

  String intToBinaryString(int value, int bitLength) {
    String result = "";
    for (int i = bitLength - 1; i >= 0; i--) {
      result += ((value >> i) & 1) ? '1' : '0';
    }
    return result;
  }

  uint8_t binaryStringToUint8(String binaryString) {
    return strtol(binaryString.c_str(), nullptr, 2);
  }

  String encodeFrame(String frame, int type) {
    //Serial.println(frame);//
    String packet = "";
    String address, concatenatedBinary;
    int ssid;
    if (frame.indexOf("-")>0) {
      address = frameCleaning(frame.substring(0,frame.indexOf("-")));
      int ssid = frame.substring(frame.indexOf("-")+1).toInt();
      if (ssid>15) {
        ssid = 0;   //String binaryString = "011" + generateSSIDBinary(ssid,4) + "0"; // ssid =  C + RR + SSSS + 0 = 011 + ssss + 0     
      }
    } else {
      address = frameCleaning(frame);
      ssid = 0;
    }
    for (int j=0;j<6;j++) {
      char c = address[j];
      packet += char(c<<1);
    }
    if (type == 0) {
      concatenatedBinary = "111" + intToBinaryString(ssid,4) + "0";
    } else if (type == 1) {
      concatenatedBinary = "011" + intToBinaryString(ssid,4) + "0";
    } else if (type == 2) {
      concatenatedBinary = "011" + intToBinaryString(ssid,4) + "1";
    }
    packet += binaryStringToUint8(concatenatedBinary);
    return packet;
  }

  String LoRaPacketToAX25Frame(String packet) {
    String encodedPacket = "";
    String payload = packet.substring(packet.indexOf(":")+1);
    String temp = packet.substring(packet.indexOf(">")+1, packet.indexOf(":"));
    if (temp.indexOf(",")>0) {    // tocall
      encodedPacket = encodeFrame(temp.substring(0,temp.indexOf(",")),0);
      temp = temp.substring(temp.indexOf(",")+1);
    } else {
      encodedPacket = encodeFrame(temp,0);
      temp = "";
    }
    encodedPacket += encodeFrame(packet.substring(0,packet.indexOf(">")),2);    // sender
    /*if (temp.length() > 0) { // si hay mas paths / digirepeaters
    // aqui el encode para los restantes path
    }*/
    encodedPacket += char(0x03);
    encodedPacket += char(0xF0);
    encodedPacket += packet.substring(packet.indexOf(":")+1);   // payload
    return encodedPacket;
  }

}