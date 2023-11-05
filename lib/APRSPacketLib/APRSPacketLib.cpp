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

    APRSPacket processReceivedPacket(String receivedPacket) {
      APRSPacket aprsPacket;
      aprsPacket.sender = receivedPacket.substring(0,receivedPacket.indexOf(">"));
      if (receivedPacket.indexOf("::") > 10) {
        aprsPacket.type = "message";
        String temp = receivedPacket.substring(receivedPacket.indexOf("::")+2);
        String temp2 = temp.substring(0,temp.indexOf(":"));
        temp2.trim();
        aprsPacket.addressee = temp2;
        aprsPacket.message = temp.substring(temp.indexOf(":")+1);
      } else if (receivedPacket.indexOf(":!") > 10 || receivedPacket.indexOf(":=") > 10 ) {
        aprsPacket.type = "gps";
        aprsPacket.addressee = "";
        aprsPacket.message = "";
      } else if (receivedPacket.indexOf(":>") > 10) {
        aprsPacket.type = "status";
        aprsPacket.addressee = "";
        aprsPacket.message = "";
      } else if (receivedPacket.indexOf(":T#") >= 10 && receivedPacket.indexOf(":=/") == -1) {
        aprsPacket.type = "telemetry";
        aprsPacket.addressee = "";
        aprsPacket.message = "";
      }
      Serial.println("Struct APRS PACKET procesado");
      return aprsPacket;
    }

}