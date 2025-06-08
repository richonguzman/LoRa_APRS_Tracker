#ifndef TELEMETRY_UTILS_H_
#define TELEMETRY_UTILS_H_

#include <Arduino.h>


namespace TELEMETRY_Utils {

    String  generateEncodedTelemetry(float voltage);
    
}

#endif