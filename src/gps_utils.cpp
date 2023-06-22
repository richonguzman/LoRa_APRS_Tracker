#include <TinyGPS++.h>
#include "gps_utils.h"
#include "pins_config.h"

extern HardwareSerial  neo6m_gps;

namespace GPS_Utils {

void setup() {
    neo6m_gps.begin(9600, SERIAL_8N1, GPS_TX, GPS_RX);
}

}