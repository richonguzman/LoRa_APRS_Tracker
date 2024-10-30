#ifndef KISS_UTILS_H
#define KISS_UTILS_H

#include <Arduino.h>


enum KissChar {
    FEND                = 0xc0,
    FESC                = 0xdb,
    TFEND               = 0xdc,
    TFESC               = 0xdd
};

enum KissCmd {
    Data                = 0x00
};

enum AX25Char {
    ControlField        = 0x03,
    InformationField    = 0xF0
};

#define HAS_BEEN_DIGIPITED_MASK         0b10000000
#define IS_LAST_ADDRESS_POSITION_MASK   0b1


namespace KISS_Utils {

    bool validateTNC2Frame(const String& tnc2FormattedFrame);
    bool validateKISSFrame(const String& kissFormattedFrame);

    String encodeKISS(const String& frame);
    String decodeKISS(const String& inputFrame, bool& dataFrame);
  
    //String encapsulateKISS(const String& ax25Frame, uint8_t command);
    //String decapsulateKISS(const String& frame);
    
}

#endif
