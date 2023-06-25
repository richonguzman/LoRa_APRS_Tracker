#include "configuration.h"
#include "lora_utils.h"
#include "display.h"
#include "utils.h"

extern Beacon           *currentBeacon;
extern Configuration    Config;
extern bool             statusState;
extern bool             displayEcoMode; 
extern uint32_t         displayTime;
extern bool             displayState;
extern int              menuDisplay;

namespace utils {

char *ax25_base91enc(char *s, uint8_t n, uint32_t v) {
  /* Creates a Base-91 representation of the value in v in the string */
  /* pointed to by s, n-characters long. String length should be n+1. */
  for(s += n, *s = '\0'; n; n--) {
    *(--s) = v % 91 + 33;
    v /= 91;
  }
  return(s);
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

void startingStatus() {
  delay(3000);
  String packet = currentBeacon->callsign + ">APLRT1";
  if (Config.path != "") {
    packet += "," + Config.path;
  }
  packet += ":>https://github.com/richonguzman/LoRa_APRS_Tracker";
  LoRa_Utils::sendNewPacket(packet);
  statusState = false;
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

}