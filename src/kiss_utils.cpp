#include "kiss_utils.h"


namespace KISS_Utils {

    bool validateTNC2Frame(const String& tnc2FormattedFrame) {
        int colonPos        = tnc2FormattedFrame.indexOf(':');
        int greaterThanPos  = tnc2FormattedFrame.indexOf('>');
        return (colonPos != -1) && (greaterThanPos != -1) && (colonPos > greaterThanPos);
    }

    bool validateKISSFrame(const String& kissFormattedFrame) {
        return kissFormattedFrame.charAt(0) == (char)KissChar::FEND && kissFormattedFrame.charAt(kissFormattedFrame.length() - 1) == (char)KissChar::FEND;
    }

    String decodeAddressAX25(const String& ax25Address, bool& isLastAddress, bool isRelay) {
        String address = "";
        for (int i = 0; i < 6; ++i) {
            uint8_t currentCharacter = ax25Address.charAt(i);
            currentCharacter >>= 1;
            if (currentCharacter != ' ') address += (char)currentCharacter;
        }
        auto ssidChar           = (uint8_t)ax25Address.charAt(6);
        bool hasBeenDigipited   = ssidChar & HAS_BEEN_DIGIPITED_MASK;
        isLastAddress           = ssidChar & IS_LAST_ADDRESS_POSITION_MASK;
        ssidChar >>= 1;

        int ssid = 0b1111 & ssidChar;
        if (ssid) {
            address += '-';
            address += ssid;
        }
        if (isRelay && hasBeenDigipited) address += '*';
        return address;
    }

    String decapsulateKISS(const String& frame) {
        String ax25Frame = "";
        for (int i = 2; i < frame.length() - 1; ++i) {
            char currentChar = frame.charAt(i);
            if (currentChar == (char)KissChar::FESC) {
                char nextChar = frame.charAt(i + 1);
                if (nextChar == (char)KissChar::TFEND) {
                    ax25Frame += (char)KissChar::FEND;
                } else if (nextChar == (char)KissChar::TFESC) {
                    ax25Frame += (char)KissChar::FESC;
                }
                i++;
            } else {
                ax25Frame += currentChar;
            }
        }
        return ax25Frame;
    }

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
            if (hasBeenDigipited) address = address.substring(0, address.length() - 1);
            address += "-0";
        }

        int separatorIndex  = address.indexOf('-');
        int ssid            = address.substring(separatorIndex + 1).toInt();
        String kissAddress  = "";
        for (int i = 0; i < 6; ++i) {
            char addressChar = ' ';
            if (address.length() > i && i < separatorIndex) addressChar = address.charAt(i);
            kissAddress += (char)(addressChar << 1);
        }
        kissAddress += (char)((ssid << 1) | 0b01100000 | (hasBeenDigipited ? HAS_BEEN_DIGIPITED_MASK : 0));
        return kissAddress;
    }

    String decodeKISS(const String& inputFrame, bool& dataFrame) {
        String frame = "";
        if (KISS_Utils::validateKISSFrame(inputFrame)) {
            dataFrame = inputFrame.charAt(1) == KissCmd::Data;
            if (dataFrame) {
                String ax25Frame    = decapsulateKISS(inputFrame);
                bool isLastAddress         = false;
                String dstAddr      = decodeAddressAX25(ax25Frame.substring(0, 7), isLastAddress, false);
                String srcAddr      = decodeAddressAX25(ax25Frame.substring(7, 14), isLastAddress, false);

                frame = srcAddr + ">" + dstAddr;

                int digiInfoIndex = 14;
                while (!isLastAddress && digiInfoIndex + 7 < ax25Frame.length()) {
                    String digiAddr = decodeAddressAX25(ax25Frame.substring(digiInfoIndex, digiInfoIndex + 7), isLastAddress, true);
                    frame += ',' + digiAddr;
                    digiInfoIndex += 7;
                }
                frame += ':';
                frame += ax25Frame.substring(digiInfoIndex + 2);
            } else {
                frame += inputFrame;
            }
        }
        return frame;
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