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

    String generateDigiRepeatedPacket(APRSPacket packet, String callsign) {
      if (packet.path.indexOf("WIDE1-")>=0) {
        String hop = packet.path.substring(packet.path.indexOf("WIDE1-")+6, packet.path.indexOf("WIDE1-")+7);
        if (hop.toInt()>=1 && hop.toInt()<=7) {
          if (hop.toInt()==1) {
            packet.path.replace("WIDE1-1", callsign + "*");
          } else {
            packet.path.replace("WIDE1-" + hop , callsign + "*,WIDE1-" + String(hop.toInt()-1));
          }
          String repeatedPacket = packet.sender + ">" + packet.tocall + "," + packet.path;
          switch (packet.type) { 
            case 0: // gps
              repeatedPacket += ":!";
              break;
            case 1: // message
              for(int i = packet.addressee.length(); i < 9; i++) {
                packet.addressee += ' ';
              }
              repeatedPacket += "::" + packet.addressee + ":";
              break;
            case 2: // status
              repeatedPacket += ":>";
              break;
            case 3: // telemetry
              repeatedPacket += ":T#";
              break;
          }
          return repeatedPacket + packet.message;          
        } else {
          return "X";
        }
      } else {
        return "X";
      }
    }

    char *ax25_base91enc(char *s, uint8_t n, uint32_t v) {
      for(s += n, *s = '\0'; n; n--) {
        *(--s) = v % 91 + 33;
        v /= 91;
      }
      return(s);
    }

    String encondeGPS(float latitude, float longitude, float course, float speed, String symbol, bool sendAltitude, int altitude, bool sendStandingUpdate, String packetType) {
      String encodedData;
      uint32_t aprs_lat, aprs_lon;
      aprs_lat = 900000000 - latitude * 10000000;
      aprs_lat = aprs_lat / 26 - aprs_lat / 2710 + aprs_lat / 15384615;
      aprs_lon = 900000000 + longitude * 10000000 / 2;
      aprs_lon = aprs_lon / 26 - aprs_lon / 2710 + aprs_lon / 15384615;

      String Ns, Ew, helper;
      if(latitude < 0) { Ns = "S"; } else { Ns = "N"; }
      if(latitude < 0) { latitude= -latitude; }

      if(longitude < 0) { Ew = "W"; } else { Ew = "E"; }
      if(longitude < 0) { longitude= -longitude; }

      char helper_base91[] = {"0000\0"};
      int i;
      ax25_base91enc(helper_base91, 4, aprs_lat);
      for (i=0; i<4; i++) {
        encodedData += helper_base91[i];
      }
      ax25_base91enc(helper_base91, 4, aprs_lon);
      for (i=0; i<4; i++) {
        encodedData += helper_base91[i];
      }
      if (packetType=="Wx") {
        encodedData += "_";
      } else {
        encodedData += symbol;
      }

      if (sendAltitude) {           // Send Altitude or... (APRS calculates Speed also)
        int Alt1, Alt2;
        if(altitude>0) {
          double ALT=log(altitude)/log(1.002);
          Alt1= int(ALT/91);
          Alt2=(int)ALT%91;
        } else {
          Alt1=0;
          Alt2=0;
        }
        if (sendStandingUpdate) {
          encodedData += " ";
        } else {
          encodedData +=char(Alt1+33);
        }
        encodedData +=char(Alt2+33);
        encodedData +=char(0x30+33);
      } else {                      // ... just send Course and Speed
        ax25_base91enc(helper_base91, 1, (uint32_t) course/4 );
        if (sendStandingUpdate) {
          encodedData += " ";
        } else {
          encodedData += helper_base91[0];
        }
        ax25_base91enc(helper_base91, 1, (uint32_t) (log1p(speed)/0.07696));
        encodedData += helper_base91[0];
        encodedData += "\x47";
      }
      return encodedData;
    }

    String generateGPSBeaconPacket(String callsign, String tocall, String path, String overlay, String gps) {
      String packet = callsign + ">" + tocall;
      if (path != "") {
        packet += "," + path;
      }
      packet += ":!" + overlay + gps;
      return packet;
    }

    float decodeEncodedLatitude(String encodedLatitude) {
      int Y1 = int(encodedLatitude[0]);
      int Y2 = int(encodedLatitude[1]);
      int Y3 = int(encodedLatitude[2]);
      int Y4 = int(encodedLatitude[3]);
      return (90.0 - ((((Y1-33) * pow(91,3)) + ((Y2-33) * pow(91,2)) + ((Y3-33) * 91) + Y4-33) / 380926.0));
    }

    float decodeEncodedLongitude(String encodedLongitude) {
      int X1 = int(encodedLongitude[0]);
      int X2 = int(encodedLongitude[1]);
      int X3 = int(encodedLongitude[2]);
      int X4 = int(encodedLongitude[3]);
      return (-180.0 + ((((X1-33) * pow(91,3)) + ((X2-33) * pow(91,2)) + ((X3-33) * 91) + X4-33) / 190463.0));
    }

    float decodeLatitude(String Latitude) {
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

    float decodeLongitude(String Longitude) {
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
      /*  Packet type:
          gps       = 0
          message   = 1
          status    = 2
          telemetry = 3   */
      APRSPacket aprsPacket;
      aprsPacket.sender = receivedPacket.substring(0,receivedPacket.indexOf(">"));
      String temp00 = receivedPacket.substring(receivedPacket.indexOf(">")+1,receivedPacket.indexOf(":"));
      if (temp00.indexOf(",") > 2) {
        aprsPacket.tocall = temp00.substring(0,temp00.indexOf(","));
        aprsPacket.path = temp00.substring(temp00.indexOf(",")+1,temp00.indexOf(":"));
      } else {
        aprsPacket.tocall = temp00;
        aprsPacket.path = "";
      }
      if (receivedPacket.indexOf(":!") > 10 || receivedPacket.indexOf(":=") > 10 ) {
        aprsPacket.type = 0;
        aprsPacket.addressee = "";
        String gpsChar = "";
        if (receivedPacket.indexOf(":!") > 10) {
          gpsChar = ":!";
        } else {
          gpsChar = ":=";
        }
        int encodedBytePosition = receivedPacket.indexOf(gpsChar) + 14;
        aprsPacket.message = receivedPacket.substring(receivedPacket.indexOf(gpsChar)+2);
        if (String(receivedPacket[encodedBytePosition]) == "G" || String(receivedPacket[encodedBytePosition]) == "Q" || String(receivedPacket[encodedBytePosition]) == "[" || String(receivedPacket[encodedBytePosition]) == "H") {
          aprsPacket.latitude = decodeEncodedLatitude(receivedPacket.substring(receivedPacket.indexOf(gpsChar)+3, receivedPacket.indexOf(gpsChar)+7));
          aprsPacket.longitude = decodeEncodedLongitude(receivedPacket.substring(receivedPacket.indexOf(gpsChar)+7, receivedPacket.indexOf(gpsChar)+11));
        } else {
          aprsPacket.latitude = decodeLatitude(receivedPacket.substring(receivedPacket.indexOf(gpsChar)+2,receivedPacket.indexOf(gpsChar)+10));
          aprsPacket.longitude = decodeLongitude(receivedPacket.substring(receivedPacket.indexOf(gpsChar)+11,receivedPacket.indexOf(gpsChar)+20));
        }
      } else if (receivedPacket.indexOf("::") > 10) {
        aprsPacket.type = 1;
        String temp1 = receivedPacket.substring(receivedPacket.indexOf("::")+2);
        String temp2 = temp1.substring(0,temp1.indexOf(":"));
        temp2.trim();
        aprsPacket.addressee = temp2;
        aprsPacket.message = temp1.substring(temp1.indexOf(":")+1);
        aprsPacket.latitude = 0;
        aprsPacket.longitude = 0;
      } else if (receivedPacket.indexOf(":>") > 10) {
        aprsPacket.type = 2;
        aprsPacket.addressee = "";
        aprsPacket.message = receivedPacket.substring(receivedPacket.indexOf(":>")+2);
        aprsPacket.latitude = 0;
        aprsPacket.longitude = 0;
      } else if (receivedPacket.indexOf(":T#") >= 10 && receivedPacket.indexOf(":=/") == -1) {
        aprsPacket.type = 3;
        aprsPacket.addressee = "";
        aprsPacket.message = receivedPacket.substring(receivedPacket.indexOf(":T#")+3);
        aprsPacket.latitude = 0;
        aprsPacket.longitude = 0;
      }
      return aprsPacket;
    }

}