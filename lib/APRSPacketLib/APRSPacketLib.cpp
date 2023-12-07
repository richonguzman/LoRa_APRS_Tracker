#include "APRSPacketLib.h"

namespace APRSPacketLib {

    String generateBasePacket(String callsign, String tocall, String path) {
      String packet = callsign + ">" + tocall;
      if (path != "") {
        packet += "," + path;
      }
      return packet;
    }

    String generateStatusPacket(String callsign, String tocall, String path, String status) {
      return generateBasePacket(callsign,tocall,path) + ":>"  + status;
    }

    String generateMessagePacket(String callsign, String tocall, String path, String addressee, String message) {
      for(int i = addressee.length(); i < 9; i++) {
        addressee += ' ';
      }      
      return generateBasePacket(callsign,tocall,path) + "::" + addressee + ":" + message;
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
            case 4: // mic-e
              repeatedPacket += ":'";
              break;
            case 5: // object
              repeatedPacket += ":;";
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
      return generateBasePacket(callsign,tocall,path) + ":!" + overlay + gps;
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

    int decodeEncodedCourse(String course) {
      return (course.toInt() - 33) * 4;
    }

    int decodeEncodedSpeed(String speed) {
      return pow(1.08,(speed.toInt() - 33)) - 1;
    }

    int decodeEncodedAltitude(String altitude) {
      char cLetter = altitude[0];
      char sLetter = altitude[1];
      int c = static_cast<int>(cLetter);
      int s = static_cast<int>(sLetter);
      return pow(1.002,((c - 33) * 91) + (s-33)) * 0.3048;
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

    int decodeCourse(String course) {
      if (course == "..." || course == "000") {
        return 0;
      } else { 
        return course.toInt();
      }
    }

    int decodeSpeed(String speed) {
      return speed.toInt() * 1.852;
    }

    int decodeAltitude(String altitude) {
      return altitude.toInt() * 0.3048;
    }

    APRSPacket processReceivedPacket(String receivedPacket) {
      /*  Packet type:
          gps       = 0
          message   = 1
          status    = 2
          telemetry = 3
          mic-e     = 4
          object    = 5   */
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
          aprsPacket.symbol = receivedPacket.substring(receivedPacket.indexOf(gpsChar)+11, receivedPacket.indexOf(gpsChar)+12);
          if (receivedPacket.substring(receivedPacket.indexOf(gpsChar)+12, receivedPacket.indexOf(gpsChar)+13) == " ") {
            aprsPacket.course = 0;
            aprsPacket.speed = 0;
            aprsPacket.altitude = 0;
          } else {
            if (String(receivedPacket[encodedBytePosition]) == "Q") { // altitude csT
              aprsPacket.altitude = decodeEncodedAltitude(receivedPacket.substring(receivedPacket.indexOf(gpsChar)+12, receivedPacket.indexOf(gpsChar)+14));
              aprsPacket.course = 0;
              aprsPacket.speed = 0;
            } else { // normal csT
              aprsPacket.course = decodeEncodedCourse(receivedPacket.substring(receivedPacket.indexOf(gpsChar)+12, receivedPacket.indexOf(gpsChar)+13));
              aprsPacket.speed = decodeEncodedSpeed(receivedPacket.substring(receivedPacket.indexOf(gpsChar)+13, receivedPacket.indexOf(gpsChar)+14));
              aprsPacket.altitude = 0;
            }
          }
        } else {
          aprsPacket.latitude = decodeLatitude(receivedPacket.substring(receivedPacket.indexOf(gpsChar)+2,receivedPacket.indexOf(gpsChar)+10));
          aprsPacket.longitude = decodeLongitude(receivedPacket.substring(receivedPacket.indexOf(gpsChar)+11,receivedPacket.indexOf(gpsChar)+20));
          aprsPacket.symbol = receivedPacket.substring(receivedPacket.indexOf(gpsChar)+20,receivedPacket.indexOf(gpsChar)+21);
          if (receivedPacket.substring(receivedPacket.indexOf(gpsChar)+24,receivedPacket.indexOf(gpsChar)+25) == "/" && receivedPacket.substring(receivedPacket.indexOf(gpsChar)+28,receivedPacket.indexOf(gpsChar)+31) == "/A=") {
            aprsPacket.course = decodeCourse(receivedPacket.substring(receivedPacket.indexOf(gpsChar)+21,receivedPacket.indexOf(gpsChar)+24));
            aprsPacket.speed = decodeSpeed(receivedPacket.substring(receivedPacket.indexOf(gpsChar)+25,receivedPacket.indexOf(gpsChar)+28));
            aprsPacket.altitude = decodeAltitude(receivedPacket.substring(receivedPacket.indexOf(gpsChar)+31,receivedPacket.indexOf(gpsChar)+39));
          } else {
            aprsPacket.course = 0;
            aprsPacket.speed = 0;
            aprsPacket.altitude = 0;
          }
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
        aprsPacket.message = receivedPacket.substring(receivedPacket.indexOf(":>")+2);
      } else if (receivedPacket.indexOf(":T#") >= 10 && receivedPacket.indexOf(":=/") == -1) {
        aprsPacket.type = 3;
        aprsPacket.message = receivedPacket.substring(receivedPacket.indexOf(":T#")+3);
      } else if (receivedPacket.indexOf(":'") > 10) {
        aprsPacket.type = 4;
        aprsPacket.message = receivedPacket.substring(receivedPacket.indexOf(":'")+2);
      } else if (receivedPacket.indexOf(":;") > 10) {
        aprsPacket.type = 5;
        aprsPacket.message = receivedPacket.substring(receivedPacket.indexOf(":;")+2);
      }
      if (aprsPacket.type==2 || aprsPacket.type==3 || aprsPacket.type==4 || aprsPacket.type==5) {
        aprsPacket.addressee = "";
        aprsPacket.latitude = 0;
        aprsPacket.longitude = 0;
      } 
      return aprsPacket;
    }

}