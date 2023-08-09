#include <Arduino.h>
#include "KISS.h"

#define APRS_CONTROL_FIELD 0x03
#define APRS_INFORMATION_FIELD 0xf0

#define HAS_BEEN_DIGIPITED_MASK 0b10000000
#define IS_LAST_ADDRESS_POSITION_MASK 0b1

bool validateTNC2Frame(const String &tnc2FormattedFrame);
bool validateKISSFrame(const String &kissFormattedFrame);

String encode_kiss(const String& tnc2FormattedFrame);
String decode_kiss(const String &inputKISSTNCFrame, bool &dataFrame);

String encapsulateKISS(const String &ax25Frame, uint8_t TNCCmd);