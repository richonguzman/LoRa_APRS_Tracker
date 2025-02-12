#include "smartbeacon_utils.h"
#include "configuration.h"
#include "winlink_utils.h"

extern Configuration    Config;
extern Beacon           *currentBeacon;
extern bool             smartBeaconActive;
extern uint32_t         txInterval;
extern uint32_t         lastTxTime;
extern bool             sendUpdate;
extern uint8_t          winlinkStatus;


SmartBeaconValues   currentSmartBeaconValues;
byte                smartBeaconSettingsIndex    = 10;
bool                wxRequestStatus             = false;
uint32_t            wxRequestTime               = 0;


SmartBeaconValues   smartBeaconSettings[3] = {
    {120,  3, 60, 15,  50, 20, 12, 60},     // Runner settings  = SLOW
    {120,  5, 60, 40, 100, 12, 12, 60},     // Bike settings    = MEDIUM
    {120, 10, 60, 70, 100, 12, 10, 80}      // Car settings     = FAST
};


namespace SMARTBEACON_Utils {

    void checkSettings(byte index) {
        if (smartBeaconSettingsIndex != index) {
            currentSmartBeaconValues = smartBeaconSettings[index];
            smartBeaconSettingsIndex = index;
        }
    }

    void checkInterval(int speed) {
        if (smartBeaconActive) {
            if (speed < currentSmartBeaconValues.slowSpeed) {
                txInterval = currentSmartBeaconValues.slowRate * 1000;
            } else if (speed > currentSmartBeaconValues.fastSpeed) {
                txInterval = currentSmartBeaconValues.fastRate * 1000;
            } else {
                txInterval = min(currentSmartBeaconValues.slowRate, currentSmartBeaconValues.fastSpeed * currentSmartBeaconValues.fastRate / speed) * 1000;
            }
        }
    }

    void checkFixedBeaconTime() {
        if (!smartBeaconActive) {
            uint32_t lastTxSmartBeacon = millis() - lastTxTime;
            if (lastTxSmartBeacon >= Config.nonSmartBeaconRate * 60 * 1000) sendUpdate = true;
        }
    }

    void checkState() {
        if (wxRequestStatus && (millis() - wxRequestTime) > 20000) wxRequestStatus = false;
        smartBeaconActive = (winlinkStatus == 0 && !wxRequestStatus) ? currentBeacon->smartBeaconActive : false;
    }
    
}