#include "APRSPacketLib.h"

namespace APRSPacketLib {

  String doubleToString(double n, int ndec) {
    String r = "";
    if (n>-1 && n<0) {
      r = "-";
    }   
    int v = n;
    r += v;
    r += '.';
    for (int i=0;i<ndec;i++) {
      n -= v;
      n = 10 * abs(n);
      v = n;
      r += v;
    }
    return r;
  }

  String gpsDecimalToDegreesLatitude(double lat) {
    String degrees = doubleToString(lat,6);
    String north_south, latitude, convDeg3;
    float convDeg, convDeg2;

    if (abs(degrees.toFloat()) < 10) {
      latitude += "0";
    }
    if (degrees.indexOf("-") == 0) {
      north_south = "S";
      latitude += degrees.substring(1,degrees.indexOf("."));
    } else {
      north_south = "N";
      latitude += degrees.substring(0,degrees.indexOf("."));
    }
    convDeg = abs(degrees.toFloat()) - abs(int(degrees.toFloat()));
    convDeg2 = (convDeg * 60)/100;
    convDeg3 = String(convDeg2,6);
    latitude += convDeg3.substring(convDeg3.indexOf(".")+1,convDeg3.indexOf(".")+3) + "." + convDeg3.substring(convDeg3.indexOf(".")+3,convDeg3.indexOf(".")+5);
    latitude += north_south;
    return latitude;
  }

  String gpsDecimalToDegreesLongitude(double lon) {
    String degrees = doubleToString(lon,6);
    String east_west, longitude, convDeg3;
    float convDeg, convDeg2;
    
    if (abs(degrees.toFloat()) < 100) {
      longitude += "0";
    }
    if (abs(degrees.toFloat()) < 10) {
      longitude += "0";
    }
    if (degrees.indexOf("-") == 0) {
      east_west = "W";
      longitude += degrees.substring(1,degrees.indexOf("."));
    } else {
      east_west = "E";
      longitude += degrees.substring(0,degrees.indexOf("."));
    }
    convDeg = abs(degrees.toFloat()) - abs(int(degrees.toFloat()));
    convDeg2 = (convDeg * 60)/100;
    convDeg3 = String(convDeg2,6);
    longitude += convDeg3.substring(convDeg3.indexOf(".")+1,convDeg3.indexOf(".")+3) + "." + convDeg3.substring(convDeg3.indexOf(".")+3,convDeg3.indexOf(".")+5);
    longitude += east_west;
    return longitude;
  }

  void encodeMiceAltitude(uint8_t *buf, uint32_t alt_m) {
    if (alt_m>40000) {
      alt_m = 0;
    }
    uint32_t altoff;
    altoff=alt_m+10000;
    buf[0]=(altoff/8281)+33;
    altoff=altoff%8281;
    buf[1]=(altoff/91)+33;	
    buf[2]=(altoff%91)+33;
    buf[3]='}';
  }

  void encodeMiceCourseSpeed(uint8_t *buf, uint32_t speed_kt, uint32_t course_deg) {
    uint32_t SP28,DC28,SE28; //three bytes are output

    uint32_t ten = speed_kt / 10;
    if (ten <= 19) {
      SP28 = ten + 108;
    } else if (ten <= 79) {
      SP28 = ten + 28;
    } else {
      SP28 = 107;
    }
    buf[0]=SP28;

    if (course_deg == 0) {
      course_deg = 360;
    } else if (course_deg >= 360) {
      course_deg = 0;
    }
    uint32_t course_hun = course_deg/100;
    DC28=(speed_kt-ten*10)*10 + course_hun + 32;
    buf[1]=DC28;				

    SE28=(course_deg-course_hun*100) + 28;
	  buf[2]=SE28;
  }

  void encodeMiceLongitude(uint8_t *buf, gpsLongitudeStruct *lon) { 
    uint32_t d28;               // degrees
    uint32_t deg = lon->degrees;
    if (deg<=9) {
      d28=118+deg;
    } else if (deg<=99) {
      d28=28+deg;
    } else if (deg<=109) {
      d28=8+deg;
    } else {
      d28=28+(deg-100);
    }
    buf[0] = d28;
    
    uint32_t m28;               // minutes
    uint32_t min = lon->minutes;
    if (min<=9) {
      m28=88+min;
    } else {
      m28=28+min;
    }
    buf[1]=m28;
    
    uint32_t h28;
    h28=28+lon->minuteHundredths;
    buf[2]=h28;
  }

