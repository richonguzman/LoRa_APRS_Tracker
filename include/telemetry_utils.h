#ifndef TELEMETRY_UTILS_H_
#define TELEMETRY_UTILS_H_

#include <Arduino.h>


namespace TELEMETRY_Utils {

    void    sendEquationsUnitsParameters();
    String  generateEncodedTelemetryBytes(float value, bool counterBytes, byte telemetryType);
    String  generateEncodedTelemetry();
    
}

#endif