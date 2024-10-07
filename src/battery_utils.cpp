#include <Arduino.h>
#include "battery_utils.h"
#include "power_utils.h"


int telemetryCounter    = random(1,999);


namespace BATTERY_Utils {

    String generateEncodedTelemetryBytes(float value, bool firstBytes, byte voltageType) {  // 0 = internal battery(0-4,2V) , 1 = external battery(0-15V)
        String encodedBytes;
        int tempValue;

        if (firstBytes) {
            tempValue = value;
        } else {
            switch (voltageType) {
                case 0:
                    tempValue = value * 100;           // Internal voltage calculation
                    break;
                case 1:
                    tempValue = (value * 100) / 2;     // External voltage calculation
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
        telemetry += generateEncodedTelemetryBytes(voltage, false, 0);
        telemetry += "|";
        return telemetry;
    }

    String getPercentVoltageBattery(float voltage) {
        int percent = ((voltage - 3.0) / (4.2 - 3.0)) * 100;
        if (percent < 10) {
            return "  " + String(percent);
        } else if (percent >= 10 && percent < 100) {
            return " " + String(percent);
        } else {
            return "100";
        }
    }

}