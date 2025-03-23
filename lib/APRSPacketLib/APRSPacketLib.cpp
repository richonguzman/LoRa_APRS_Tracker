/*_______________________________________________

          █████╗ ██████╗ ██████╗ ███████╗
         ██╔══██╗██╔══██╗██╔══██╗██╔════╝
         ███████║██████╔╝██████╔╝███████╗
         ██╔══██║██╔═══╝ ██╔══██╗╚════██║
         ██║  ██║██║     ██║  ██║███████║
         ╚═╝  ╚═╝╚═╝     ╚═╝  ╚═╝╚══════╝

██████╗  █████╗  ██████╗██╗  ██╗███████╗████████╗
██╔══██╗██╔══██╗██╔════╝██║ ██╔╝██╔════╝╚══██╔══╝
██████╔╝███████║██║     █████╔╝ █████╗     ██║
██╔═══╝ ██╔══██║██║     ██╔═██╗ ██╔══╝     ██║
██║     ██║  ██║╚██████╗██║  ██╗███████╗   ██║
╚═╝     ╚═╝  ╚═╝ ╚═════╝╚═╝  ╚═╝╚══════╝   ╚═╝

               ██╗     ██╗██████╗
               ██║     ██║██╔══██╗
               ██║     ██║██████╔╝
               ██║     ██║██╔══██╗
               ███████╗██║██████╔╝
               ╚══════╝╚═╝╚═════╝

             Ricardo Guzman - CA2RXU 
https://github.com/richonguzman/LoRa_APRS_Tracker
   (donations : http://paypal.me/richonguzman)
_______________________________________________*/


#include <APRSPacketLib.h>

namespace APRSPacketLib {

    String generateBasePacket(const String& callsign, const String& tocall, const String& path) {
        String packet = callsign;
        packet += ">";
        packet += tocall;
        if (path.indexOf("WIDE") == 0) {
            packet += ",";
            packet += path;
        }
        return packet;
    }

    String generateStatusPacket(const String& callsign, const String& tocall, const String& path, const String& status) {
        return generateBasePacket(callsign,tocall,path) + ":>"  + status;
    }

    String generateMessagePacket(const String& callsign, const String& tocall, const String& path, const String& addressee, const String& message) {
        String processedAddressee = addressee;
        for (int i = addressee.length(); i < 9; i++) {
            processedAddressee += ' ';
        }
        String processedMessage = message;
        processedMessage.trim();            
        return generateBasePacket(callsign,tocall,path) + "::" + processedAddressee + ":" + processedMessage;
    }

    String buildDigiPacket(const String& packet, const String& callsign, const String& path, const String& fullPath, bool thirdParty) {
        String packetToRepeat = packet.substring(0, packet.indexOf(",") + 1);
        String tempPath = fullPath;
        tempPath.replace(path, callsign + "*");
        packetToRepeat += tempPath;
        packetToRepeat += packet.substring(packet.indexOf(thirdParty ? ":}" : ":"));
        return packetToRepeat;
    }

    String generateDigipeatedPacket(const String& packet, const String &callsign, const String& path) {
        bool thirdParty = false;

        int firstColonIndex = packet.indexOf(":");
        if (firstColonIndex > 5 && firstColonIndex < (packet.length() - 1) && packet[firstColonIndex + 1] == '}') thirdParty = true;

        String temp = packet.substring(packet.indexOf(">") + 1, packet.indexOf(":"));
        if (thirdParty) {               // only header is used and temp is replaced
            const String& header = packet.substring(3, packet.indexOf(":}"));
            temp = header.substring(header.indexOf(">") + 1);
        }
        if (temp.indexOf(",") > 2) {    // checks for path
            const String& completePath = temp.substring(temp.indexOf(",") + 1); // after tocall
            return (completePath.indexOf(path) != -1) ? buildDigiPacket(packet.substring(3), callsign, path, completePath, thirdParty) : "X";
        }
        return "X";
    }

    char *ax25_base91enc(char *s, uint8_t n, uint32_t v) {
        for (s += n, *s = '\0'; n; n--) {
            *(--s) = v % 91 + 33;
            v /= 91;
        }
        return(s);
    }

