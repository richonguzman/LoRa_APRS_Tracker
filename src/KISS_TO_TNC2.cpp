#include "KISS_TO_TNC2.h"
#include "kiss_utils.h"

String encode_address_ax25(String tnc2Address);
String decode_address_ax25(const String &ax25Address, bool &isLast, bool isRelay);

String decapsulateKISS(const String &frame);


String encode_kiss(const String &tnc2FormattedFrame) {
    String ax25Frame = "";

    if (KISS_Utils::validateTNC2Frame(tnc2FormattedFrame)) {
        String address = "";
        bool dst_addres_written = false;
        for (int p = 0; p <= tnc2FormattedFrame.indexOf(':'); p++) {
            char currentChar = tnc2FormattedFrame.charAt(p);
            if (currentChar == ':' || currentChar == '>' || currentChar == ',') {
                if (!dst_addres_written && (currentChar == ',' || currentChar == ':')) {
                    // ax25 frame DST SRC
                    // tnc2 frame SRC DST
                    ax25Frame = encode_address_ax25(address) + ax25Frame;
                    dst_addres_written = true;
                } else {
                    ax25Frame += encode_address_ax25(address);
                }
                address = "";
            } else {
                address += currentChar;
            }
        }
        auto lastAddressChar = (uint8_t) ax25Frame.charAt(ax25Frame.length() - 1);
        ax25Frame.setCharAt(ax25Frame.length() - 1, (char) (lastAddressChar | IS_LAST_ADDRESS_POSITION_MASK));
        ax25Frame += (char) AX25Char::ControlField;
        ax25Frame += (char) AX25Char::InformationField;
        ax25Frame += tnc2FormattedFrame.substring(tnc2FormattedFrame.indexOf(':') + 1);
    }

    String kissFrame = encapsulateKISS(ax25Frame, KissCmd::Data);
    return kissFrame;
}

String encapsulateKISS(const String &ax25Frame, uint8_t TNCCmd) {
    String kissFrame = "";
    kissFrame += (char) KissChar::FEND; // start of frame
    kissFrame += (char) (0x0f & TNCCmd); // TNC0, cmd
    for (int i = 0; i < ax25Frame.length(); ++i) {
        char currentChar = ax25Frame.charAt(i);
        if (currentChar == (char) KissChar::FEND) {
            kissFrame += (char) KissChar::FESC;
            kissFrame += (char) KissChar::TFEND;
        } else if (currentChar == (char) KissChar::FESC) {
            kissFrame += (char) KissChar::FESC;
            kissFrame += (char) KissChar::TFESC;
        } else {
            kissFrame += currentChar;
        }
    }
    kissFrame += (char) KissChar::FEND; // end of frame
    return kissFrame;
}


String decapsulateKISS(const String &frame) {
    String ax25Frame = "";
    for (int i = 2; i < frame.length() - 1; ++i) {
        char currentChar = frame.charAt(i);
        if (currentChar == (char) KissChar::FESC) {
            char nextChar = frame.charAt(i + 1);
            if (nextChar == (char) KissChar::TFEND) {
                ax25Frame += (char) KissChar::FEND;
            } else if (nextChar == (char) KissChar::TFESC) {
                ax25Frame += (char) KissChar::FESC;
            }
            i++;
        } else {
            ax25Frame += currentChar;
        }
    }
    return ax25Frame;
}

/**
 *
 * @param inputKISSTNCFrame
 * @param dataFrame
 * @return Decapsulated TNC2KISS APRS data frame, or raw command data frame
 */
String decode_kiss(const String &inputKISSTNCFrame, bool &dataFrame) {
    String TNC2Frame = "";

    if (KISS_Utils::validateKISSFrame(inputKISSTNCFrame)) {
        dataFrame = inputKISSTNCFrame.charAt(1) == KissCmd::Data;
        if (dataFrame){
            String ax25Frame = decapsulateKISS(inputKISSTNCFrame);
            bool isLast = false;
            String dst_addr = decode_address_ax25(ax25Frame.substring(0, 7), isLast, false);
            String src_addr = decode_address_ax25(ax25Frame.substring(7, 14), isLast, false);
            TNC2Frame = src_addr + ">" + dst_addr;
            int digi_info_index = 14;
            while (!isLast && digi_info_index + 7 < ax25Frame.length()) {
                String digi_addr = decode_address_ax25(ax25Frame.substring(digi_info_index, digi_info_index + 7), isLast, true);
                TNC2Frame += ',' + digi_addr;
                digi_info_index += 7;
            }
            TNC2Frame += ':';
            TNC2Frame += ax25Frame.substring(digi_info_index + 2);
        } else {
            // command frame, currently ignored
            TNC2Frame += inputKISSTNCFrame;
        }
    }

    return TNC2Frame;
}

/**
 * Encode adress in TNC2 monitor format to ax.25 format
 * @param tnc2Address
 * @return
 */
String encode_address_ax25(String tnc2Address) {
    bool hasBeenDigipited = tnc2Address.indexOf('*') != -1;

    if (tnc2Address.indexOf('-') == -1) {
        if (hasBeenDigipited) {
            // ex. TCPIP* in tnc2Address
            // so we skip last char
            tnc2Address = tnc2Address.substring(0, tnc2Address.length() - 1);
        }
        tnc2Address += "-0";
    }

    int separatorIndex = tnc2Address.indexOf('-');
    int ssid = tnc2Address.substring(separatorIndex + 1).toInt();
    // TODO: SSID should not be > 16
    String kissAddress = "";
    for (int i = 0; i < 6; ++i) {
        char addressChar;
        if (tnc2Address.length() > i && i < separatorIndex) {
            addressChar = tnc2Address.charAt(i);
        } else {
            addressChar = ' ';
        }
        kissAddress += (char) (addressChar << 1);
    }
    kissAddress += (char) ((ssid << 1) | 0b01100000 | (hasBeenDigipited ? HAS_BEEN_DIGIPITED_MASK : 0));
    return kissAddress;
}

/**
 * Decode address from ax.25 format to TNC2 monitor format
 * @param ax25Address
 * @return
 */
String decode_address_ax25(const String &ax25Address, bool &isLast, bool isRelay) {
    String TNCAddress = "";
    for (int i = 0; i < 6; ++i) {
        uint8_t currentCharacter = ax25Address.charAt(i);
        currentCharacter >>= 1;
        if (currentCharacter != ' ') {
            TNCAddress += (char) currentCharacter;
        }
    }
    auto ssid_char = (uint8_t) ax25Address.charAt(6);
    bool hasBeenDigipited = ssid_char & HAS_BEEN_DIGIPITED_MASK;
    isLast = ssid_char & IS_LAST_ADDRESS_POSITION_MASK;
    ssid_char >>= 1;

    int ssid = 0b1111 & ssid_char;
    if (ssid) {
        TNCAddress += '-';
        TNCAddress += ssid;
    }
    if (isRelay && hasBeenDigipited) {
        TNCAddress += '*';
    }
    return TNCAddress;
}