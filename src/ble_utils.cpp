/* Copyright (C) 2025 Ricardo Guzman - CA2RXU
 * 
 * This file is part of LoRa APRS Tracker.
 * 
 * LoRa APRS Tracker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or 
 * (at your option) any later version.
 * 
 * LoRa APRS Tracker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with LoRa APRS Tracker. If not, see <https://www.gnu.org/licenses/>.
 */

#include <NimBLEDevice.h>
#include <esp_wifi.h>
#include "configuration.h"
#include "lora_utils.h"
#include "kiss_utils.h"
#include "ble_utils.h"
#include "display.h"
#include "logger.h"
#ifdef USE_LVGL_UI
#include "lvgl_ui.h"
#endif

#define BLE_CHUNK_SIZE  512
#define MAX_KISS_BUFFER 1024


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
String  bleConnectedDeviceAddr  = "";  // Connected device MAC address
String  bleConnectedDeviceName  = "";  // Connected device name (from GAP)
bool    bleNeedToReadName       = false;  // Flag to read name after connection
NimBLEAddress bleConnectedPeerAddr;  // Peer address for client connection

// BLE Eco Mode variables
bool        bleEcoMode          = true;     // BLE eco mode always active (automatic)
bool        bleSleeping         = false;    // BLE is currently sleeping (stopped)
uint32_t    bleLastActivityTime = 0;        // Last time BLE had activity (connection)
const uint32_t BLE_ECO_TIMEOUT  = 5 * 60 * 1000;  // 5 minutes timeout

class MyServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        bluetoothConnected = true;
        bleConnectedDeviceName = "";
        bleNeedToReadName = true;
        bleLastActivityTime = millis();  // Reset eco mode timer
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BLE", "%s", "BLE Client Connected");
    }

    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) {
        bluetoothConnected = true;
        // Get connected device MAC address from connection descriptor
        bleConnectedPeerAddr = NimBLEAddress(desc->peer_ota_addr);
        bleConnectedDeviceAddr = bleConnectedPeerAddr.toString().c_str();
        bleConnectedDeviceName = "";  // Will be read later
        bleNeedToReadName = true;  // Signal to read name in loop
        bleLastActivityTime = millis();  // Reset eco mode timer
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BLE", "BLE Client Connected: %s", bleConnectedDeviceAddr.c_str());
    }

    void onDisconnect(NimBLEServer* pServer) {
        bluetoothConnected = false;
        bleConnectedDeviceAddr = "";
        bleConnectedDeviceName = "";
        bleNeedToReadName = false;
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BLE", "%s", "BLE client Disconnected");
        pServer->startAdvertising();
    }

    void onDisconnect(NimBLEServer* pServer, ble_gap_conn_desc* desc, int reason) {
        bluetoothConnected = false;
        bleConnectedDeviceAddr = "";
        bleConnectedDeviceName = "";
        bleNeedToReadName = false;
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BLE", "BLE client Disconnected (reason: %d)", reason);
        pServer->startAdvertising();
    }
};

class MyCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *pCharacteristic) {
        if (Config.bluetooth.useKISS) {   // KISS (AX.25)
            std::string receivedData = pCharacteristic->getValue();

            for (uint8_t c : receivedData) {                                                // save all received data from buffer
                kissSerialBuffer += (char)c;
            }
            if (kissSerialBuffer.length() > MAX_KISS_BUFFER) {                              // buffer overflow protection
                kissSerialBuffer = "";
                return;
            }

            int maxIterations = 10;                                                        // infinite loop protection
            while (maxIterations-- > 0) {

                if (kissSerialBuffer.length() == 0) break;                                  // empty buffer protection

                int fendIndex = -1;
                if (kissSerialBuffer.charAt(0) == (char)KissChar::FEND) {                   // starts with FEND???
                    for (int i = 1; i < kissSerialBuffer.length(); i++) {                   // look for next FEND
                        if (kissSerialBuffer.charAt(i) == (char)KissChar::FEND) {
                            fendIndex = i;
                            break;
                        }
                    }
                } else {
                    int firstFendIndex = kissSerialBuffer.indexOf((char)KissChar::FEND);    // find first FEND byte to discard leading corrupted bytes
                    if (firstFendIndex != -1) {
                        kissSerialBuffer.remove(0, firstFendIndex);                         // delete corrupted data before FEND 
                    } else {
                        kissSerialBuffer = "";                                              // if no FEND found, delete all
                        break;
                    }
                    continue;
                }

                if (fendIndex == -1) {                                                      // exit: no FEND byte to process the kissSerialBuffer (yet)
                    break;
                }

                String frame = kissSerialBuffer.substring(0, fendIndex + 1);                // extract full frame (With FEND at start and end)
                kissSerialBuffer.remove(0, fendIndex + 1);
                
                if (frame.length() >= 4) {                                                  // FEND | CMD | DATA | FEND
                    bool isDataFrame    = false;
                    BLEToLoRaPacket     = KISS_Utils::decodeKISS(frame, isDataFrame);
                    if (isDataFrame) shouldSendBLEtoLoRa = true;
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
        // Initialize eco mode timer
        bleLastActivityTime = millis();
        bleSleeping = false;

        // Ensure WiFi modem sleep is enabled for WiFi/BLE coexistence
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

        String BLEid = Config.bluetooth.deviceName;
        BLEDevice::init(BLEid.c_str());
        BLEDevice::setPower(ESP_PWR_LVL_P3);  // Moderate power for coexistence
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

        logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "BLE Tx", "%s", BLEToLoRaPacket.c_str());
        #ifdef USE_LVGL_UI
            LVGL_UI::showTxPacket(BLEToLoRaPacket.c_str());
        #else
            displayShow("BLE Tx >>", "", BLEToLoRaPacket, 1000);
        #endif
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

    String getConnectedDeviceAddress() {
        return bleConnectedDeviceAddr;
    }

    String getConnectedDeviceName() {
        return bleConnectedDeviceName;
    }

    // Try to read the device name from the connected peer
    // Note: Most smartphones don't allow reverse client connections, so this usually fails
    // Keeping the function but it's essentially a no-op for now
    void tryReadDeviceName() {
        // Disabled - smartphones typically don't expose their GAP service to peripherals
        // The MAC address will be displayed instead
        bleNeedToReadName = false;
    }

    // Check BLE eco mode timeout - call this from main loop
    void checkEcoMode() {
        if (!bleEcoMode || !bluetoothActive || bleSleeping || bluetoothConnected) {
            return;  // Eco mode disabled, BLE not active, already sleeping, or connected
        }

        uint32_t now = millis();
        if (now - bleLastActivityTime >= BLE_ECO_TIMEOUT) {
            // Timeout reached - put BLE to sleep
            bleSleeping = true;
            BLEDevice::deinit();
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BLE", "Eco mode: BLE stopped after %d min inactivity", BLE_ECO_TIMEOUT / 60000);
            Serial.println("[BLE] Eco mode: BLE stopped (5 min timeout)");
        }
    }

    // Wake up BLE from eco mode sleep
    void wake() {
        if (!bleSleeping) return;

        Serial.println("[BLE] Waking up from eco mode");
        bleSleeping = false;
        bleLastActivityTime = millis();
        setup();  // Re-initialize BLE
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BLE", "Eco mode: BLE restarted");
    }

    // Check if BLE is sleeping
    bool isSleeping() {
        return bleSleeping;
    }

}