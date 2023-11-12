#include "ble_utils.h"
#include <NimBLEDevice.h>
#include "lora_utils.h"
#include "logger.h"
#include "display.h"

#define SERVICE_UUID        "0000180A-0000-1000-8000-00805F9B34FB"
#define CHARACTERISTIC_UUID "00002A29-0000-1000-8000-00805F9B34FB"

NimBLEServer* pServer;
NimBLECharacteristic* pCharacteristic;

extern logging::Logger  logger;
extern bool             sendBleToLoRa;
extern String           bleLoRaPacket;

class MyCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar) {
    std::string receivedData = pChar->getValue();       // Read the data from the characteristic
    //Serial.print("Received message: "); Serial.println(receivedData.c_str());
    String receivedString = "";
    for (int i=0; i<receivedData.length()-2;i++) {
      receivedString += receivedData[i];
    }
    if (receivedString == "hola") {
      Serial.println("hola tambien");
      pCharacteristic->setValue("hola tambien");
      pCharacteristic->notify();
    } else if (receivedString=="/send message") {
      Serial.println("Send Message To...");
      pCharacteristic->setValue("Send Message To...");
      pCharacteristic->notify();
    } else if (receivedString=="CD2RXU-7") {
      Serial.println("Sending message to CD2RXU-7...");
      pCharacteristic->setValue("Sending message to CD2RXU-7...");
      pCharacteristic->notify();
    } else if (receivedString=="test") {
      Serial.println("Sending message TEST...");
      pCharacteristic->setValue("Sending TEST message");
      pCharacteristic->notify();
      bleLoRaPacket = "CD2RXU-7>APLRT1,WIDE1-1::XQ3OP-7  :Test con Bluetooth BLE";
      sendBleToLoRa = true;
      //pCharacteristic->setValue("TEST message sended!");
      //pCharacteristic->notify();
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
    show_display("BT Tx >>", "", bleLoRaPacket, 1000);

    LoRa_Utils::sendNewPacket(bleLoRaPacket);

    sendBleToLoRa = false;
  }

}
