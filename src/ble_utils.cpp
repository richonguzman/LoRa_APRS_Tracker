#include "ble_utils.h"
#include <NimBLEDevice.h>
//#include "lora_utils.h"
#include "msg_utils.h"
#include "logger.h"
#include "display.h"

#define SERVICE_UUID        "0000180A-0000-1000-8000-00805F9B34FB"
#define CHARACTERISTIC_UUID "00002A29-0000-1000-8000-00805F9B34FB"

NimBLEServer* pServer;
NimBLECharacteristic* pCharacteristic;

extern logging::Logger  logger;
extern bool             sendBleToLoRa;
extern String           bleLoRaPacket;
extern int              bleMsgCompose;
extern String           bleMsgAddresse;
extern String           bleMsgTxt;

class MyCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar) {
    std::string receivedData = pChar->getValue();       // Read the data from the characteristic
    //Serial.print("Received message: "); Serial.println(receivedData.c_str());
    String receivedString = "";
    for (int i=0; i<receivedData.length()-2;i++) {
      receivedString += receivedData[i];
    }
    if (bleMsgCompose > 0) {
      switch (bleMsgCompose) {
      case 1:
        Serial.println("Adressee : " + receivedString);
        bleMsgAddresse = receivedString;
        bleMsgCompose = 2;
        pCharacteristic->setValue("Adressee : " + receivedString);
        pCharacteristic->notify();
        pCharacteristic->setValue("Message ...");
        pCharacteristic->notify();
        break;
      case 2:
        Serial.println("Message : " + receivedString);
        bleMsgTxt = receivedString;
        pCharacteristic->setValue("Msg -> " + bleMsgAddresse + " : " + receivedString);
        pCharacteristic->notify();
        sendBleToLoRa = true;
        break;
      }
    } else {
      if (receivedString == "hola") {
        Serial.println("hola tambien");
        pCharacteristic->setValue("hola tambien");
        pCharacteristic->notify();
      } else if (receivedString=="/msg") {
        bleMsgCompose = 1;
        Serial.println("Send Message To...");
        pCharacteristic->setValue("Send Message To...");
        pCharacteristic->notify();
      }
    }
  }
};


namespace BLE_Utils {

  void setup() {
    NimBLEDevice::init("LoRa APRS Tracker");
    NimBLEServer* pServer = NimBLEDevice::createServer();
    NimBLEService* pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);

    pCharacteristic->setCallbacks(new MyCallbacks());
    pService->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->start();

    Serial.println("Waiting for BLE central to connect...");
  }

  void sendToLoRa() {
    if (!sendBleToLoRa) {
      return;
    }
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BT TX", "%s", bleLoRaPacket.c_str());
    show_display("BLE Tx >>", "", bleLoRaPacket, 1000);

    MSG_Utils::sendMessage(bleMsgAddresse,bleMsgTxt);
    bleMsgCompose = 0;
    bleMsgAddresse = "";
    bleMsgTxt = "";
    sendBleToLoRa = false;
  }

}
