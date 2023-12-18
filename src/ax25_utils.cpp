#include "ax25_utils.h"

namespace AX25_Utils {

  AX25Frame decodedFrame;

  String decodeFrame(String frame) {
    String packet = "";
    for (int a=0;a<6;a++) {
      uint16_t shiftedValue = frame[a] >> 1;
      if (shiftedValue == 32) {
        a=10;
      } else {
        Serial.print(char(shiftedValue));
        packet += char(shiftedValue);
      }
    }
    byte ssid = (frame[6]>>1) & 0x0f;
    if (String(ssid) != "0") {
      packet += "-" + String(ssid);
    }
    return packet;
  }

  bool decodeAX25(String frame, int frameSize, AX25Frame* decodedFrame) {
    if ((frameSize <14) || (frame[0] != KissChar::Fend && frame[1] != KissCmd::Data && frame[frameSize-1] != KissChar::Fend)) {
      return false;
    }
    int payloadFrameStart = 0;
    for (int i=0;i<frameSize;i++) {                   // where is CONTROL y PID ?
      if (frame[i] == 0x03 && frame[i+1] == 0xf0) {
        payloadFrameStart = i+1;
      }
    }
    decodedFrame->tocall = frame.substring(2,9);      // Extract destination address
    decodedFrame->sender = frame.substring(9,16);     // Extract source address
    if (payloadFrameStart >= 21) {                    // is there path1?
      decodedFrame->path1 = frame.substring(16,23);
    }
    if (payloadFrameStart >= 28) {                    // is there path2?
      decodedFrame->path2 = frame.substring(23,30);
    }
    decodedFrame->control = frame.substring(payloadFrameStart-1,payloadFrameStart);   // Extract control information  // 0x03
    decodedFrame->pid = frame.substring(payloadFrameStart,payloadFrameStart+1);       // Extract pid information      // 0xF0                                
    decodedFrame->payload = frame.substring(payloadFrameStart+1,frameSize-1);         // Extract payload
    return true;
  }

  String AX25FrameToLoRaPacket(String frame) {
    //Serial.println(frame);
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

  std::string intToBinaryString(int value, int bitLength) {
    std::string result = "";
    for (int i = bitLength - 1; i >= 0; i--) {
      result += ((value >> i) & 1) ? '1' : '0';
    }
    return result;
  }

  String encodeAX25Address(String frame, int type, bool lastAddress) {
    String packet = "";
    String address;
    std::string concatenatedBinary;
    int ssid;
    if (frame.indexOf("-")>0) {
      address = frameCleaning(frame.substring(0,frame.indexOf("-")));
      ssid = frame.substring(frame.indexOf("-")+1).toInt();
      if (ssid>15) {
        ssid = 0;
      }
    } else {
      address = frameCleaning(frame);
      ssid = 0;
    }
    for (int j=0;j<6;j++) {
      char c = address[j];
      packet += char(c<<1);
    }
    std::string firstSSIDBit = std::to_string(type); //type=0 (sender or path not repeated) type=1 (tocall or path bein repeated)
    std::string lastSSIDBit = "0";
    if (lastAddress) {
      lastSSIDBit = "1";            // address is the last from AX.25 Frame
    }
    concatenatedBinary = firstSSIDBit + "11" + intToBinaryString(ssid,4) + lastSSIDBit; // ( CRRSSSSX / HRRSSSSX )
    long decimalValue = strtol(concatenatedBinary.c_str(), NULL, 2);
    packet += (char)decimalValue;   //SSID 
    return packet;
  }

  String LoRaPacketToAX25Frame(String packet) {
    String encodedPacket = "";
    String tocall = "";
    String sender = packet.substring(0,packet.indexOf(">"));
    bool lastAddress = false;
    String payload = packet.substring(packet.indexOf(":")+1);
    String temp = packet.substring(packet.indexOf(">")+1, packet.indexOf(":"));

    if (temp.indexOf(",")>0) {    
      tocall = temp.substring(0,temp.indexOf(","));
      temp = temp.substring(temp.indexOf(",")+1);
    } else {
      tocall = temp;
      temp = "";
      lastAddress = true;
    }
    encodedPacket = encodeAX25Address(tocall, 1, false);
    encodedPacket += encodeAX25Address(sender, 0, lastAddress);

    while (temp.length() > 0) {
      int repeatedPath = 0;
      String address = "";
      if (temp.indexOf(",")>0) {
        address = temp.substring(0,temp.indexOf(","));
        temp = temp.substring(temp.indexOf(",")+1);
      } else {
        address = temp;
        temp = "";
        lastAddress = true;        
      }
      if (address.indexOf("*")>0) {
        repeatedPath = 1;
      }
      encodedPacket += encodeAX25Address(address, repeatedPath, lastAddress);
    }

    encodedPacket += char(0x03);
    encodedPacket += char(0xF0);
    encodedPacket += packet.substring(packet.indexOf(":")+1);
    return encodedPacket;
  }

}