  void encodeMiceDestinationField(String msgType, uint8_t *buf, const gpsLatitudeStruct *lat, gpsLongitudeStruct *lon) {
    uint32_t temp;
    temp = lat->degrees/10;             // degrees
    buf[0] = (temp + 0x30);
    if (msgType[0] == '1') {
      buf[0] = buf[0] + 0x20;
    }
    buf[1] = (lat->degrees-temp*10 + 0x30);
    if (msgType[1] == '1') {
      buf[1] = buf[1] + 0x20;
    }

    temp = lat->minutes/10;             // minutes
    buf[2] = (temp + 0x30);
    if (msgType[2] == '1') {
      buf[2] = buf[2] + 0x20;
    }
    buf[3] = (lat->minutes - temp*10 + 0x30) + (lat->north ? 0x20 : 0);             // North validation

    temp = lat->minuteHundredths/10;            // minute hundredths
    buf[4] = (temp + 0x30) + ((lon->degrees >= 100 || lon->degrees <= 9) ? 0x20 : 0);   // Longitude Offset
    buf[5] = (lat->minuteHundredths - temp*10 + 0x30) + (!lon->east ? 0x20 : 0);            // West validation
  }

  gpsLatitudeStruct gpsDecimalToDegreesMiceLatitude(float latitude) {
    gpsLatitudeStruct miceLatitudeStruct;
    String lat = gpsDecimalToDegreesLatitude(latitude);
    char latitudeArray[10];
    strncpy(latitudeArray,lat.c_str(),8);
    miceLatitudeStruct.degrees= 10*(latitudeArray[0]-'0')+latitudeArray[1]-'0';
    miceLatitudeStruct.minutes= 10*(latitudeArray[2]-'0')+latitudeArray[3]-'0';
    miceLatitudeStruct.minuteHundredths= 10*(latitudeArray[5]-'0')+latitudeArray[6]-'0';
    if (latitudeArray[7] == 'N') {
      miceLatitudeStruct.north = 1;
    } else {
      miceLatitudeStruct.north = 0;
    }
    return miceLatitudeStruct;
  }

  gpsLongitudeStruct gpsDecimalToDegreesMiceLongitude(float longitude) {
    gpsLongitudeStruct miceLongitudeStruct;
    String lng = gpsDecimalToDegreesLongitude(longitude);
    char longitudeArray[10];
    strncpy(longitudeArray,lng.c_str(),9);
    miceLongitudeStruct.degrees= 100*(longitudeArray[0]-'0')+10*(longitudeArray[1]-'0')+longitudeArray[2]-'0';
    miceLongitudeStruct.minutes= 10*(longitudeArray[3]-'0')+longitudeArray[4]-'0';
    miceLongitudeStruct.minuteHundredths= 10*(longitudeArray[6]-'0')+longitudeArray[7]-'0';
    if (longitudeArray[8] == 'E') {
      miceLongitudeStruct.east = 1;
    } else {
      miceLongitudeStruct.east = 0;
    }
    return miceLongitudeStruct;
  }

  String generateMiceGPSBeacon(String miceMsgType, String callsign, String symbol, String overlay, String path, float latitude, float longitude, float course, float speed, int altitude) {
    gpsLatitudeStruct latitudeStruct = gpsDecimalToDegreesMiceLatitude(latitude);
    gpsLongitudeStruct longitudeStruct = gpsDecimalToDegreesMiceLongitude(longitude);

    uint8_t miceDestinationArray[7];
    encodeMiceDestinationField(miceMsgType, &miceDestinationArray[0], &latitudeStruct, &longitudeStruct);
    miceDestinationArray[6] = 0x00;     // por repetidor?
    String miceDestination = (char*)miceDestinationArray;

    uint8_t miceInfoFieldArray[14];
    miceInfoFieldArray[0] = 0x60; //  0x60 for ` and 0x27 for '
    encodeMiceLongitude(&miceInfoFieldArray[1], &longitudeStruct);
    encodeMiceCourseSpeed(&miceInfoFieldArray[4], (uint32_t)speed, (uint32_t)course); //speed= gps.speed.knots(), course = gps.course.deg());

    char symbolOverlayArray[1];
    strncpy(symbolOverlayArray,symbol.c_str(),1);
    miceInfoFieldArray[7] = symbolOverlayArray[0];
    strncpy(symbolOverlayArray,overlay.c_str(),1);
    miceInfoFieldArray[8] = symbolOverlayArray[0];
    
    encodeMiceAltitude(&miceInfoFieldArray[9], (uint32_t)altitude); // altitude = gps.altitude.meters()
    miceInfoFieldArray[13] = 0x00;      // por repetidor?
    String miceInformationField = (char*)miceInfoFieldArray;

    String miceAPRSPacket = callsign + ">" + miceDestination;
    if (path != "") {
      miceAPRSPacket += "," + path;
    }
    miceAPRSPacket += ":" + miceInformationField;
    return miceAPRSPacket;
}

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
            repeatedPacket += ":`";
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
    return (pow(1.08,(speed.toInt() - 33)) - 1) * 1.852;
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

