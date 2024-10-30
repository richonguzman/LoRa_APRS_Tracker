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

}