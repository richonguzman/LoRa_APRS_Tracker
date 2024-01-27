#include "notification_utils.h"
#include "configuration.h"
#ifdef ESP32_BV5DJ_1W_LoRa_GPS
  #include <Adafruit_NeoPixel.h>
  extern Adafruit_NeoPixel  myLED;
#endif

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

  #ifdef ESP32_BV5DJ_1W_LoRa_GPS
  void startRGB() {
    myLED.setPixelColor(0, 0xff0000); myLED.show();
    delay(150);
    myLED.setPixelColor(0, 0x00ff00); myLED.show();
    delay(150);
    myLED.setPixelColor(0, 0x0000ff); myLED.show();
    delay(150);
    myLED.setPixelColor(0, 0x000000); myLED.show();
    delay(150);
    myLED.setPixelColor(1, 0x0000ff); myLED.show();
    delay(150);
    myLED.setPixelColor(1, 0x00ff00); myLED.show();
    delay(150);
    myLED.setPixelColor(1, 0xff0000); myLED.show();
    delay(150);
    myLED.setPixelColor(1, 0x000000); myLED.show();
    delay(50);
  }

  #endif
}