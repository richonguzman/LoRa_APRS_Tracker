#include "sleep_utils.h"
#include "boards_pinout.h"
/*#include "configuration.h"
#include "station_utils.h"
#include "boards_pinout.h"
#include "power_utils.h"
#include "gps_utils.h"
#include "display.h"
#include "logger.h"

#include "APRSPacketLib.h"

#ifdef HIGH_GPS_BAUDRATE
    #define GPS_BAUD  115200
#else
    #define GPS_BAUD  9600
#endif

extern Configuration    Config;
extern HardwareSerial   neo6m_gps;      // cambiar a gpsSerial
extern TinyGPSPlus      gps;
extern Beacon           *currentBeacon;
extern logging::Logger  logger;
extern bool             sendUpdate;
extern bool		        sendStandingUpdate;

extern uint32_t         lastTxTime;
extern uint32_t         txInterval;
extern double           lastTxLat;
extern double           lastTxLng;
extern double           lastTxDistance;
extern uint32_t         lastTx;
extern bool             disableGPS;

double      currentHeading  = 0;
double      previousHeading = 0;
float       bearing         = 0;*/

bool wakeUpFlag = false;


namespace SLEEP_Utils {

    void handle_wakeup() {
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
        switch (wakeup_cause) {
            case ESP_SLEEP_WAKEUP_TIMER:
                Serial.println("Woken up by timer (Sending Beacon) \n");
                break;
            case ESP_SLEEP_WAKEUP_EXT1:
                Serial.println("Woken up by EXT1 (GPIO) (Packet Received)\n");
                break;
            default:
                Serial.println("Woken up by unknown reason\n");
                break;
        }
    }


    void wakeUpLoRaPacketReceived() {
        wakeUpFlag = true;
    }

    void setup() {
        pinMode(RADIO_WAKEUP_PIN, INPUT);
        attachInterrupt(digitalPinToInterrupt(RADIO_WAKEUP_PIN), wakeUpLoRaPacketReceived, RISING);
        //LoRa_Utils::wakeRadio();
    }

    // getWakeUpReason

    // setSleepTimeUntilBeacon

    // startSleeping (sleep)



    // if MSG .... 
        // check if its for me -> process -> setSleepTimeUntilBeacon -> sleep
        // else ignore -> setSleepTimeUntilBeacon -> sleep
    // if BeaconInterval
        // turn on GPS -> get gps Fix -> processBeacon (send it) -> setSleepTimeUntilBeacon -> sleep
    // if userButtonPressed
        // wake up without any sleep for X min -> if nothing is done -> setSleepTimeUntilBeacon -> sleep


    

}