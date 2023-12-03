#include <NimBLEDevice.h>
#include "ble_utils.h"
#include "lora_utils.h"
#include "display.h"
#include "logger.h"
#include "KISS_TO_TNC2.h"
#include "ax25_utils.h"

#define SERVICE_UUID            "00000001-ba2a-46c9-ae49-01b0961f68bb"
#define CHARACTERISTIC_UUID_TX  "00000003-ba2a-46c9-ae49-01b0961f68bb"
#define CHARACTERISTIC_UUID_RX  "00000002-ba2a-46c9-ae49-01b0961f68bb"

BLEServer *pServer;
BLECharacteristic *pCharacteristicTx;
BLECharacteristic *pCharacteristicRx;

extern logging::Logger  logger;
extern bool             sendBleToLoRa;
extern bool             bleConnected;
extern String           BLEToLoRaPacket;


class MyServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
      bleConnected = true;
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BLE", "%s", "BLE Client Connected");
    }

    void onDisconnect(NimBLEServer* pServer) {
      bleConnected = false;
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BLE", "%s", "BLE client Disconnected, Started Advertising");
      pServer->startAdvertising();
    }
};

class MyCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic) {
    std::string receivedData = pCharacteristic->getValue();
    String receivedString = "";
    for (int i=0; i<receivedData.length();i++) {
      receivedString += receivedData[i];
    }
    BLEToLoRaPacket = AX25_Utils::AX25FrameToLoRaPacket(receivedString);
    sendBleToLoRa = true;
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
    pServer->getAdvertising()->setMaxPreferred(0x0C);

    pAdvertising->start();

    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BLE", "%s", "Waiting for BLE central to connect...");
  }

  void sendToLoRa() {
    if (!sendBleToLoRa) {
      return;
    }

    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BLE Tx", "%s", BLEToLoRaPacket.c_str());
    show_display("BLE Tx >>", "", BLEToLoRaPacket, 1000);

    LoRa_Utils::sendNewPacket(BLEToLoRaPacket);
    BLEToLoRaPacket = "";
    sendBleToLoRa = false;
  }

  void txBLE(uint8_t p) {
    pCharacteristicTx->setValue(&p,1);
    pCharacteristicTx->notify();
    delay(3);
  }

  void txToPhoneOverBLE(String frame) {
    txBLE((byte)KissSpecialCharacter::Fend);
    txBLE((byte)KissCommandCode::Data);
    for(int n=0;n<frame.length();n++) {   
      uint8_t byteCharacter = frame[n];
      if (byteCharacter == KissSpecialCharacter::Fend) {
        txBLE((byte)KissSpecialCharacter::Fesc);
        txBLE((byte)KissSpecialCharacter::Tfend);
      } else if (byteCharacter == KissSpecialCharacter::Fesc) {
        txBLE((byte)KissSpecialCharacter::Fesc);
        txBLE((byte)KissSpecialCharacter::Tfesc);
      } else {
        txBLE(byteCharacter);
      }       
    }
    txBLE((byte)KissSpecialCharacter::Fend);
  }

  void sendToPhone(const String& packet) {
    if (!packet.isEmpty()) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BLE Rx", "%s", packet.c_str());
      String receivedPacketString = "";
      for (int i=0; i<packet.length();i++) {
        receivedPacketString += packet[i];
      }
      String AX25Frame = AX25_Utils::LoRaPacketToAX25Frame(receivedPacketString);
      txToPhoneOverBLE(AX25Frame);      
    }
  }

}