#include <TinyGPS++.h>
#include "boards_pinout.h"
#include "configuration.h"
#include "station_utils.h"
#include "sleep_utils.h"
#include "power_utils.h"
#include "lora_utils.h"
#include "msg_utils.h"
#include "gps_utils.h"
#include "display.h"


extern Configuration    Config;
extern TinyGPSPlus      gps;
extern uint32_t         lastTxTime;
extern bool             sendUpdate;

bool        wakeUpFlag         = false;
bool        wakeUpByButton     = false;
uint32_t    wakeUpByButtonTime = 0;


namespace SLEEP_Utils {


    void processBeaconAfterSleep() {
        if (lastTxTime == 0 || ((millis() - lastTxTime) > 5 * 60 * 1000)) { // 5 min non-smartBeacon
            POWER_Utils::activateGPS();
            display_toggle(false);
            sendUpdate = true;
            while (sendUpdate) {
                MSG_Utils::checkReceivedMessage(LoRa_Utils::receivePacket());
                GPS_Utils::getData();
                bool gps_loc_update  = gps.location.isUpdated();
                if (gps_loc_update){
                    display_toggle(true);       // opcional
                    STATION_Utils::sendBeacon(0);
                    lastTxTime = millis();
                }
            }
        }
    }

    void processBufferAfterSleep() {
        if (!MSG_Utils::checkOutputBufferEmpty()) {
            //POWER_Utils::activateGPS();
            //display_toggle(true);
            MSG_Utils::processOutputBuffer();
        }
    }

    void handle_wakeup() {
        esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
        switch (wakeup_cause) {
            case ESP_SLEEP_WAKEUP_TIMER:
                processBeaconAfterSleep();  //Serial.println("Woken up by timer (Sending Beacon) \n");
                break;
            case ESP_SLEEP_WAKEUP_EXT1:
                //Serial.println("Woken up by EXT1 (GPIO) (Packet Received)\n");
                break;
            case ESP_SLEEP_WAKEUP_EXT0: 
                Serial.println("Wakeup caused by external signal using RTC_IO");
                wakeUpByButton = true;
                wakeUpByButtonTime = millis();

                POWER_Utils::activateGPS();
                display_toggle(true);

            default:
                processBeaconAfterSleep();  //Serial.println("Woken up by unknown reason\n");
                break;
        }
    }

    void wakeUpLoRaPacketReceived() {
        wakeUpFlag = true;
    }

    void sleep(int seconds) {
        esp_sleep_enable_timer_wakeup(300 * 1000000);
        //esp_sleep_enable_timer_wakeup(seconds * 1000000);   // 1 min = 60sec
        delay(100);
        POWER_Utils::deactivateGPS();
        delay(100);
        #ifdef ADC_CTRL
            #ifdef HELTEC_WIRELESS_TRACKER
                digitalWrite(ADC_CTRL, LOW);
            #endif
        #endif
        LoRa_Utils::wakeRadio();
        //LoRa_Utils::sleepRadio();
        delay(100);
        //esp_deep_sleep_start();
        esp_light_sleep_start();
    }

    // this could be used for smartBeaconTime delta
    /*uint32_t getTimeToSleep() { // quizas no?
        uint32_t currentCycleTime   = millis() - lastTxTime;
        uint32_t timeToSleep = 20 * 1000;//Config.nonSmartBeaconRate * 60;
        if (timeToSleep - currentCycleTime <= 0) {
            return timeToSleep / 1000;
        } else {
            return (timeToSleep - currentCycleTime) / 1000;
        }
    }*/

    void startSleep() {
        #if defined(HELTEC_WIRELESS_TRACKER)
            esp_sleep_enable_ext1_wakeup(WAKEUP_RADIO, ESP_EXT1_WAKEUP_ANY_HIGH);
            //pinMode(BUTTON_PIN, INPUT_PULLUP);   //internal pull down???
            //esp_sleep_enable_ext0_wakeup(WAKEUP_BUTTON, 0);
        #endif
        sleep(20);
    }

    void setup() {
        //if (SleepModeActive) ?????
        #ifdef RADIO_WAKEUP_PIN
            pinMode(RADIO_WAKEUP_PIN, INPUT);
            #if defined(HELTEC_WIRELESS_TRACKER)
                attachInterrupt(digitalPinToInterrupt(RADIO_WAKEUP_PIN), wakeUpLoRaPacketReceived, RISING);
                LoRa_Utils::wakeRadio();
            #else
                Serial.println("NO SLEEP MODE AVAILABLE FOR THIS BOARD");
            #endif
        #else
            Serial.println("NO SLEEP MODE AVAILABLE FOR THIS BOARD");
        #endif
    }

}