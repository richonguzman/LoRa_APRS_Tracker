#include "notification_utils.h"
#include "configuration.h"

int channel       = 0;
int resolution    = 8; 
int pauseDuration = 20;

int startUpSound[]          = {440, 880, 440, 1760};
int startUpSoundDuration[]  = {100, 100, 100, 200};

int shutDownSound[]         = {1720, 880, 400};
int shutDownSoundDuration[] = {60, 60, 200};


extern Configuration    Config;

namespace Notification_Utils {

  void playTone(int frequency, int duration){
    ledcSetup(channel, frequency, resolution);
    ledcAttachPin(Config.notification.buzzerPin, 0);
    ledcWrite(channel, 128);
    delay(duration);
    ledcWrite(channel, 0);
    delay(pauseDuration);
  }

  void gpsFixBeep() {
    playTone(440,50);
    playTone(1320,200);
  }

  void beaconTxBeep() {
    playTone(880,100);
  }

  void messageBeep() {
    playTone(880,100);
    playTone(880,100);
  }

  void stationHeardBeep() {
    playTone(880,100);
    playTone(440,100);
  }

  void shutDownBeep() {
    for (int i = 0; i < sizeof(shutDownSound) / sizeof(shutDownSound[0]); i++) {
      playTone(shutDownSound[i], shutDownSoundDuration[i]);
    }
  }

  void start() {
    for (int i = 0; i < sizeof(startUpSound) / sizeof(startUpSound[0]); i++) {
      playTone(startUpSound[i], startUpSoundDuration[i]);
    }
    /*delay(3000);
    gpsFixBeep();
    delay(3000);
    shutDownBeep();   */ 
  }
    
    /*Serial.println(Config.notification.gpsFixBeep);*/
}