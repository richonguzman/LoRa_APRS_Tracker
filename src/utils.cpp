#include <APRSPacketLib.h>
#include <logger.h>
#include <Wire.h>
#include "configuration.h"
#include "board_pinout.h"
#include "lora_utils.h"
#include "display.h"
#include "utils.h"

extern Beacon                   *currentBeacon;
extern Configuration            Config;
extern logging::Logger          logger;

extern uint32_t                 lastTx;
extern uint32_t                 lastTxTime;

extern bool                     displayEcoMode;
extern uint32_t                 displayTime;
extern bool                     displayState;
extern int                      menuDisplay;
extern String                   versionDate;
extern bool                     flashlight;

extern bool                     statusState;

uint32_t    statusTime          = millis();
uint8_t     wxModuleAddress     = 0x00;
uint8_t     keyboardAddress     = 0x00;
uint8_t     touchModuleAddress  = 0x00;


namespace Utils {
  
    static char locator[11];    // letterize and getMaidenheadLocator functions are Copyright (c) 2021 Mateusz Salwach - MIT License

    static char letterize(int x) {
        return (char) x + 65;
    }

    char *getMaidenheadLocator(double lat, double lon, uint8_t size) {
        double LON_F[]={20,2.0,0.083333,0.008333,0.0003472083333333333};
        double LAT_F[]={10,1.0,0.0416665,0.004166,0.0001735833333333333};
        int i;
        lon += 180;
        lat += 90;

        if (size <= 0 || size > 10) size = 6;
        size/=2; size*=2;

        for (i = 0; i < size/2; i++) {
            if (i % 2 == 1) {
                locator[i*2] = (char) (lon/LON_F[i] + '0');
                locator[i*2+1] = (char) (lat/LAT_F[i] + '0');
            } else {
                locator[i*2] = letterize((int) (lon/LON_F[i]));
                locator[i*2+1] = letterize((int) (lat/LAT_F[i]));
            }
            lon = fmod(lon, LON_F[i]);
            lat = fmod(lat, LAT_F[i]);
        }
        locator[i*2]=0;
        return locator;
    }

    static String padding(unsigned int number, unsigned int width) {
        String result;
        String num(number);
        if (num.length() > width) width = num.length();
        for (unsigned int i = 0; i < width - num.length(); i++) {
            result.concat('0');
        }
        result.concat(num);
        return result;
    }

    String createDateString(time_t t) {
        String dateString = padding(year(t), 4);
        dateString += "-";
        dateString += padding(month(t), 2);
        dateString += "-";
        dateString += padding(day(t), 2);
        return dateString;
    }

    String createTimeString(time_t t) {
        String timeString = padding(hour(t), 2);
        timeString += ":";
        timeString += padding(minute(t), 2);
        timeString += ":";
        timeString += padding(second(t), 2);
        return timeString;
    }

    void checkStatus() {
        if (statusState) {
            uint32_t currentTime = millis();
            uint32_t statusTx = currentTime - statusTime;
            lastTx = currentTime - lastTxTime;
            if (statusTx > 10 * 60 * 1000 && lastTx > 10 * 1000) {
                LoRa_Utils::sendNewPacket(APRSPacketLib::generateStatusPacket(currentBeacon->callsign, "APLRT1", Config.path, "https://github.com/richonguzman/LoRa_APRS_Tracker " + versionDate));
                statusState = false;
            }
        }
    }

    void checkDisplayEcoMode() {
        if (displayState && displayEcoMode && menuDisplay == 0) {
            uint32_t currentTime = millis();
            uint32_t lastDisplayTime = currentTime - displayTime;
            if (currentTime > 10 * 1000 && lastDisplayTime >= Config.display.timeout * 1000) {
                displayToggle(false);
                displayState = false;
            }
        }
    }

    String getSmartBeaconState() {
        if (currentBeacon->smartBeaconActive) return "On";
        return "Off";
    }

    void checkFlashlight() {
        if (flashlight && !digitalRead(Config.notification.ledFlashlightPin)) {
            digitalWrite(Config.notification.ledFlashlightPin, HIGH);
        } else if (!flashlight && digitalRead(Config.notification.ledFlashlightPin)) {
            digitalWrite(Config.notification.ledFlashlightPin, LOW);
        }       
    }

    void i2cScannerForPeripherals() {
        uint8_t err, addr;
        if (Config.wxsensor.active) {
            for (addr = 1; addr < 0x7F; addr++) {
                #if defined(HELTEC_V3_GPS) || defined(HELTEC_V3_2_GPS)
                    Wire1.beginTransmission(addr);
                    err = Wire1.endTransmission();
                #else
                    Wire.beginTransmission(addr);
                    err = Wire.endTransmission();
                #endif
                if (err == 0) {
                    //Serial.println(addr); this shows any connected board to I2C
                    if (addr == 0x76 || addr == 0x77) {
                        wxModuleAddress = addr;
                        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Wx Module Connected to I2C");
                    }
                }
            }
        }

        for (addr = 1; addr < 0x7F; addr++) {
            Wire.beginTransmission(addr);
            err = Wire.endTransmission();
            if (err == 0) {
                //Serial.println(addr); this shows any connected board to I2C
                if (addr == 0x55) {         // T-Deck internal keyboard (Keyboard Backlight On = ALT + B)
                    keyboardAddress = addr;
                    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "T-Deck Keyboard Connected to I2C");
                } else if (addr == 0x5F) {  // CARDKB from m5stack.com (YEL - SDA / WTH SCL)
                    keyboardAddress = addr;
                    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "CARDKB Keyboard Connected to I2C");
                }
            }
        }

        #ifdef HAS_TOUCHSCREEN
            for (addr = 1; addr < 0x7F; addr++) {
                Wire.beginTransmission(addr);
                err = Wire.endTransmission();
                if (err == 0) {
                    if (addr == 0x14 || addr == 0x5D ) {
                        touchModuleAddress = addr;
                        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Touch Module Connected to I2C");
                    }
                }
            }
        #endif
    }
  
}