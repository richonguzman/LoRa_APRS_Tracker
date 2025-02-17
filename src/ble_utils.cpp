#include <NimBLEDevice.h>
#include "configuration.h"
#include "lora_utils.h"
#include "kiss_utils.h"
#include "ble_utils.h"
#include "display.h"
#include "logger.h"

#define BLE_CHUNK_SIZE  64


// APPLE - APRS.fi app
#define SERVICE_UUID_0            "00000001-ba2a-46c9-ae49-01b0961f68bb"
#define CHARACTERISTIC_UUID_TX_0  "00000003-ba2a-46c9-ae49-01b0961f68bb"
#define CHARACTERISTIC_UUID_RX_0  "00000002-ba2a-46c9-ae49-01b0961f68bb"

// ANDROID - BLE Terminal app (Serial Bluetooth Terminal from Playstore)
#define SERVICE_UUID_1            "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX_1  "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX_1  "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

BLEServer               *pServer;
BLECharacteristic       *pCharacteristicTx;
BLECharacteristic       *pCharacteristicRx;

extern Configuration    Config;
extern Beacon           *currentBeacon;
extern logging::Logger  logger;
extern bool             bluetoothConnected;
extern bool             bluetoothActive;

bool    shouldSendBLEtoLoRa     = false;
String  BLEToLoRaPacket         = "";
String  kissSerialBuffer        = "";


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
        if (Config.bluetooth.useKISS) {   // KISS (AX.25)
            std::string receivedData = pCharacteristic->getValue();
            delay(100);
            for (int i = 0; i < receivedData.length(); i++) {
                char character = receivedData[i];

                if (kissSerialBuffer.length() == 0 && character != (char)KissChar::FEND) continue;
                kissSerialBuffer += receivedData[i];
                
                if (character == (char)KissChar::FEND && kissSerialBuffer.length() > 3) {
                    bool isDataFrame = false;

                    BLEToLoRaPacket = KISS_Utils::decodeKISS(kissSerialBuffer, isDataFrame);

                    if (isDataFrame) {
                        shouldSendBLEtoLoRa = true;
                        kissSerialBuffer = "";
                    }
                }
            }
        } else {                            // TNC2
            std::string receivedData = pCharacteristic->getValue();
            String receivedString = "";
            for (int i = 0; i < receivedData.length(); i++) receivedString += receivedData[i];
            BLEToLoRaPacket = receivedString;
            shouldSendBLEtoLoRa = true;
        }
    }
};


namespace BLE_Utils {

    void stop() {
        BLEDevice::deinit();
    }
  
    void setup() {
        String BLEid = Config.bluetooth.deviceName;
        BLEDevice::init(BLEid.c_str()); 
        pServer = BLEDevice::createServer();
        pServer->setCallbacks(new MyServerCallbacks());

        BLEService *pService = nullptr;

        //  KISS (AX.25) or TNC2
        bool useKISS = Config.bluetooth.useKISS;
        pService = pServer->createService(useKISS ? SERVICE_UUID_0 : SERVICE_UUID_1);
        pCharacteristicTx = pService->createCharacteristic(useKISS ? CHARACTERISTIC_UUID_TX_0 : CHARACTERISTIC_UUID_TX_1, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
        pCharacteristicRx = pService->createCharacteristic(useKISS ? CHARACTERISTIC_UUID_RX_0 : CHARACTERISTIC_UUID_RX_1, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);

        if (pService != nullptr) {
            pCharacteristicRx->setCallbacks(new MyCallbacks());
            pService->start();

            BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
            pAdvertising->addServiceUUID(useKISS ? SERVICE_UUID_0 : SERVICE_UUID_1);

            pServer->getAdvertising()->setScanResponse(true);
            pServer->getAdvertising()->setMinPreferred(0x06);
            pServer->getAdvertising()->setMaxPreferred(0x0C);
            pAdvertising->start();
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "BLE", "%s", "Waiting for BLE central to connect...");
        } else {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "BLE", "Failed to create BLE service");
        }
    }

    void sendToLoRa() {
        if (!shouldSendBLEtoLoRa) return;

        if (!Config.acceptOwnFrameFromTNC && BLEToLoRaPacket.indexOf("::") == -1) {
            String sender = BLEToLoRaPacket.substring(0, BLEToLoRaPacket.indexOf(">"));
            if (sender == currentBeacon->callsign) {
                BLEToLoRaPacket = "";
                shouldSendBLEtoLoRa = false;
                return;
            }
        }

        logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "BLE Tx", "%s", BLEToLoRaPacket.c_str());
        displayShow("BLE Tx >>", "", BLEToLoRaPacket, 1000);
        LoRa_Utils::sendNewPacket(BLEToLoRaPacket);
        BLEToLoRaPacket = "";
        shouldSendBLEtoLoRa = false;
    }

    void txBLE(uint8_t p) {
        pCharacteristicTx->setValue(&p,1);
        pCharacteristicTx->notify();
        delay(3);
    }

    void txToPhoneOverBLE(const String& frame) {
        if (Config.bluetooth.useKISS) {   // KISS (AX.25)
            const String kissEncodedFrame = KISS_Utils::encodeKISS(frame);

            const char* t   = kissEncodedFrame.c_str();
            int length      = kissEncodedFrame.length();
            for (int i = 0; i < length; i += BLE_CHUNK_SIZE) {
                int chunkSize = (length - i < BLE_CHUNK_SIZE) ? (length - i) : BLE_CHUNK_SIZE;
                
                uint8_t* chunk = new uint8_t[chunkSize];
                memcpy(chunk, t + i, chunkSize);

                pCharacteristicTx->setValue(chunk, chunkSize);
                pCharacteristicTx->notify();
                delete[] chunk;
                delay(200);
            }
        } else {        // TNC2
            for (int n = 0; n < frame.length(); n++) txBLE(frame[n]);
            txBLE('\n');
        }
    }

    void sendToPhone(const String& packet) {
        if (!packet.isEmpty() && bluetoothConnected) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "BLE Rx", "%s", packet.c_str());
            String receivedPacketString = "";
            for (int i = 0; i < packet.length(); i++) receivedPacketString += packet[i];
            txToPhoneOverBLE(receivedPacketString);
        }
    }

}