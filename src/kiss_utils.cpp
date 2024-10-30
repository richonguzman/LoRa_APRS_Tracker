#include "kiss_utils.h"


namespace KISS_Utils {

    bool validateTNC2Frame(const String& tnc2FormattedFrame) {
        int colonPos        = tnc2FormattedFrame.indexOf(':');
        int greaterThanPos  = tnc2FormattedFrame.indexOf('>');
        return (colonPos != -1) && (greaterThanPos != -1) && (colonPos > greaterThanPos);
    }

}