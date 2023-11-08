#include "notification_utils.h"
#include "configuration.h"

int channel       = 0;
int resolution    = 8; 
int pauseDuration = 20;

int startUpSound[]          = {440, 880, 440, 1760};
int startUpSoundDuration[]  = {100, 100, 100, 200};

//int shutDownSound[]         = {1720, 880, 400};
//int shutDownSoundDuration[] = {60, 60, 200};

extern Configuration    Config;
extern bool             digirepeaterActive;

namespace NOTIFICATION_Utils {

  void playTone(int frequency, int duration){
    ledcSetup(channel, frequency, resolution);
    ledcAttachPin(Config.notification.buzzerPinTone, 0);
    ledcWrite(channel, 128);
    delay(duration);
    ledcWrite(channel, 0);
    delay(pauseDuration);
  }

  void beaconTxBeep() {
    digitalWrite(Config.notification.buzzerPinVcc, HIGH);
    playTone(1320,100);
    if (digirepeaterActive) {
      playTone(1560,100);
    }
    digitalWrite(Config.notification.buzzerPinVcc, LOW);
  }

  void messageBeep() {
    digitalWrite(Config.notification.buzzerPinVcc, HIGH);
    playTone(1100,100);
    playTone(1100,100);
    digitalWrite(Config.notification.buzzerPinVcc, LOW);
  }

  void stationHeardBeep() {
    digitalWrite(Config.notification.buzzerPinVcc, HIGH);
    playTone(1200,100);
    playTone(600,100);
    digitalWrite(Config.notification.buzzerPinVcc, LOW);
  }

  /*void shutDownBeep() {
    digitalWrite(Config.notification.buzzerPinVcc, HIGH);
    for (int i = 0; i < sizeof(shutDownSound) / sizeof(shutDownSound[0]); i++) {
      playTone(shutDownSound[i], shutDownSoundDuration[i]);
    }
    digitalWrite(Config.notification.buzzerPinVcc, LOW);
  }*/

  void lowBatteryBeep() {
    digitalWrite(Config.notification.buzzerPinVcc, HIGH);
    playTone(1550,100);
    playTone(650,100);
    playTone(1550,100);
    playTone(650,100);
    digitalWrite(Config.notification.buzzerPinVcc, LOW);
  }

  void start() {
    digitalWrite(Config.notification.buzzerPinVcc, HIGH);
    for (int i = 0; i < sizeof(startUpSound) / sizeof(startUpSound[0]); i++) {
      playTone(startUpSound[i], startUpSoundDuration[i]);
    }
    digitalWrite(Config.notification.buzzerPinVcc, LOW);
    /*shutDownBeep();*/ 
  }
}