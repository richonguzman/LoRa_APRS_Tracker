#ifndef AX25_UTILS_H_
#define AX25_UTILS_H_

#include <Arduino.h>

namespace AX25_Utils {

    String          encodeKISS(const String& frame);
    String          decodeKISS(const String& inputFrame, bool& dataFrame);

}

#endif