  String decodeMiceMsgType(String tocall) {
    String miceType;
    for (int i=0; i<3; i++) {
      if (int(tocall[i]) > 57) {
        miceType += "1";
      } else {
        miceType += "0";
      }
    }
    return miceType;
  }

  String decodeMiceSymbol(String informationField) {
    return informationField.substring(6,7);
  }

  String decodeMiceOverlay(String informationField) {
    return informationField.substring(7,8);
  }

  int decodeMiceSpeed(String informationField) {
    int temp = int(informationField[3]);
    if (temp>107) {
      temp -= 80;
    }
    int SP28 = (temp-28)*10;
    int DC28 = (int(informationField[4]) - 28)/10;
    return (SP28 + DC28) * 1.852;
  }

  int decodeMiceCourse(String informationField) {
    int DC28 = (int(informationField[4]) - 28)/10;
    int temp = (int(informationField[4]) - 28) - (DC28*10);
    int SE28 = int(informationField[5]) - 28;
    int course = ((temp - 4) * 100) + SE28;
    return course;
  }

  int decodeMiceAltitude(String informationField) {
    int altitude;
    String temp;
    if (informationField.indexOf("`")==8 && informationField.indexOf("}")==12) {
      temp = informationField.substring(9,12);
    } else if (informationField.indexOf("}")==11) {
      temp = informationField.substring(8,11);
    } else {
      temp = "";
    }
    if (temp.length()!=0) {
      int a = int(temp[0]) - 33;
      int b = int(temp[1]) - 33;
      int c = int(temp[2]) - 33;
      altitude = (a*pow(91,2) + b*91 + c) - 10000;
    } else {
      altitude = 0;
    }
    return altitude;
  }

  float gpsDegreesToDecimalLatitude(String degreesLatitude) {
    int degrees = degreesLatitude.substring(0,2).toInt();
    int minute = degreesLatitude.substring(2,4).toInt();
    int minuteHundredths = degreesLatitude.substring(5,7).toInt();
    String northSouth = degreesLatitude.substring(7);
    float decimalLatitude = degrees + (minute/60.0) + (minuteHundredths/10000.0);
    if (northSouth=="N") {
      return decimalLatitude;
    } else { 
      return -decimalLatitude;
    }
  }

  float gpsDegreesToDecimalLongitude(String degreesLongitude) {
    int degrees = degreesLongitude.substring(0,3).toInt();
    int minute = degreesLongitude.substring(3,5).toInt();
    int minuteHundredths = degreesLongitude.substring(6,8).toInt();
    String westEast = degreesLongitude.substring(8);
    float decimalLongitude = degrees + (minute/60.0) + (minuteHundredths/10000.0);
    if (westEast=="W") {
      return -decimalLongitude;
    } else { 
      return decimalLongitude;
    }
  }

  float decodeMiceLatitude(String destinationField) {
    String gpsLat;
    String northSouth;
    for (int i=0; i<=5; i++) {
      if (int(destinationField[i]) > 57) {
        gpsLat += char(int(destinationField[i]) - 32);
      } else {
        gpsLat += char(int(destinationField[i]));
      }
      if (i==3) {
        gpsLat += ".";
        if (int(destinationField[i]) > 57) {
          northSouth = "N";
        } else {
          northSouth = "S";
        }
      }
    }
    gpsLat += northSouth;
    return gpsDegreesToDecimalLatitude(gpsLat);
  }

