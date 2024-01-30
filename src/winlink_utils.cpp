#include "winlink_utils.h"
#include "configuration.h"

extern Configuration    Config;
extern int              winlinkStatus

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
    if (winlinkStatus == 5) {
      //menuDisplay = 800;
    } else {
      winlinkStatus = 1;
      sendMessage(1, "WLNK-1", "L");
      menuDisplay = 50;
    }
    /*
    ---genero ack number en random
    ---reviso si llega a 999 y paso a 1
    ---confirmo envio de Mensaje con ack
    ---espero confirimacion de ACK desde winlink para continuar a otro paso


    
    menu50 seria:
    si no logged:
    start login
    read msg/mails
    delete all msg/mails


    si esta logeado pasa a otro menu de :
    L
    R
    Y
    B 
    y mas
    
    
    */




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