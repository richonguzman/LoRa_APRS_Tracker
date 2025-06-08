#include <Arduino.h>
#include "telemetry_utils.h"


int telemetryCounter = random(1,999);


namespace TELEMETRY_Utils {

    String generateEncodedTelemetryBytes(float value, bool counterBytes, byte telemetryType) {  // 0 = internal battery(0-4,2V) , 1 = external battery(0-15V)
        String encodedBytes;
        int tempValue;

        if (counterBytes) {
            tempValue = value;
        } else {
            switch (telemetryType) {
                case 0:
                    tempValue = value * 100;           // Internal voltage or Humidity calculation
                    break;
                case 1:
                    tempValue = (value * 100) / 2;     // External voltage calculation
                    break;
                case 2:
                    tempValue = (value * 10) + 500;     // Temperature
                    break;
                case 3:
                    tempValue = (value * 8);            // Pressure
                    break;
                default:
                    tempValue = value;
                    break;
            }
        }        

        int firstByte   = tempValue / 91;
        tempValue       -= firstByte * 91;

        encodedBytes    = char(firstByte + 33);
        encodedBytes    += char(tempValue + 33);
        return encodedBytes;
    }

    String generateEncodedTelemetry(float voltage) {
        String telemetry = "|";
        telemetry += generateEncodedTelemetryBytes(telemetryCounter, true, 0);
        telemetryCounter++;
        if (telemetryCounter == 1000) {
            telemetryCounter = 0;
        }
        //
        bool telemetryNotAsWxPacket = true;
        //
        if (telemetryNotAsWxPacket) {
            telemetry += generateEncodedTelemetryBytes(24.8, false, 2);     // temperature
            telemetry += generateEncodedTelemetryBytes(63, false, 0);       // humidity
            telemetry += generateEncodedTelemetryBytes(1015.5, false, 3);   // pressure
        }
        //
        telemetry += generateEncodedTelemetryBytes(voltage, false, 0);
        telemetry += "|";
        return telemetry;
    }

}