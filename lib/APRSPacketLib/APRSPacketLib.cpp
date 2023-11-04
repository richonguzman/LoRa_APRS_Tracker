#include "APRSPacketLib.h"

namespace APRSPacketLib {

    String generateStatusPacket(String callsign, String tocall, String path, String status) {
        String packet = callsign + ">" + tocall;
        if (path != "") {
          packet += "," + path;
        }
        packet += ":>"  + status;
        return packet;
    }

    String generateGPSBeaconPacket(String callsign, String tocall, String path, String overlay, String gps) {
        String packet = callsign + ">" + tocall;
        if (path != "") {
          packet += "," + path;
        }
        packet += ":!" + overlay + gps;
        return packet;
    }

}