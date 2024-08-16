#include <NimBLEDevice.h>
#include "configuration.h"
#include "ax25_utils.h"
#include "lora_utils.h"
#include "ble_utils.h"
#include "display.h"
#include "logger.h"


// APPLE - APRS.fi app
#define SERVICE_UUID_0            "00000001-ba2a-46c9-ae49-01b0961f68bb"
#define CHARACTERISTIC_UUID_TX_0  "00000003-ba2a-46c9-ae49-01b0961f68bb"
#define CHARACTERISTIC_UUID_RX_0  "00000002-ba2a-46c9-ae49-01b0961f68bb"

// ANDROID - BLE Terminal app (Serial Bluetooth Terminal from Playstore)
#define SERVICE_UUID_2            "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX_2  "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX_2  "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

BLEServer *pServer;
BLECharacteristic *pCharacteristicTx;
BLECharacteristic *pCharacteristicRx;

extern Configuration    Config;
extern logging::Logger  logger;
extern bool             sendBleToLoRa;
extern bool             bluetoothConnected;
extern String           BLEToLoRaPacket;


class MyServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        bluetoothConnected = true;
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BLE", "%s", "BLE Client Connected");
        delay(100);
    }

    void onDisconnect(NimBLEServer* pServer) {
        bluetoothConnected = false;
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BLE", "%s", "BLE client Disconnected, Started Advertising");
        delay(100);
        pServer->startAdvertising();
    }
};

class MyCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *pCharacteristic) {
        std::string receivedData = pCharacteristic->getValue();
        String receivedString = "";
        for (int i = 0; i < receivedData.length(); i++) {
            //Serial.print(receivedData[i],HEX); // delete
            //Serial.print(" ");
            receivedString += receivedData[i];
        }
        if (Config.bluetoothType == 0) {
            BLEToLoRaPacket = AX25_Utils::AX25FrameToLoRaPacket(receivedString);
        } else if (Config.bluetoothType == 2) {
            BLEToLoRaPacket = receivedString;
        }
        sendBleToLoRa = true;
    }
};


namespace BLE_Utils {

    void stop() {
        BLEDevice::deinit();
    }
  
    void setup() {
        uint8_t dmac[6];
        esp_efuse_mac_get_default(dmac);
        std::string BLEid = "LoRa Tracker " + std::to_string(dmac[4]) + std::to_string(dmac[5]);
        BLEDevice::init(BLEid);
        pServer = BLEDevice::createServer();
        pServer->setCallbacks(new MyServerCallbacks());

        BLEService *pService = nullptr;

        if (Config.bluetoothType == 0) {
            pService = pServer->createService(SERVICE_UUID_0);
            pCharacteristicTx = pService->createCharacteristic(CHARACTERISTIC_UUID_TX_0, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
            pCharacteristicRx = pService->createCharacteristic(CHARACTERISTIC_UUID_RX_0, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
        } else if (Config.bluetoothType == 2) {
            pService = pServer->createService(SERVICE_UUID_2);
            pCharacteristicTx = pService->createCharacteristic(CHARACTERISTIC_UUID_TX_2, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
            pCharacteristicRx = pService->createCharacteristic(CHARACTERISTIC_UUID_RX_2, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
        }

        if (pService != nullptr) {
            pCharacteristicRx->setCallbacks(new MyCallbacks());
            pService->start();

            BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();

            if (Config.bluetoothType == 0) {
                pAdvertising->addServiceUUID(SERVICE_UUID_0);
            } else if (Config.bluetoothType == 2) {
                pAdvertising->addServiceUUID(SERVICE_UUID_2);
            }
            pServer->getAdvertising()->setScanResponse(true);
            pServer->getAdvertising()->setMinPreferred(0x06);
            pServer->getAdvertising()->setMaxPreferred(0x0C);
            pAdvertising->start();
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "BLE", "%s", "Waiting for BLE central to connect...");
        } else {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "BLE", "Failed to create BLE service. Invalid bluetoothType: %d", Config.bluetoothType);
        }
    }

    void sendToLoRa() {
        if (!sendBleToLoRa) {
            return;
        }
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "BLE Tx", "%s", BLEToLoRaPacket.c_str());
        displayShow("BLE Tx >>", "", BLEToLoRaPacket, 1000);
        LoRa_Utils::sendNewPacket(BLEToLoRaPacket);
        BLEToLoRaPacket = "";
        sendBleToLoRa = false;
    }

    void txBLE(uint8_t p) {
        pCharacteristicTx->setValue(&p,1);
        pCharacteristicTx->notify();
        delay(3);
    }

    void txToPhoneOverBLE(const String& frame) {
        if (Config.bluetoothType == 0) {
            txBLE((byte)KissChar::Fend);
            txBLE((byte)KissCmd::Data);
        }        
        for(int n = 0; n < frame.length(); n++) {
            uint8_t byteCharacter = frame[n];
            if (Config.bluetoothType == 2) {
                txBLE(byteCharacter);
            } else {
                if (byteCharacter == KissChar::Fend) {
                    txBLE((byte)KissChar::Fesc);
                    txBLE((byte)KissChar::Tfend);
                } else if (byteCharacter == KissChar::Fesc) {
                    txBLE((byte)KissChar::Fesc);
                    txBLE((byte)KissChar::Tfesc);
                } else {
                    txBLE(byteCharacter);
                }
            }    
        }
        if (Config.bluetoothType == 0) {
            txBLE((byte)KissChar::Fend);
        } else if (Config.bluetoothType == 2) {
            txBLE('\n');
        }   
    }

    void sendToPhone(const String& packet) {
        if (!packet.isEmpty() && bluetoothConnected) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "BLE Rx", "%s", packet.c_str());
            String receivedPacketString = "";
            for (int i = 0; i < packet.length(); i++) {
                receivedPacketString += packet[i];
            }
            if (Config.bluetoothType == 0) {
                txToPhoneOverBLE(AX25_Utils::LoRaPacketToAX25Frame(receivedPacketString));
            } else if (Config.bluetoothType == 2) {
                txToPhoneOverBLE(receivedPacketString);                
            }
        }
    }

}