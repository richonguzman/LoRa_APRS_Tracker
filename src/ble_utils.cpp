#include "ble_utils.h"
#include <NimBLEDevice.h>

#define SERVICE_UUID        "0000180A-0000-1000-8000-00805F9B34FB"
#define CHARACTERISTIC_UUID "00002A29-0000-1000-8000-00805F9B34FB"

extern NimBLECharacteristic* pCharacteristic;

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
    }
  }
};


namespace BLE_Utils {

void setup() {
    NimBLEDevice::init("Tracker BLE Test");
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

}
