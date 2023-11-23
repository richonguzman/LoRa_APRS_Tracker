#ifndef AX25_UTILS_H_
#define AX25_UTILS_H_

#include <Arduino.h>

struct AX25Frame {  // Define AX.25 frame structure
  char tocall[7];   // destination
  char sender[7];   // source;
  char path1[7];    // if present
  char path2[7];    // if present
  char control;
  char pid;
  char payload[64]; // how to validate this size?
};

enum KissSpecialCharacter {
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
  };

namespace AX25_Utils {

  String decodeFrame(char frame[]);
  bool decodeAX25(byte* frame, int frameSize, AX25Frame* decodedFrame);
  String processAX25(byte* frame);

}

#endif