    String encodeGPSIntoBase91(float latitude, float longitude, float course, float speed, const String& symbol, bool sendAltitude, int altitude, bool sendStandingUpdate, const String& packetType) {
        String encodedData;
        uint32_t aprs_lat, aprs_lon;
        aprs_lat = 900000000 - latitude * 10000000;
        aprs_lat = aprs_lat / 26 - aprs_lat / 2710 + aprs_lat / 15384615;
        aprs_lon = 900000000 + longitude * 10000000 / 2;
        aprs_lon = aprs_lon / 26 - aprs_lon / 2710 + aprs_lon / 15384615;

        String Ns, Ew, helper;
        if (latitude < 0) { Ns = "S"; } else { Ns = "N"; }
        if (latitude < 0) { latitude = -latitude; }

        if (longitude < 0) { Ew = "W"; } else { Ew = "E"; }
        if (longitude < 0) { longitude = -longitude; }

        char helper_base91[] = {"0000\0"};
        int i;
        ax25_base91enc(helper_base91, 4, aprs_lat);
        for (i = 0; i < 4; i++) {
            encodedData += helper_base91[i];
        }
        ax25_base91enc(helper_base91, 4, aprs_lon);
        for (i = 0; i < 4; i++) {
            encodedData += helper_base91[i];
        }
        if (packetType == "Wx") {
            encodedData += "_";
        } else {
            encodedData += symbol;
        }

        if (sendAltitude) {           // Send Altitude or... (APRS calculates Speed also)
            int Alt1, Alt2;
            if (altitude > 0) {
                double ALT = log(altitude)/log(1.002);
                Alt1 = int(ALT/91);
                Alt2 =(int)ALT%91;
            } else {
                Alt1 = 0;
                Alt2 = 0;
            }
            encodedData += char(Alt1 + 33);
            encodedData += char(Alt2 + 33);
            encodedData += char(0x30 + 33);
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

    String generateBase91GPSBeaconPacket(const String& callsign, const String& tocall, const String& path, const String& overlay, const String& gps) {
        return generateBasePacket(callsign,tocall,path) + ":=" + overlay + gps;
    }

    float decodeBase91EncodedLatitude(const String& encodedLatitude) {
        int Y1 = encodedLatitude[0] - 33;
        int Y2 = encodedLatitude[1] - 33;
        int Y3 = encodedLatitude[2] - 33;
        int Y4 = encodedLatitude[3] - 33;
        return (90.0 - (((Y1 * pow(91,3)) + (Y2 * pow(91,2)) + (Y3 * 91) + Y4) / 380926.0));
    }

    float decodeBase91EncodedLongitude(const String& encodedLongitude) {
        int X1 = encodedLongitude[0] - 33;
        int X2 = encodedLongitude[1] - 33;
        int X3 = encodedLongitude[2] - 33;
        int X4 = encodedLongitude[3] - 33;
        return (-180.0 + (((X1 * pow(91,3)) + (X2 * pow(91,2)) + (X3 * 91) + X4) / 190463.0));
    }

    int decodeBase91EncodedCourse(const String& course) {
        return (course.toInt() - 33) * 4;
    }

    int decodeBase91EncodedSpeed(const String& speed) {
        return (pow(1.08,(speed.toInt() - 33)) - 1) * 1.852;
    }

    int decodeBase91EncodedAltitude(const String& altitude) {
        char cLetter = altitude[0];
        char sLetter = altitude[1];
        int c = static_cast<int>(cLetter);
        int s = static_cast<int>(sLetter);
        return pow(1.002,((c - 33) * 91) + (s-33)) * 0.3048;
    }

    float decodeLatitude(const String& Latitude) {
        int latitudeDotIndex        = Latitude.indexOf(".");
        float convertedLatitude     = Latitude.substring(0,2).toFloat();
        convertedLatitude += Latitude.substring(2,4).toFloat() / 60;
        convertedLatitude += Latitude.substring(latitudeDotIndex + 1, latitudeDotIndex + 3).toFloat() / (60*100);
        if (Latitude.endsWith("S")) return - convertedLatitude;
        return convertedLatitude;
    }

    float decodeLongitude(const String& Longitude) {
        int longitudeDotIndex       = Longitude.indexOf(".");
        float convertedLongitude    = Longitude.substring(0,3).toFloat();
        convertedLongitude += Longitude.substring(3,5).toFloat() / 60;
        convertedLongitude += Longitude.substring(longitudeDotIndex + 1, longitudeDotIndex + 3).toFloat() / (60*100);
        if (Longitude.endsWith("W")) return -convertedLongitude;
        return convertedLongitude;
    }

    int decodeCourse(const String& course) {
        if (course == "..." || course == "000") return 0;
        return course.toInt();
    }

    int decodeSpeed(const String& speed) {
        return speed.toInt() * 1.852;
    }

    int decodeAltitude(const String& altitude) {
        return altitude.toInt() * 0.3048;
    }


    //  Mic-E

    String decodeMiceMsgType(const String& tocall) {
        char miceType[4];
        for (int i = 0; i < 3; i++) {
            miceType[i] = (tocall[i] > '9') ? '1' : '0';
        }
        miceType[3] = '\0';
        return String(miceType);
    }

    int decodeMiceSpeed(char char3, char char4) {
        int temp = int(char3);
        if (temp > 107) {
            temp -= 80;
        }
        int SP28 = (temp - 28) * 10;
        int DC28 = (int(char4) - 28) / 10;
        return (SP28 + DC28) * 1.852;
    }

    int decodeMiceCourse(char char4, char char5) {
        int DC28 = (int(char4) - 28)/10;
        int temp = (int(char4) - 28) - (DC28 * 10);
        int SE28 = int(char5) - 28;
        int course = ((temp - 4) * 100) + SE28;
        return course;
    }

    int decodeMiceAltitude(const String& informationField) {
        int altitude    = 0;
        String temp     = "";
        int rightCurlyBraceIndex = informationField.indexOf("}");
        if (informationField.indexOf("`") == 8 && rightCurlyBraceIndex == 12) {
            temp = informationField.substring(9, 12);
        } else if (rightCurlyBraceIndex == 11) {
            temp = informationField.substring(8, 11);
        }
        if (temp.length() != 0) {
            int a = int(temp[0]) - 33;
            int b = int(temp[1]) - 33;
            int c = int(temp[2]) - 33;
            altitude = ((a * pow(91,2)) + (b * 91) + c) - 10000;
        }
        return altitude;
    }

    float gpsDegreesToDecimalLatitude(const String& degreesLatitude) {
        int degrees             = degreesLatitude.substring(0,2).toInt();
        int minute              = degreesLatitude.substring(2,4).toInt();
        int minuteHundredths    = degreesLatitude.substring(5,7).toInt();
        float decimalLatitude   = degrees + (minute/60.0) + (minuteHundredths/6000.0);
        return (degreesLatitude[7] == 'N') ? decimalLatitude : -decimalLatitude;
    }

    float gpsDegreesToDecimalLongitude(const String& degreesLongitude) {
        int degrees             = degreesLongitude.substring(0,3).toInt();
        int minute              = degreesLongitude.substring(3,5).toInt();
        int minuteHundredths    = degreesLongitude.substring(6,8).toInt();
        float decimalLongitude  = degrees + (minute/60.0) + (minuteHundredths/6000.0);
        return (degreesLongitude[8] == 'W') ? -decimalLongitude : decimalLongitude;
    }

    float decodeMiceLatitude(const String& destinationField) {
        String gpsLat;
        String northSouth = "S";
        for (int i = 0; i < 6; i++) {
            char currentChar = destinationField[i];
            if (currentChar > '9') {
                gpsLat += char(int(currentChar) - 32);
            } else {
                gpsLat += char(int(currentChar));
            }
            if (i == 3) {
                gpsLat += ".";
                if (currentChar > '9') northSouth = "N";        // ???? para todos?
            }   
        }
        gpsLat += northSouth;
        return gpsDegreesToDecimalLatitude(gpsLat);
    }

    float decodeMiceLongitude(const String& destinationField, const String& informationField) {
        bool offset = false;
        String westEast = "E";
        if (destinationField[4] > '9') offset = true;
        if (destinationField[5] > '9') westEast = "W";

        String temp;
        int d28 = (int)informationField[0] - 28;
        if (offset) d28 += 100;
        temp = String(d28);
        for (int i = temp.length(); i < 3; i++) {
            temp = '0' + temp;
        }
        String longitudeString = temp;
        
        int m28 = (int)informationField[1] - 28;
        if (m28 >= 60) m28 -= 60;
        temp = String(m28);
        for (int i = temp.length(); i < 2; i++) {
            temp = '0' + temp;
        }
        longitudeString += temp;
        longitudeString += ".";
            
        int h28 = (int)informationField[2] - 28;
        temp = String(h28);
        for (int i = temp.length(); i < 2; i++) {
            temp = '0' + temp;
        }
        longitudeString += temp;
        longitudeString += westEast;
        return gpsDegreesToDecimalLongitude(longitudeString) ;
    }

    void encodeMiceAltitude(uint8_t *buf, uint32_t alt_m) {
        if (alt_m > 40000) alt_m = 0;
        uint32_t altoff = alt_m + 10000;
        buf[0] = (altoff/8281) + 33;
        altoff = altoff%8281;
        buf[1] = (altoff/91) + 33;	
        buf[2] = (altoff%91) + 33;
        buf[3] = '}';
    }

    void encodeMiceCourseSpeed(uint8_t *buf, uint32_t speed_kt, uint32_t course_deg) {
        uint32_t DC28, SE28; //three bytes are output

        uint32_t SP28 = 107;
        uint32_t ten = speed_kt / 10;
        if (ten <= 19) {
            SP28 = ten + 108;
        } else if (ten <= 79) {
            SP28 = ten + 28;
        }
        buf[0] = SP28;

        if (course_deg == 0) {
            course_deg = 360;
        } else if (course_deg >= 360) {
            course_deg = 0;
        }
        uint32_t course_hun = course_deg/100;
        DC28    = (speed_kt-ten * 10) * 10 + course_hun + 32;
        buf[1]  = DC28;				

        SE28    = (course_deg - course_hun * 100) + 28;
        buf[2]  = SE28;
    }

    void encodeMiceLongitude(uint8_t *buf, gpsLongitudeStruct *lon) { 
        uint32_t deg = lon->degrees;
        uint32_t d28 = 28 + (deg - 100);    // degrees
        if (deg <= 9) {
            d28 = 118 + deg;
        } else if (deg <= 99) {
            d28 = 28 + deg;
        } else if (deg <= 109) {
            d28 = 8 + deg;
        }
        buf[0] = d28;
        
        uint32_t min = lon->minutes;
        uint32_t m28 = 28 + min;            // minutes
        if (min <= 9) m28 = 88 + min;
        buf[1] = m28;
        
        uint32_t h28 = 28 + lon->minuteHundredths;
        buf[2] = h28;
    }

    void encodeMiceDestinationField(const String& msgType, uint8_t *buf, const gpsLatitudeStruct *lat, const gpsLongitudeStruct *lon) {
        uint32_t temp;
        temp = lat->degrees / 10;             // degrees
        buf[0] = (temp + 0x30);
        if (msgType[0] == '1') buf[0] = buf[0] + 0x20;
        buf[1] = (lat->degrees - temp * 10 + 0x30);
        if (msgType[1] == '1') buf[1] = buf[1] + 0x20;

        temp = lat->minutes/10;             // minutes
        buf[2] = (temp + 0x30);
        if (msgType[2] == '1') buf[2] = buf[2] + 0x20;
        buf[3] = (lat->minutes - temp * 10 + 0x30) + (lat->north ? 0x20 : 0);               // North validation

        temp   = lat->minuteHundredths/10;  // minute hundredths
        buf[4] = (temp + 0x30) + ((lon->degrees >= 100 || lon->degrees <= 9) ? 0x20 : 0);   // Longitude Offset
        buf[5] = (lat->minuteHundredths - temp * 10 + 0x30) + (!lon->east ? 0x20 : 0);      // West validation
    }

    String doubleToString(double n, int ndec) {
        String r = "";
        if (n > -1 && n < 0) r = "-";
        int v = n;
        r += v;
        r += '.';
        for (int i = 0; i < ndec; i++) {
            n -= v;
            n = 10 * abs(n);
            v = n;
            r += v;
        }
        return r;
    }

    String gpsDecimalToDegreesLatitude(double lat) {
        String degrees = doubleToString(lat, 6);
        String latitude, convDeg3;
        float convDeg, convDeg2;
        String north_south = "N";
        if (abs(degrees.toFloat()) < 10) latitude += "0";
        if (degrees.indexOf("-") == 0) {
            north_south = "S";
            latitude += degrees.substring(1, degrees.indexOf("."));
        } else {
            latitude += degrees.substring(0, degrees.indexOf("."));
        }
        convDeg  = abs(degrees.toFloat()) - abs(int(degrees.toFloat()));
        convDeg2 = (convDeg * 60)/100;
        convDeg3 = String(convDeg2,6);

        int dotIndex = convDeg3.indexOf(".");
        latitude += convDeg3.substring(dotIndex + 1, dotIndex + 3);
        latitude += ".";
        latitude += convDeg3.substring(dotIndex + 3, dotIndex + 5);
        latitude += north_south;
        return latitude;
    }

    String gpsDecimalToDegreesLongitude(double lon) {
        String degrees = doubleToString(lon,6);
        String longitude, convDeg3;
        float convDeg, convDeg2;
        String east_west = "E";
        if (abs(degrees.toFloat()) < 100) longitude += "0";
        if (abs(degrees.toFloat()) < 10)  longitude += "0";
        if (degrees.indexOf("-") == 0) {
            east_west = "W";
            longitude += degrees.substring(1, degrees.indexOf("."));
        } else {
            longitude += degrees.substring(0, degrees.indexOf("."));
        }
        convDeg  = abs(degrees.toFloat()) - abs(int(degrees.toFloat()));
        convDeg2 = (convDeg * 60)/100;
        convDeg3 = String(convDeg2,6);

        int dotIndex = convDeg3.indexOf(".");
        longitude += convDeg3.substring(dotIndex + 1, dotIndex + 3);
        longitude += ".";
        longitude += convDeg3.substring(dotIndex + 3, dotIndex + 5);
        longitude += east_west;
        return longitude;
    }

    gpsLatitudeStruct gpsDecimalToDegreesMiceLatitude(float latitude) {
        gpsLatitudeStruct miceLatitudeStruct;
        String lat = gpsDecimalToDegreesLatitude(latitude);
        char latitudeArray[10];
        strncpy(latitudeArray, lat.c_str(), 8);
        miceLatitudeStruct.degrees          = 10 * (latitudeArray[0] - '0') + latitudeArray[1] - '0';
        miceLatitudeStruct.minutes          = 10 * (latitudeArray[2] - '0') + latitudeArray[3] - '0';
        miceLatitudeStruct.minuteHundredths = 10 * (latitudeArray[5] - '0') + latitudeArray[6] - '0';
        miceLatitudeStruct.north = 0;
        if (latitudeArray[7] == 'N') miceLatitudeStruct.north = 1;
        return miceLatitudeStruct;
    }

    gpsLongitudeStruct gpsDecimalToDegreesMiceLongitude(float longitude) {
        gpsLongitudeStruct miceLongitudeStruct;
        String lng = gpsDecimalToDegreesLongitude(longitude);
        char longitudeArray[10];
        strncpy(longitudeArray,lng.c_str(), 9);
        miceLongitudeStruct.degrees             = 100 * (longitudeArray[0] - '0') + 10 * (longitudeArray[1] - '0') + longitudeArray[2] - '0';
        miceLongitudeStruct.minutes             = 10  * (longitudeArray[3] - '0') + longitudeArray[4] - '0';
        miceLongitudeStruct.minuteHundredths    = 10  * (longitudeArray[6] - '0') + longitudeArray[7] - '0';
        miceLongitudeStruct.east = 0;
        if (longitudeArray[8] == 'E') miceLongitudeStruct.east = 1;
        return miceLongitudeStruct;
    }

    String generateMiceGPSBeaconPacket(const String& miceMsgType, const String& callsign, const String& symbol, const String& overlay, const String& path, float latitude, float longitude, float course, float speed, int altitude) {
        gpsLatitudeStruct latitudeStruct    = gpsDecimalToDegreesMiceLatitude(latitude);
        gpsLongitudeStruct longitudeStruct  = gpsDecimalToDegreesMiceLongitude(longitude);

        uint8_t miceDestinationArray[7];
        encodeMiceDestinationField(miceMsgType, &miceDestinationArray[0], &latitudeStruct, &longitudeStruct);
        miceDestinationArray[6] = 0x00;     // por repetidor?
        String miceDestination = (char*)miceDestinationArray;

        uint8_t miceInfoFieldArray[14];
        miceInfoFieldArray[0] = 0x60;   //  0x60 for ` and 0x27 for '
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

        String miceAPRSPacket = callsign;
        miceAPRSPacket += ">";
        miceAPRSPacket += miceDestination;
        if (path != "") {
            miceAPRSPacket += ",";
            miceAPRSPacket += path;
        }
        miceAPRSPacket += ":";
        miceAPRSPacket += miceInformationField;
        return miceAPRSPacket;
    }

    /********** MAIN PROCESS**********/

    APRSPacket processReceivedPacket(const String& receivedPacket, int rssi, float snr, int freqError) {
        /*  Packet type:
            gps       = 0
            message   = 1
            status    = 2
            telemetry = 3
            mic-e     = 4
            object    = 5   */
        APRSPacket aprsPacket;

        aprsPacket.header = "";
        String temp0 = receivedPacket;
        int firstColonIndex = receivedPacket.indexOf(":");
        if (firstColonIndex > 0 && firstColonIndex < receivedPacket.length() && receivedPacket[firstColonIndex + 1] == '}') {   // 3rd Party
            aprsPacket.header = receivedPacket.substring(receivedPacket.indexOf(":}"));
            temp0 = receivedPacket.substring(receivedPacket.indexOf(":}") + 2);
        }

        aprsPacket.sender   = temp0.substring(0, temp0.indexOf(">"));
        
        String temp1 = temp0.substring(temp0.indexOf(">") + 1, temp0.indexOf(":"));
        aprsPacket.tocall   = temp1;
        aprsPacket.path     = "";
        if (temp1.indexOf(",") > 2) {
            aprsPacket.tocall   = temp1.substring(0, temp1.indexOf(","));
            aprsPacket.path     = temp1.substring(temp1.indexOf(",") + 1, temp1.indexOf(":"));
        }

        if (temp0.indexOf(":=") > 10 || temp0.indexOf(":!") > 10 || temp0.indexOf(":@") > 10 ) {
            aprsPacket.type = 0;
            String gpsChars = ":=";
            int gpsCharsOffset = 2;
            if (temp0.indexOf(":!") > 10) gpsChars = ":!";
            if (temp0.indexOf(":@") > 10) {
                gpsChars = ":@";
                gpsCharsOffset = 9;
            }
            int gpsCharsIndex       = temp0.indexOf(gpsChars);
            int payloadOffset       = gpsCharsIndex + gpsCharsOffset;
            aprsPacket.payload      = temp0.substring(payloadOffset);
            int encodedBytePosition = payloadOffset + 12;
            char currentChar        = temp0[encodedBytePosition];
            if (currentChar == 'G' || currentChar == 'Q' || currentChar == '[' || currentChar == 'H' || currentChar == 'X' || currentChar == 'T') {   //  Base91 Encoding
                aprsPacket.latitude     = decodeBase91EncodedLatitude(temp0.substring(payloadOffset + 1, payloadOffset + 5));
                aprsPacket.longitude    = decodeBase91EncodedLongitude(temp0.substring(payloadOffset + 5, payloadOffset + 9));
                aprsPacket.symbol       = temp0.substring(payloadOffset + 9, payloadOffset + 10);
                aprsPacket.overlay      = temp0.substring(payloadOffset, payloadOffset + 1);

                if ((currentChar == 'T' || currentChar == 'Q') && temp0.substring(payloadOffset + 10, payloadOffset + 11) == " ") {
                    aprsPacket.course   = 0;
                    aprsPacket.speed    = 0;
                    aprsPacket.altitude = 0;
                } else {
                    if (currentChar == 'Q') { // altitude csT
                        aprsPacket.altitude = decodeBase91EncodedAltitude(temp0.substring(payloadOffset + 10, payloadOffset + 12));
                        aprsPacket.course   = 0;
                        aprsPacket.speed    = 0;
                    } else { // normal csT ('G' or '[')
                        aprsPacket.course   = decodeBase91EncodedCourse(temp0.substring(payloadOffset + 10, payloadOffset + 11));
                        aprsPacket.speed    = decodeBase91EncodedSpeed(temp0.substring(payloadOffset + 11, payloadOffset + 12));
                        aprsPacket.altitude = 0;
                    }
                }
            } else {    //  Degrees and Decimal Minutes
                aprsPacket.latitude     = decodeLatitude(temp0.substring(payloadOffset, payloadOffset + 8));
                aprsPacket.longitude    = decodeLongitude(temp0.substring(payloadOffset + 9, payloadOffset + 18));
                aprsPacket.symbol       = temp0.substring(payloadOffset + 18, payloadOffset+ 19);
                aprsPacket.overlay      = temp0.substring(payloadOffset + 8, payloadOffset + 9);
                if (temp0.substring(payloadOffset + 22, payloadOffset + 23) == "/") {
                    aprsPacket.course   = decodeCourse(temp0.substring(payloadOffset + 19, payloadOffset + 22));
                    aprsPacket.speed    = decodeSpeed(temp0.substring(payloadOffset + 23, payloadOffset + 26));
                } else {
                    aprsPacket.course   = 0;
                    aprsPacket.speed    = 0;
                }
                int altitudeIndex = temp0.indexOf("/A=");
                if (altitudeIndex > 0 && (altitudeIndex + 9 <= temp0.length())) {
                    aprsPacket.altitude = decodeAltitude(temp0.substring(altitudeIndex + 3, altitudeIndex + 9));
                } else {
                    aprsPacket.altitude = 0;
                }
            }
        } else if (temp0.indexOf("::") > 10) {
            aprsPacket.type = 1;
            int doubleColonIndex = temp0.indexOf("::");
            String temp1 = temp0.substring(doubleColonIndex + 2, doubleColonIndex + 11);
            temp1.trim();
            aprsPacket.addressee    = temp1;
            aprsPacket.payload      = temp0.substring(doubleColonIndex + 12);
            aprsPacket.latitude     = 0;
            aprsPacket.longitude    = 0;
        } else if (temp0.indexOf(":>") > 10) {
            aprsPacket.type = 2;
            aprsPacket.payload = temp0.substring(temp0.indexOf(":>") + 2);
        } else if (temp0.indexOf(":T#") >= 10 && temp0.indexOf(":=/") == -1) {
            aprsPacket.type = 3;
            aprsPacket.payload = temp0.substring(temp0.indexOf(":T#") + 3);
        } else if (temp0.indexOf(":`") > 10 || temp0.indexOf(":'") > 10) {
            aprsPacket.type = 4;
            if (temp0.indexOf(":`") > 10) {
                aprsPacket.payload  = temp0.substring(temp0.indexOf(":`") + 2);
            } else {
                aprsPacket.payload  = temp0.substring(temp0.indexOf(":'") + 2);
            }
            aprsPacket.miceType     = decodeMiceMsgType(aprsPacket.tocall.substring(0,3));
            aprsPacket.symbol       = aprsPacket.payload.substring(6,7);
            aprsPacket.overlay      = aprsPacket.payload.substring(7,8);
            aprsPacket.latitude     = decodeMiceLatitude(aprsPacket.tocall);
            aprsPacket.longitude    = decodeMiceLongitude(aprsPacket.tocall, aprsPacket.payload);
            aprsPacket.speed        = decodeMiceSpeed(aprsPacket.payload[3], aprsPacket.payload[4]);
            aprsPacket.course       = decodeMiceCourse(aprsPacket.payload[4], aprsPacket.payload[5]);
            aprsPacket.altitude     = decodeMiceAltitude(aprsPacket.payload);      
        } else if (temp0.indexOf(":;") > 10) {
            aprsPacket.type = 5;
            aprsPacket.payload = temp0.substring(temp0.indexOf(":;") + 2);
        }
            
        if (aprsPacket.type != 1) aprsPacket.addressee = "";

        if (aprsPacket.type != 0 && aprsPacket.type != 4) {
            aprsPacket.symbol       = "";
            aprsPacket.overlay      = "";
            aprsPacket.latitude     = 0;
            aprsPacket.longitude    = 0;
            aprsPacket.course       = 0;
            aprsPacket.speed        = 0;
            aprsPacket.altitude     = 0;
        }

        if (aprsPacket.type != 4) aprsPacket.miceType = "";

        aprsPacket.rssi             = rssi;
        aprsPacket.snr              = snr;
        aprsPacket.freqError        = freqError;

        return aprsPacket;
    }

}
