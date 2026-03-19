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
static const char *TAG = "APRS_IS";

#include <WiFi.h>
#include <esp_task_wdt.h>
#include "aprs_is_utils.h"
#include "configuration.h"
#include "wifi_utils.h"


extern Configuration    Config;
extern Beacon*          currentBeacon;
extern String           versionNumber;
WiFiClient              aprsIsClient;
bool                    aprsIsConnected     = false;
bool                    passcodeValid       = false;
uint32_t                lastConnectionTry   = 0;
static IPAddress        cachedServerIP;
static bool             dnsResolved         = false;


namespace APRS_IS_Utils {

    void setup() {
        // Nothing to initialize, connection is done when WiFi is available
    }

    void connect() {
        if (!Config.aprs_is.active || !WIFI_Utils::isConnected()) {
            return;
        }

        // Resolve DNS once, then reuse cached IP to avoid blocking
        if (!dnsResolved) {
            ESP_LOGI(TAG, "Resolving %s ...",
                       Config.aprs_is.server.c_str());
            if (WiFi.hostByName(Config.aprs_is.server.c_str(), cachedServerIP)) {
                dnsResolved = true;
                ESP_LOGI(TAG, "Resolved to %s",
                           cachedServerIP.toString().c_str());
            } else {
                ESP_LOGW(TAG, "DNS failed for %s",
                           Config.aprs_is.server.c_str());
                lastConnectionTry = millis();
                return;
            }
        }

        ESP_LOGI(TAG, "Connecting to %s:%d",
                   cachedServerIP.toString().c_str(), Config.aprs_is.port);

        aprsIsClient.setTimeout(2);  // 2 second TCP timeout

        if (!aprsIsClient.connect(cachedServerIP, Config.aprs_is.port)) {
            ESP_LOGW(TAG, "Connection failed");
            aprsIsClient.stop();
            aprsIsConnected = false;
            passcodeValid = false;
        } else {
            ESP_LOGI(TAG, "Connected to server");

            // Send authentication
            String aprsAuth = "user ";
            aprsAuth += currentBeacon->callsign;
            aprsAuth += " pass ";
            aprsAuth += Config.aprs_is.passcode;
            aprsAuth += " vers CA2RXU-Tracker ";
            aprsAuth += versionNumber;

            aprsIsClient.print(aprsAuth + "\r\n");
            ESP_LOGD(TAG, "Auth sent: %s", aprsAuth.c_str());

            // Wait for server response to validate passcode.
            // Some servers send the banner (#aprsc) first, then logresp — allow up to 10 s.
            bool logrexpReceived = false;
            uint32_t startWait = millis();
            while (millis() - startWait < 10000) {
                esp_task_wdt_reset();  // Reset watchdog during server response wait
                if (aprsIsClient.available()) {
                    String response = aprsIsClient.readStringUntil('\n');
                    response.trim();
                    ESP_LOGD(TAG, "Server: %s", response.c_str());

                    if (response.indexOf("verified") != -1 && response.indexOf("unverified") == -1) {
                        passcodeValid = true;
                        aprsIsConnected = true;
                        logrexpReceived = true;
                        ESP_LOGI(TAG, "Passcode verified");
                        break;
                    } else if (response.indexOf("unverified") != -1) {
                        passcodeValid = false;
                        aprsIsConnected = true;  // Connected but read-only
                        logrexpReceived = true;
                        ESP_LOGW(TAG, "Passcode invalid - read-only mode");
                        break;
                    }
                    // Banner or other line — keep waiting
                }
                delay(100);
            }
            if (!logrexpReceived) {
                ESP_LOGW(TAG, "logresp timeout (10s) — server did not confirm passcode, will retry");
                aprsIsClient.stop();
                aprsIsConnected = false;
                passcodeValid = false;
            }
        }
        lastConnectionTry = millis();
    }

    void upload(const String& packet) {
        if (!aprsIsConnected || !aprsIsClient.connected()) {
            ESP_LOGW(TAG, "Not connected, cannot upload");
            return;
        }

        if (!passcodeValid) {
            ESP_LOGW(TAG, "Passcode invalid, cannot upload");
            return;
        }

        aprsIsClient.print(packet + "\r\n");
        ESP_LOGI(TAG, "Uploaded: %s", packet.c_str());
    }

    bool isConnected() {
        return aprsIsConnected && aprsIsClient.connected();
    }

    void checkConnection() {
        if (!Config.aprs_is.active) {
            return;
        }

        if (!WIFI_Utils::isConnected()) {
            if (aprsIsConnected) {
                aprsIsClient.stop();
                aprsIsConnected = false;
                passcodeValid = false;
                dnsResolved = false;  // Re-resolve DNS on next WiFi connection
                ESP_LOGI(TAG, "Disconnected (WiFi lost)");
            }
            return;
        }

        // Check if connection was lost
        if (aprsIsConnected && !aprsIsClient.connected()) {
            aprsIsConnected = false;
            passcodeValid = false;
            ESP_LOGI(TAG, "Connection lost");
        }

        // Try to reconnect every 30 seconds if not connected
        if (!aprsIsConnected && (millis() - lastConnectionTry > 30000)) {
            connect();
        }
    }

}
