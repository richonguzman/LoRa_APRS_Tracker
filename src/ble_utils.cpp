/* Copyright (C) 2026 Ricardo Guzman - CA2RXU
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
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "configuration.h"
#include "lora_utils.h"
#include "kiss_utils.h"
#include "ble_utils.h"
#include "display.h"
#include "logger.h"

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
extern logging::Logger  logger;
extern bool             bluetoothConnected;
extern bool             bluetoothActive;

bool    shouldSendBLEtoLoRa     = false;
String  BLEToLoRaPacket         = "";
String  kissSerialBuffer        = "";


// Pairing approach adapted from Antonio Megia (EA4HXZ), armegia/LoRa_APRS_Tracker,
// pr/diagnostics-ble-bonding. Keep display, logger and packet parsing out of
// NimBLE's host task. Only authenticated peers may enqueue data or receive it.
static std::atomic<uint16_t> activeHandle {BLE_HS_CONN_HANDLE_NONE};
static std::atomic<bool> authenticated {false};
static std::atomic<uint32_t> sessionId {0};
static std::atomic<uint32_t> connectionStarted {0};
static uint32_t parsingSession = 0;
static std::atomic<uint32_t> pairingPin {0};
static std::atomic<bool> showPin {false};
static std::atomic<bool> rxOverflow {false};
static uint32_t pinStartedAt = 0;
static uint32_t shownPin = 0;
struct RxChunk {
    uint16_t handle;
    uint32_t session;
    uint16_t size;
    uint8_t data[512];
};
static QueueHandle_t rxQueue = nullptr;

static bool peerIsAuthorized(uint16_t handle) {
    ble_gap_conn_desc desc;
    return handle != BLE_HS_CONN_HANDLE_NONE &&
           ble_gap_conn_find(handle, &desc) == 0 &&
           desc.sec_state.encrypted && desc.sec_state.authenticated;
}

class MyServerCallbacks : public NimBLEServerCallbacks {
    uint32_t onPassKeyRequest() override {
        showPin.store(true);
        return pairingPin.load();
    }

    void onConnect(NimBLEServer* server, ble_gap_conn_desc* desc) override {
        uint16_t empty = BLE_HS_CONN_HANDLE_NONE;
        if (!activeHandle.compare_exchange_strong(empty, desc->conn_handle)) {
            server->disconnect(desc->conn_handle);
            return;
        }
        authenticated.store(false);
        sessionId.fetch_add(1);
        connectionStarted.store(millis());
        showPin.store(false);
        // A fresh PIN for each connection; known peers reuse their saved keys.
        pairingPin.store(100000UL + esp_random() % 900000UL);
        // NimBLE 1.4.1 only calls onPassKeyRequest for DISPLAY_ONLY with this
        // sentinel configured. The callback supplies the actual random PIN.
        NimBLEDevice::setSecurityPasskey(123456);
        if (NimBLEDevice::startSecurity(desc->conn_handle) != 0) {
            server->disconnect(desc->conn_handle);
        }
    }

    void onAuthenticationComplete(ble_gap_conn_desc* desc) override {
        if (desc->conn_handle != activeHandle.load()) return;
        showPin.store(false);
        const bool ok = desc->sec_state.encrypted && desc->sec_state.authenticated;
        authenticated.store(ok);
        if (!ok) pServer->disconnect(desc->conn_handle);
    }

    void onDisconnect(NimBLEServer* server, ble_gap_conn_desc* desc) override {
        if (desc->conn_handle == activeHandle.load()) {
            authenticated.store(false);
            sessionId.fetch_add(1);
            showPin.store(false);
            activeHandle.store(BLE_HS_CONN_HANDLE_NONE);
            // Queue entries from a previous connection must never be transmitted.
            if (rxQueue) xQueueReset(rxQueue);
            rxOverflow.store(true);  // clears partial KISS frame in loop task
            server->startAdvertising();
        }
    }
};

class MyCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* characteristic, ble_gap_conn_desc* desc) override {
        if (!desc || desc->conn_handle != activeHandle.load() ||
            !authenticated.load() || !peerIsAuthorized(desc->conn_handle)) return;
        const std::string value = characteristic->getValue();
        if (value.empty()) return;
        if (value.size() > sizeof(RxChunk::data)) {
            rxOverflow.store(true);
            return;
        }
        RxChunk chunk;
        chunk.handle = desc->conn_handle;
        chunk.session = sessionId.load();
        chunk.size = value.size();
        memcpy(chunk.data, value.data(), value.size());
        if (xQueueSend(rxQueue, &chunk, 0) != pdTRUE) rxOverflow.store(true);
    }
};

static void parseReceivedData(const std::string& receivedData) {
        if (Config.bluetooth.useKISS) {   // KISS (AX.25)


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
                    if (isDataFrame) {
                        shouldSendBLEtoLoRa = true;
                        BLE_Utils::sendToLoRa();
                    }
                }
            }
        } else {                            // TNC2

            String receivedString = "";
            for (int i = 0; i < receivedData.length(); i++) receivedString += receivedData[i];
            BLEToLoRaPacket = receivedString;
            shouldSendBLEtoLoRa = true;
        }
}

namespace BLE_Utils {

    void stop() {
        authenticated.store(false);
        showPin.store(false);
        bluetoothConnected = false;
        BLEDevice::deinit();
    }

    void setup() {
        String BLEid = Config.bluetooth.deviceName;
        BLEDevice::init(BLEid.c_str());
        NimBLEDevice::setMTU(517);
        // Authenticated legacy pairing is compatible with NimBLE 1.4.1 and
        // Android clients tested by EA4HXZ. LE Secure Connections needs separate
        // compatibility testing; do not silently fall back to Just Works.
        NimBLEDevice::setSecurityAuth(true, true, false);
        NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
        NimBLEDevice::setSecurityPasskey(123456);
        NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
        NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
        if (Config.bluetooth.bondResetPending) {
            NimBLEDevice::deleteAllBonds();
            Config.bluetooth.bondResetPending = false;
            Config.writeFile();
        }
        rxQueue = xQueueCreate(8, sizeof(RxChunk));
        if (!rxQueue) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "BLE", "RX queue allocation failed; BLE disabled");
            BLEDevice::deinit();
            return;
        }
        pServer = BLEDevice::createServer();
        pServer->setCallbacks(new MyServerCallbacks());

        BLEService *pService = nullptr;

        //  KISS (AX.25) or TNC2
        bool useKISS = Config.bluetooth.useKISS;
        pService = pServer->createService(useKISS ? SERVICE_UUID_0 : SERVICE_UUID_1);
        pCharacteristicTx = pService->createCharacteristic(useKISS ? CHARACTERISTIC_UUID_TX_0 : CHARACTERISTIC_UUID_TX_1, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::READ_AUTHEN);
        pCharacteristicRx = pService->createCharacteristic(useKISS ? CHARACTERISTIC_UUID_RX_0 : CHARACTERISTIC_UUID_RX_1, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN);

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

    void loop() {
        const uint16_t handle = activeHandle.load();
        if (handle != BLE_HS_CONN_HANDLE_NONE && !authenticated.load() &&
            millis() - connectionStarted.load() >= 60000UL) {
            pServer->disconnect(handle);
        }
        bluetoothConnected = authenticated.load() && peerIsAuthorized(activeHandle.load());
        if (rxOverflow.exchange(false) || !bluetoothConnected) {
            kissSerialBuffer = "";
            BLEToLoRaPacket = "";
            shouldSendBLEtoLoRa = false;
            if (rxQueue) xQueueReset(rxQueue);
        }
        if (!bluetoothConnected || !rxQueue) return;
        RxChunk chunk;
        // Bounded work per iteration to keep radio/UI processing responsive.
        for (unsigned i = 0; i < 8 && xQueueReceive(rxQueue, &chunk, 0) == pdTRUE; ++i) {
            if (chunk.handle == activeHandle.load() && chunk.session == sessionId.load() && peerIsAuthorized(chunk.handle)) {
                parsingSession = chunk.session;
                parseReceivedData(std::string(reinterpret_cast<char*>(chunk.data), chunk.size));
                sendToLoRa();
            }
        }
    }

    void showPairing() {
        if (!showPin.load()) {
            shownPin = 0;
            return;
        }
        const uint32_t pin = pairingPin.load();
        if (shownPin != pin) {
            shownPin = pin;
            pinStartedAt = millis();
        }
        if (millis() - pinStartedAt >= 60000UL) {
            showPin.store(false);
            const uint16_t handle = activeHandle.load();
            if (handle != BLE_HS_CONN_HANDLE_NONE && !authenticated.load()) pServer->disconnect(handle);
            return;
        }
        displayWake();
        char digits[11];
        snprintf(digits, sizeof(digits), "%06lu", static_cast<unsigned long>(pin));
        displayShow("BLE PAIR", "Enter PIN on phone", "PIN: " + String(digits));
    }

    void sendToLoRa() {
        if (!shouldSendBLEtoLoRa) return;
        if (!authenticated.load() || parsingSession != sessionId.load() || !peerIsAuthorized(activeHandle.load())) {
            shouldSendBLEtoLoRa = false;
            BLEToLoRaPacket = "";
            return;
        }

        logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "BLE Tx", "%s", BLEToLoRaPacket.c_str());
        displayWake();
        displayShow("BLE Tx >>", "", BLEToLoRaPacket, 1000);
        // A disconnect can occur while the OLED message is being displayed.
        if (authenticated.load() && parsingSession == sessionId.load() &&
            peerIsAuthorized(activeHandle.load())) {
            LoRa_Utils::sendNewPacket(BLEToLoRaPacket);
        }
        BLEToLoRaPacket = "";
        shouldSendBLEtoLoRa = false;
    }

    void txBLE(uint8_t p) {
        if (!authenticated.load() || !peerIsAuthorized(activeHandle.load())) return;
        pCharacteristicTx->setValue(&p,1);
        pCharacteristicTx->notify();
        delay(3);
    }

    void txToPhoneOverBLE(const String& frame) {
        if (Config.bluetooth.useKISS) {   // KISS (AX.25)
            const String kissEncodedFrame = KISS_Utils::encodeKISS(frame);

            const char* t   = kissEncodedFrame.c_str();
            int length      = kissEncodedFrame.length();
            const uint16_t mtu = pServer->getPeerMTU(activeHandle.load());
            if (mtu <= 3) return;
            const int payloadSize = (mtu - 3 < BLE_CHUNK_SIZE) ? mtu - 3 : BLE_CHUNK_SIZE;
            for (int i = 0; i < length; i += payloadSize) {
                int chunkSize = (length - i < payloadSize) ? (length - i) : payloadSize;

                uint8_t* chunk = new uint8_t[chunkSize];
                memcpy(chunk, t + i, chunkSize);

                if (!authenticated.load() || !peerIsAuthorized(activeHandle.load())) {
                    delete[] chunk;
                    return;
                }
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
        if (!packet.isEmpty() && bluetoothConnected && authenticated.load() && peerIsAuthorized(activeHandle.load())) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "BLE Rx", "%s", packet.c_str());
            String receivedPacketString = "";
            for (int i = 0; i < packet.length(); i++) receivedPacketString += packet[i];
            txToPhoneOverBLE(receivedPacketString);
        }
    }

}
