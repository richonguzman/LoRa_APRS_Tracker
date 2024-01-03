#include "APRSPacketLib.h"
#include "configuration.h"
#include "lora_utils.h"
#include "display.h"
#include "utils.h"

extern Beacon           *currentBeacon;
extern Configuration    Config;
extern bool             statusState;
extern uint32_t         statusTime;
extern uint32_t         lastTx;
extern uint32_t         lastTxTime;

extern bool             displayEcoMode; 
extern uint32_t         displayTime;
extern bool             displayState;
extern int              menuDisplay;
extern String           versionDate;
extern bool             flashlight;

namespace Utils {
  
  static char locator[11];    // letterize and getMaidenheadLocator functions are Copyright (c) 2021 Mateusz Salwach - MIT License

  static char letterize(int x) {
      return (char) x + 65;
  }

  char *getMaidenheadLocator(double lat, double lon, int size) {
    double LON_F[]={20,2.0,0.083333,0.008333,0.0003472083333333333};
    double LAT_F[]={10,1.0,0.0416665,0.004166,0.0001735833333333333};
    int i;
    lon += 180;
    lat += 90;

    if (size <= 0 || size > 10) size = 6;
    size/=2; size*=2;

    for (i = 0; i < size/2; i++){
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
      if (num.length() > width) {
          width = num.length();
      }
      for (unsigned int i = 0; i < width - num.length(); i++) {
          result.concat('0');
      }
      result.concat(num);
      return result;
  }

  String createDateString(time_t t) {
      return String(padding(year(t), 4) + "-" + padding(month(t), 2) + "-" + padding(day(t), 2));
  }

  String createTimeString(time_t t) {
      return String(padding(hour(t), 2) + ":" + padding(minute(t), 2) + ":" + padding(second(t), 2));
  }

  void checkStatus() {
    if (statusState) {
      lastTx = millis() - lastTxTime;
      uint32_t statusTx = millis() - statusTime;
      if (statusTx > 10*60*1000 && lastTx > 10*1000) {
        LoRa_Utils::sendNewPacket(APRSPacketLib::generateStatusPacket(currentBeacon->callsign, "APLRT1", Config.path, "https://github.com/richonguzman/LoRa_APRS_Tracker " + versionDate));
        statusState = false;
        lastTx = millis();
      }
    }
  }

  void checkDisplayEcoMode() {
    uint32_t lastDisplayTime = millis() - displayTime;
    if (displayEcoMode && menuDisplay==0 && millis()>10*1000 && lastDisplayTime >= Config.displayTimeout*1000) {
      display_toggle(false);
      displayState = false;
    }
  }

  String getSmartBeaconState() {
    if (currentBeacon->smartBeaconState) {
      return "On";
    }
    return "Off";
  }

  void checkFlashlight() {
    if (flashlight && !digitalRead(Config.notification.ledFlashlightPin)) {
      digitalWrite(Config.notification.ledFlashlightPin, HIGH);
    } else if (!flashlight && digitalRead(Config.notification.ledFlashlightPin)) {
      digitalWrite(Config.notification.ledFlashlightPin, LOW);
    }       
  }
  
}