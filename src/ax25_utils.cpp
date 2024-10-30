#include "ax25_utils.h"
#include "kiss_utils.h"


namespace AX25_Utils {

    AX25Frame decodedFrame;

    String decodeFrame(const String& frame) {
        String packet = "";
        for (int a = 0; a < 6; a ++) {
            uint16_t shiftedValue = frame[a] >> 1;
            if (shiftedValue == 32) {
                a = 10;
            } else {
                //Serial.print(char(shiftedValue));
                packet += char(shiftedValue);
            }
        }
        byte ssid = (frame[6]>>1) & 0x0f;
        if (String(ssid) != "0") {
            packet += "-";
            packet += String(ssid);
        }
        return packet;
    }

    bool decodeAX25(const String& frame, int frameSize, AX25Frame* decodedFrame) {
        if ((frameSize < 14) || (frame[0] != KissChar::FEND && frame[1] != KissCmd::Data && frame[frameSize - 1] != KissChar::FEND)) {
            return false;
        }
        int payloadFrameStart = 0;
        for (int i = 0; i < frameSize; i++) {                   // where is CONTROL y PID ?
            if (frame[i] == 0x03 && frame[i+1] == 0xf0) {
                payloadFrameStart = i+1;
            }
        }
        decodedFrame->tocall    = frame.substring(2, 9);      // Extract destination address
        decodedFrame->sender    = frame.substring(9, 16);     // Extract source address
        if (payloadFrameStart >= 21) {                    // is there path1?
            decodedFrame->path1 = frame.substring(16, 23);
        }
        if (payloadFrameStart >= 28) {                    // is there path2?
            decodedFrame->path2 = frame.substring(23, 30);
        }
        decodedFrame->control   = frame.substring(payloadFrameStart-1, payloadFrameStart);   // Extract control information  // 0x03
        decodedFrame->pid       = frame.substring(payloadFrameStart, payloadFrameStart + 1);       // Extract pid information      // 0xF0
        decodedFrame->payload   = frame.substring(payloadFrameStart + 1, frameSize - 1);         // Extract payload
        return true;
    }

    String AX25FrameToLoRaPacket(const String& frame) {
        //Serial.println(frame);
        if (decodeAX25(frame, frame.length(), &decodedFrame)) {
            //String packetToLoRa = "";
            String packetToLoRa = decodeFrame(decodedFrame.sender) + ">" + decodeFrame(decodedFrame.tocall);

            if (decodedFrame.path1[0] != 0) {
                packetToLoRa += ",";
                packetToLoRa += decodeFrame(decodedFrame.path1);
            }
            if (decodedFrame.path2[0] != 0) {
                packetToLoRa += ",";
                packetToLoRa += decodeFrame(decodedFrame.path2);
            }
            packetToLoRa += ":";
            packetToLoRa += decodedFrame.payload;
            return packetToLoRa;
        } else {
            return "";
        }
    }

    //**************************************

    String encapsulateKISS(const String& ax25Frame, uint8_t command) {
        String kissFrame = "";
        kissFrame += (char)KissChar::FEND;
        kissFrame += (char)(0x0f & command);

        for (int i = 0; i < ax25Frame.length(); ++i) {
            char currentChar = ax25Frame.charAt(i);
            if (currentChar == (char)KissChar::FEND) {
                kissFrame += (char)KissChar::FESC;
                kissFrame += (char)KissChar::TFEND;
            } else if (currentChar == (char)KissChar::FESC) {
                kissFrame += (char)KissChar::FESC;
                kissFrame += (char)KissChar::TFESC;
            } else {
                kissFrame += currentChar;
            }
        }
        kissFrame += (char)KissChar::FEND; // end of frame
        return kissFrame;
    }

    String encodeAddressAX25(String address) {
        bool hasBeenDigipited = address.indexOf('*') != -1;
        if (address.indexOf('-') == -1) {
            if (hasBeenDigipited) {
                address = address.substring(0, address.length() - 1);
            }
            address += "-0";
        }

        int separatorIndex  = address.indexOf('-');
        int ssid            = address.substring(separatorIndex + 1).toInt();
        String kissAddress  = "";
        for (int i = 0; i < 6; ++i) {
            char addressChar;
            if (address.length() > i && i < separatorIndex) {
                addressChar = address.charAt(i);
            } else {
                addressChar = ' ';
            }
            kissAddress += (char)(addressChar << 1);
        }
        kissAddress += (char)((ssid << 1) | 0b01100000 | (hasBeenDigipited ? HAS_BEEN_DIGIPITED_MASK : 0));
        return kissAddress;
    }

    String encodeKISS(const String& frame) {
        String ax25Frame = "";

        if (KISS_Utils::validateTNC2Frame(frame)) {
            int colonIndex = frame.indexOf(':');

            String address = "";
            bool destinationAddressWritten = false;
            for (int i = 0; i <= colonIndex; i++) {
                char currentChar = frame.charAt(i);
                if (currentChar == ':' || currentChar == '>' || currentChar == ',') {
                    if (!destinationAddressWritten && (currentChar == ',' || currentChar == ':')) {
                        ax25Frame = encodeAddressAX25(address) + ax25Frame;
                        destinationAddressWritten = true;
                    } else {
                        ax25Frame += encodeAddressAX25(address);
                    }
                    address = "";
                } else {
                    address += currentChar;
                }
            }
            auto lastAddressChar = (uint8_t)ax25Frame.charAt(ax25Frame.length() - 1);
            ax25Frame.setCharAt(ax25Frame.length() - 1, (char)(lastAddressChar | IS_LAST_ADDRESS_POSITION_MASK));
            ax25Frame += (char)AX25Char::ControlField;
            ax25Frame += (char)AX25Char::InformationField;
            ax25Frame += frame.substring(colonIndex + 1);
        }
        String kissFrame = encapsulateKISS(ax25Frame, KissCmd::Data);
        return kissFrame;
    }

}