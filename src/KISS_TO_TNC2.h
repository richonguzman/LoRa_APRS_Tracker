#include <Arduino.h>

String encode_kiss(const String& tnc2FormattedFrame);
String decode_kiss(const String &inputKISSTNCFrame, bool &dataFrame);

String encapsulateKISS(const String &ax25Frame, uint8_t TNCCmd);