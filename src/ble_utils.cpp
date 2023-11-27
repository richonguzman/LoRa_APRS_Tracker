#include <NimBLEDevice.h>
#include "ble_utils.h"
#include "msg_utils.h"
#include "lora_utils.h"
#include "display.h"
#include "logger.h"
#include "KISS_TO_TNC2.h"
#include "ax25_utils.h"

#include <iostream>
#include <string>
//#include <cstring>

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
    std::string receivedD = pCharacteristic->getValue();       // Read the data from the characteristic
    
    Serial.println(receivedD.c_str());
    const char * receivedData = receivedD.c_str();
    Serial.println(receivedData);
    size_t receivedLength = strlen(receivedData) + 1;       // Read the data from the characteristic
  
    char * receivedString = (char*) malloc(receivedLength);

    if (!receivedString) {
      Serial.println("Memory allocation failed!");
      return;
    }
    strcpy(receivedString, receivedData);


    /*String receivedString = "";
    for (int i=0; i<receivedData.length();i++) {
      receivedString += receivedData[i];
    }

    // LA1HSA-4>APFII0,WIDE1-1:@152201h5955.23N/01056.93E$/A=000620Testing..!w!G!
    //unsigned char byteArray[]  = {0xc0, 0x00, 0x82, 0xA0, 0x8C, 0x92, 0x92, 0x60, 0xE0, 0x98, 0x82, 0x62, 0x90, 0xA6, 0x82, 0x68, 0xAE, 0x92, 0x88, 0x8A, 0x62, 0x40, 0x63, 0x03, 0xF0, 0x40, 0x31, 0x35, 0x32, 0x32, 0x30, 0x31, 0x68, 0x35, 0x39, 0x35, 0x35, 0x2E, 0x32, 0x33, 0x4E, 0x2F, 0x30, 0x31, 0x30, 0x35, 0x36, 0x2E, 0x39, 0x33, 0x45, 0x24, 0x2F, 0x41, 0x3D, 0x30, 0x30, 0x30, 0x36, 0x32, 0x30, 0x54, 0x65, 0x73, 0x74, 0x69, 0x6E, 0x67, 0x2E, 0x2E, 0x21, 0x77, 0x21, 0x47, 0x21, 0xc0};

    //int arrayLength = sizeof(byteArray) / sizeof(byteArray[0]); // Calculate the length of the byte array
    //Serial.println(arrayLength);

    //std::string receivedString(reinterpret_cast<char*>(byteArray), arrayLength);

    //char* receivedString = reinterpret_cast<char*>(byteArray);


    char receivedString[arrayLength + 1];  // +1 for the null terminator
    for (int i = 0; i < arrayLength; i++) {
      receivedString[i] = static_cast<char>(receivedString[i]);
    }
    receivedString[arrayLength] = '\0';*/

    //Serial.println(receivedString);

    BLEToLoRaPacket = AX25_Utils::processAX25(receivedString);
    //
    ///BLEToLoRaPacket = "CD2RXU-7>APLRT1:>test";
    Serial.println(BLEToLoRaPacket); //just validation
    //
    sendBleToLoRa = true;
    free(receivedString);
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

    pAdvertising->start(); //    pServer->getAdvertising()->start();

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

  void sendToPhone(const String& packet) {
    if (!packet.isEmpty()) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BLE Rx", "%s", packet.c_str());
      String receivedPacketString = "";
      for (int i=0; i<packet.length();i++) {
        receivedPacketString += packet[i];
      }
      //pCharacteristicTx->setValue(receivedPacketString.c_str());
      //pCharacteristicTx->setValue((const uint8_t*)receivedPacketStrin, strlen(receivedPacketStrin));
      //pCharacteristicTx->setValue((uint8_t *)receivedPacketString.c_str(), 20);
      //pCharacteristicTx->setValue((uint8_t *)receivedPacketString.c_str(), receivedPacketString.length());
      
      /*int parts = (receivedPacketString.length()/20) + 1;
      for(int n=0;n<parts;n++) {   
        pCharacteristicTx->setValue(receivedPacketString.substring(n*20, 20)); 
        pCharacteristicTx->notify();
        delay(10);                                                                                // Bluetooth stack will go into congestion, if too many packets are sent
      }*/

      pCharacteristicTx->setValue((byte)KissSpecialCharacter::Fend);
      pCharacteristicTx->notify();
      delay(3);
      pCharacteristicTx->setValue((byte)KissCommandCode::Data);
      pCharacteristicTx->notify();
      delay(3);


      for(int n=0;n<receivedPacketString.length();n++) {   
        uint8_t _c = receivedPacketString[n];
        if (_c == KissSpecialCharacter::Fend) {
          pCharacteristicTx->setValue((byte)KissSpecialCharacter::Fesc);
          pCharacteristicTx->notify();
          delay(3);
          pCharacteristicTx->setValue((byte)KissSpecialCharacter::Tfend);
          pCharacteristicTx->notify();
          delay(3);
        } else if (_c == KissSpecialCharacter::Fesc) {
          pCharacteristicTx->setValue((byte)KissSpecialCharacter::Fesc);
          pCharacteristicTx->notify();
          delay(3);
          pCharacteristicTx->setValue((byte)KissSpecialCharacter::Tfesc);
          pCharacteristicTx->notify();
          delay(3);
        } else {
          pCharacteristicTx->setValue(&_c, 1);
          pCharacteristicTx->notify();
          delay(3);
        }       
      }
      pCharacteristicTx->setValue((byte)KissSpecialCharacter::Fend);
      pCharacteristicTx->notify();
    }
  }

}