  float decodeMiceLongitude(String destinationField, String informationField) {
    bool offset = false;
    String westEast = "E";
    if (int(destinationField[4]) > 57) {
      offset = true;
    } 
    if (int(destinationField[5]) > 57) {
      westEast = "W";
    }

    String longitudeString, temp;
    int d28 = (int)informationField[0] - 28;
    if (offset) {
      d28 += 100;
    }
    temp = String(d28);
    for(int i = temp.length(); i < 3; i++) {
      temp = '0' + temp;
    }
    longitudeString = temp;
    
    int m28 = (int)informationField[1] - 28;
    if (m28 >= 60) {
      m28 -= 60;
    }
    temp = String(m28);
    for(int i = temp.length(); i < 2; i++) {
      temp = '0' + temp;
    }
    longitudeString += temp + ".";
        
    int h28 = (int)informationField[2] - 28;
    temp = String(h28);
    for(int i = temp.length(); i < 2; i++) {
      temp = '0' + temp;
    }
    longitudeString += temp + westEast;
    return gpsDegreesToDecimalLongitude(longitudeString) ;
  }

  APRSPacket processReceivedPacket(String receivedPacket, int rssi, float snr, int freqError) {
    /*  Packet type:
        gps       = 0
        message   = 1
        status    = 2
        telemetry = 3
        mic-e     = 4
        object    = 5   */
    APRSPacket aprsPacket;
    aprsPacket.sender = receivedPacket.substring(0,receivedPacket.indexOf(">"));
    String temp0 = receivedPacket.substring(receivedPacket.indexOf(">")+1,receivedPacket.indexOf(":"));
    if (temp0.indexOf(",") > 2) {
      aprsPacket.tocall = temp0.substring(0,temp0.indexOf(","));
      aprsPacket.path = temp0.substring(temp0.indexOf(",")+1,temp0.indexOf(":"));
    } else {
      aprsPacket.tocall = temp0;
      aprsPacket.path = "";
    }
    if (receivedPacket.indexOf(":!") > 10 || receivedPacket.indexOf(":=") > 10 ) {
      aprsPacket.type = 0;
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
        aprsPacket.overlay = receivedPacket.substring(receivedPacket.indexOf(gpsChar)+2,receivedPacket.indexOf(gpsChar)+3);
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
        aprsPacket.overlay = receivedPacket.substring(receivedPacket.indexOf(gpsChar)+10,receivedPacket.indexOf(gpsChar)+11);
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
    } else if (receivedPacket.indexOf(":`") > 10 || receivedPacket.indexOf(":'") > 10) {
      aprsPacket.type = 4;
      if (receivedPacket.indexOf(":`") > 10) {
        aprsPacket.message = receivedPacket.substring(receivedPacket.indexOf(":`")+2);
      } else {
        aprsPacket.message = receivedPacket.substring(receivedPacket.indexOf(":'")+2);
      }
      aprsPacket.miceType = decodeMiceMsgType(aprsPacket.tocall);
      aprsPacket.symbol = decodeMiceSymbol(aprsPacket.message);
      aprsPacket.overlay = decodeMiceOverlay(aprsPacket.message);
      aprsPacket.latitude = decodeMiceLatitude(aprsPacket.tocall);
      aprsPacket.longitude = decodeMiceLongitude(aprsPacket.tocall, aprsPacket.message);
      aprsPacket.speed = decodeMiceSpeed(aprsPacket.message);
      aprsPacket.course = decodeMiceCourse(aprsPacket.message);
      aprsPacket.altitude = decodeMiceAltitude(aprsPacket.message);      
    } else if (receivedPacket.indexOf(":;") > 10) {
      aprsPacket.type = 5;
      aprsPacket.message = receivedPacket.substring(receivedPacket.indexOf(":;")+2);
    }
        
    if (aprsPacket.type!=1) {
      aprsPacket.addressee = "";
    }
    if (aprsPacket.type!=0 && aprsPacket.type!=4) {
      aprsPacket.symbol = "";
      aprsPacket.overlay = "";
      aprsPacket.latitude = 0;
      aprsPacket.longitude = 0;
      aprsPacket.course = 0;
      aprsPacket.speed = 0;
      aprsPacket.altitude = 0;
    }    
    if (aprsPacket.type!=4) {
      aprsPacket.miceType = "";
    }
    aprsPacket.rssi       = rssi;
    aprsPacket.snr        = snr;
    aprsPacket.freqError  = freqError;
    return aprsPacket;
  }

}