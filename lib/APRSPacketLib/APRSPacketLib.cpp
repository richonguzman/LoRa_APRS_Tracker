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

    float decodeEncodedLatitude(String receivedPacket) {
      String packet = receivedPacket.substring(receivedPacket.indexOf(":!")+3);
      String encodedLatitude = packet.substring(0,4);
      int Y1 = int(encodedLatitude[0]);
      int Y2 = int(encodedLatitude[1]);
      int Y3 = int(encodedLatitude[2]);
      int Y4 = int(encodedLatitude[3]);
      return (90.0 - ((((Y1-33) * pow(91,3)) + ((Y2-33) * pow(91,2)) + ((Y3-33) * 91) + Y4-33) / 380926.0));
    }

    float decodeEncodedLongitude(String receivedPacket) {
      String packet = receivedPacket.substring(receivedPacket.indexOf(":!")+3);
      String encodedLongtitude = packet.substring(4,8);
      int X1 = int(encodedLongtitude[0]);
      int X2 = int(encodedLongtitude[1]);
      int X3 = int(encodedLongtitude[2]);
      int X4 = int(encodedLongtitude[3]);
      return (-180.0 + ((((X1-33) * pow(91,3)) + ((X2-33) * pow(91,2)) + ((X3-33) * 91) + X4-33) / 190463.0));
    }

    float decodeLatitude(String receivedPacket) {
      String gpsData;
      if (receivedPacket.indexOf(":!") > 10) {
        gpsData = receivedPacket.substring(receivedPacket.indexOf(":!")+2);
      } else if (receivedPacket.indexOf(":=") > 10) {
        gpsData = receivedPacket.substring(receivedPacket.indexOf(":=")+2);
      }
      String Latitude         = gpsData.substring(0,8);
      String firstLatPart     = Latitude.substring(0,2);
      String secondLatPart    = Latitude.substring(2,4);
      String thirdLatPart     = Latitude.substring(Latitude.indexOf(".")+1,Latitude.indexOf(".")+3);
      float convertedLatitude = firstLatPart.toFloat() + (secondLatPart.toFloat()/60) + (thirdLatPart.toFloat()/(60*100));
      String LatSign          = String(Latitude[7]);
      if (LatSign == "S") {
        return -convertedLatitude;;
      } else {
        return convertedLatitude;
      }
    }

    float decodeLongitude(String receivedPacket) {
      String gpsData;
      if (receivedPacket.indexOf(":!") > 10) {
        gpsData = receivedPacket.substring(receivedPacket.indexOf(":!")+2);
      } else if (receivedPacket.indexOf(":=") > 10) {
        gpsData = receivedPacket.substring(receivedPacket.indexOf(":=")+2);
      }
      String Longitude          = gpsData.substring(9,18);
      String firstLngPart       = Longitude.substring(0,3);
      String secondLngPart      = Longitude.substring(3,5);
      String thirdLngPart       = Longitude.substring(Longitude.indexOf(".")+1,Longitude.indexOf(".")+3);
      float convertedLongitude  = firstLngPart.toFloat() + (secondLngPart.toFloat()/60) + (thirdLngPart.toFloat()/(60*100));
      String LngSign            = String(Longitude[8]);
      if (LngSign == "W") {
        return -convertedLongitude;
      } else {
        return convertedLongitude;
      }
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
        aprsPacket.latitude = 0;
        aprsPacket.longitude = 0;
      } else if (receivedPacket.indexOf(":!") > 10 || receivedPacket.indexOf(":=") > 10 ) {
        aprsPacket.type = "gps";
        aprsPacket.addressee = "";
        aprsPacket.message = "";
        int encodedBytePosition = 0;
        if (receivedPacket.indexOf(":!") > 10) {
          encodedBytePosition = receivedPacket.indexOf(":!") + 14;
        }
        if (receivedPacket.indexOf(":=") > 10) {
          encodedBytePosition = receivedPacket.indexOf(":=") + 14;
        }
        if (encodedBytePosition != 0) {
          if (String(receivedPacket[encodedBytePosition]) == "G" || String(receivedPacket[encodedBytePosition]) == "Q" || String(receivedPacket[encodedBytePosition]) == "[" || String(receivedPacket[encodedBytePosition]) == "H") {
            aprsPacket.latitude = decodeEncodedLatitude(receivedPacket);
            aprsPacket.longitude = decodeEncodedLongitude(receivedPacket);
          } else {
            Serial.println(" gps no codificado");
            aprsPacket.latitude = decodeLatitude(receivedPacket);
            aprsPacket.longitude = decodeLongitude(receivedPacket);
          }
          //
          Serial.print(aprsPacket.sender);
          Serial.print(" GPS : ");
          Serial.print(aprsPacket.latitude); Serial.print(" N ");
          Serial.print(aprsPacket.longitude);Serial.println(" E");
          //
        }
      } else if (receivedPacket.indexOf(":>") > 10) {
        aprsPacket.type = "status";
        aprsPacket.addressee = "";
        aprsPacket.message = "";
        aprsPacket.latitude = 0;
        aprsPacket.longitude = 0;
      } else if (receivedPacket.indexOf(":T#") >= 10 && receivedPacket.indexOf(":=/") == -1) {
        aprsPacket.type = "telemetry";
        aprsPacket.addressee = "";
        aprsPacket.message = "";
        aprsPacket.latitude = 0;
        aprsPacket.longitude = 0;
      }
      return aprsPacket;
    }

}