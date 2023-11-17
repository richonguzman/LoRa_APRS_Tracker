#include <NimBLEDevice.h>
#include "ble_utils.h"
#include "msg_utils.h"
#include "display.h"
#include "logger.h"

#define SERVICE_UUID            "00000001-ba2a-46c9-ae49-01b0961f68bb"
#define CHARACTERISTIC_UUID_TX  "00000003-ba2a-46c9-ae49-01b0961f68bb"
#define CHARACTERISTIC_UUID_RX  "00000002-ba2a-46c9-ae49-01b0961f68bb"

BLEServer *pServer;
BLECharacteristic *pCharacteristicTx;
BLECharacteristic *pCharacteristicRx;

extern logging::Logger  logger;
extern bool             sendBleToLoRa;
extern String           bleLoRaPacket;
extern int              bleMsgCompose;
extern String           bleMsgAddresse;
extern String           bleMsgTxt;
extern bool             bleConnected;


class MyServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
      bleConnected = true;
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BLE", "%s", "BLE Client Connected");
      //delay(1000);
    }

    void onDisconnect(NimBLEServer* pServer) {
      bleConnected = false;
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BLE", "%s", "BLE client Disconnected, Started Advertising");
      pServer->startAdvertising();
    }
};

class MyCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic) {
    std::string receivedData = pCharacteristic->getValue();       // Read the data from the characteristic
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
          pCharacteristicTx->setValue("Adressee : " + receivedString);
          pCharacteristicTx->notify();
          pCharacteristicTx->setValue("Message ...");
          pCharacteristicTx->notify();
          break;
        case 2:
          Serial.println("Message : " + receivedString);
          bleMsgTxt = receivedString;
          pCharacteristicTx->setValue("Msg -> " + bleMsgAddresse + " : " + receivedString);
          pCharacteristicTx->notify();
          sendBleToLoRa = true;
          break;
      }
    } else {
      if (receivedString == "hola") {
        Serial.println("hola tambien");
        pCharacteristicTx->setValue("hola tambien");
        pCharacteristicTx->notify();
      } else if (receivedString=="/msg") {
        bleMsgCompose = 1;
        Serial.println("Send Message To...");
        pCharacteristicTx->setValue("Send Message To...");
        pCharacteristicTx->notify();
      }
    }
  }
};


namespace BLE_Utils {

  void setup() {
    BLEDevice::init("LoRa APRS Tracker");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);


    pCharacteristicTx = pService->createCharacteristic(
                          CHARACTERISTIC_UUID_TX,
                          NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
                        );
    pCharacteristicRx = pService->createCharacteristic(
                          CHARACTERISTIC_UUID_RX,
                          NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
                        );

    pCharacteristicRx->setCallbacks(new MyCallbacks());

    pService->start();

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pServer->getAdvertising()->setScanResponse(true);
    pServer->getAdvertising()->setMinPreferred(0x06);
    //pServer->getAdvertising()->setMaxPreferred(0x12);
    pServer->getAdvertising()->setMaxPreferred(0x0C); //12x1.25 = 15 ms (Apple BLE MIDI spec)

    pAdvertising->start(); //    pServer->getAdvertising()->start();

    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BLE", "%s", "Waiting for BLE central to connect...");
  }

  void sendToLoRa() {
    if (!sendBleToLoRa) {
      return;
    }

    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BLE Tx", "%s", bleLoRaPacket.c_str());
    show_display("BLE Tx >>", "", bleLoRaPacket, 1000);

    MSG_Utils::sendMessage(bleMsgAddresse,bleMsgTxt);
    bleMsgCompose = 0;
    bleMsgAddresse = "";
    bleMsgTxt = "";
    sendBleToLoRa = false;
  }

  void sendToPhone(const String& packet) {
    if (!packet.isEmpty()) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BLE Rx", "%s", packet.c_str());
      String receivedPacketString = "";
      for (int i=0; i<packet.length()-1;i++) {
        receivedPacketString += packet[i];
      }
      pCharacteristicTx->setValue(receivedPacketString);
      pCharacteristicTx->notify();
      delay(3);
    }
  }

}
