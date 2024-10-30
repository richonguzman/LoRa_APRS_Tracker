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

namespace AX25_Utils {

    String          decodeFrame(const String& frame);
    bool            decodeAX25(const String& frame, int frameSize, AX25Frame* decodedFrame);
    String          AX25FrameToLoRaPacket(const String& frame);

    String          encodeKISS(const String& frame);
}

#endif