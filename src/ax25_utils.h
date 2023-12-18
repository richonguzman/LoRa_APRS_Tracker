#ifndef AX25_UTILS_H_
#define AX25_UTILS_H_

#include <Arduino.h>

struct AX25Frame {  // Define AX.25 frame structure
  String tocall;    // destination
  String sender;    // source
  String path1;     // if present
  String path2;     // if present
  String control;
  String pid;
  String payload;
};

enum KissChar {
    Fend = 0xc0,
    Fesc = 0xdb,
    Tfend = 0xdc,
    Tfesc = 0xdd
  };

enum KissCmd {
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
  };

namespace AX25_Utils {

  String decodeFrame(String frame);
  bool decodeAX25(String frame, int frameSize, AX25Frame* decodedFrame);
  String AX25FrameToLoRaPacket(String frame);
  String frameCleaning(String frame);
  std::string intToBinaryString(int value, int bitLength);
  String encodeAX25Address(String frame, int type, bool lastAddress);
  String LoRaPacketToAX25Frame(String packet);

}

#endif