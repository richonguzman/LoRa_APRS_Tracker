#include "winlink_utils.h"
#include "configuration.h"

extern Configuration    Config;

namespace WINLINK_Utils {

  String processWinlinkChallenge(String winlinkInteger) {
    String challengeAnswer;
    for (int i=0; i<winlinkInteger.length(); i++) {
      String number = String(winlinkInteger[i]);
      int digit = number.toInt();
      challengeAnswer += Config.winlink.password[digit-1];
    }
    challengeAnswer += "AZ6";
    Serial.println("el challenge creado es " + challengeAnswer);
    return challengeAnswer;
  }

  void login() {
    Serial.println("Starting Winlink Login");
    // enviar "L" con ack
    // recibir ack - esperar challenge
    // recibir challenge con ack
    // contestar ack
    // procesar challenge
    // enviar challenge con ack
    // recibir ack
    // recibir Login valid
    // responder ack
  }

}