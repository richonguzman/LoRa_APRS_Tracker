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

#ifdef UNIT_TEST
#include "mock_esp_log.h"
#else
#include <esp_log.h>
#endif
static const char *TAG = "Bluetooth";

#include <NMEAGPS.h>
#include <esp_bt.h>
#include "bluetooth_utils.h"
#include "configuration.h"
#include "lora_utils.h"
#include "kiss_utils.h"
#include "display.h"
extern Configuration    Config;
extern Beacon           *currentBeacon;
extern BluetoothSerial  SerialBT;
extern NMEAGPS          nmeaGPS;
extern gps_fix          gpsFix;
extern bool             bluetoothConnected;
extern bool             bluetoothActive;

namespace BLUETOOTH_Utils {
    String serialReceived;
    bool shouldSendToLoRa = false;
    bool useKiss = Config.bluetooth.useKISS? true : false;

    void setup() {
        if (!bluetoothActive) {
            btStop();
            esp_bt_controller_disable();
            ESP_LOGI(TAG, "BT controller disabled");
            return;
        }

        serialReceived.reserve(255);

        SerialBT.register_callback(BLUETOOTH_Utils::bluetoothCallback);
        SerialBT.onData(BLUETOOTH_Utils::getData); // callback instead of while to avoid RX buffer limit when NMEA data received

        String BTid = Config.bluetooth.deviceName;

        if (!SerialBT.begin(String(BTid))) {
            ESP_LOGE(TAG, "Starting Bluetooth failed!");
            displayShow("ERROR", "Starting Bluetooth failed!", "");
            while(true) {
                delay(1000);
            }
        }
        ESP_LOGI(TAG, "Bluetooth Classic init done!");
    }

    void bluetoothCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
        if (event == ESP_SPP_SRV_OPEN_EVT) {
            ESP_LOGI(TAG, "Client connected !");
            bluetoothConnected = true;
        } else if (event == ESP_SPP_CLOSE_EVT) {
            ESP_LOGI(TAG, "Client disconnected !");
            bluetoothConnected = false;
        } else {
            ESP_LOGD(TAG, "Status: %d", event);
        }
    }

    void getData(const uint8_t *buffer, size_t size) {
        if (size == 0) return;
        shouldSendToLoRa = false;
        serialReceived.clear();
        bool isNmea = buffer[0] == '$';
        ESP_LOGD(TAG, "Received buffer size %d. Nmea=%d. %s", size, isNmea, buffer);

        for (int i = 0; i < size; i++) {
            ESP_LOGD(TAG, "[%d/%d] %x -> %c", i + 1, size, buffer[i], buffer[i]);
        }
        for (int i = 0; i < size; i++) {
            char c = (char) buffer[i];
            if (isNmea) {
                nmeaGPS.handle(c);
                if (nmeaGPS.available()) {
                    gpsFix = nmeaGPS.read();
                }
            } else {
                serialReceived += c;
            }
        }
        // Test if we have to send frame
        isNmea = serialReceived.indexOf("$G") != -1 || serialReceived.indexOf("$B") != -1;
        if (isNmea) useKiss = false;
        if (isNmea || serialReceived.isEmpty()) return;
        if (KISS_Utils::validateKISSFrame(serialReceived)) {
            bool dataFrame;
            String decodeKiss = KISS_Utils::decodeKISS(serialReceived, dataFrame);
            serialReceived.clear();
            serialReceived += decodeKiss;
            ESP_LOGD(TAG, "It's a kiss frame. dataFrame: %d", dataFrame);
            useKiss = true;
        } else {
            useKiss = false;
        }
        if (KISS_Utils::validateTNC2Frame(serialReceived)) {
            shouldSendToLoRa = true;
            ESP_LOGD(TAG, "Data received should be transmitted to RF => %s", serialReceived.c_str());
        }
    }

    void sendToLoRa() {
        if (!shouldSendToLoRa) return;
        ESP_LOGD(TAG, "Tx %s", serialReceived.c_str());
        displayShow("BT Tx >>", "", serialReceived, 1000);
        LoRa_Utils::sendNewPacket(serialReceived);
        shouldSendToLoRa = false;
    }

    void sendToPhone(const String& packet) {
        if (!packet.isEmpty()) {
            if (useKiss) {
                ESP_LOGD(TAG, "Rx Kiss %s", serialReceived.c_str());
                SerialBT.println(KISS_Utils::encodeKISS(packet));
            } else {
                ESP_LOGD(TAG, "Rx TNC2 %s", serialReceived.c_str());
                SerialBT.println(packet);
            }
        }
    }
  
}