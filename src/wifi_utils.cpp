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

#include <esp_log.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_task_wdt.h>
#include "configuration.h"
#include "wifi_utils.h"
#include "web_utils.h"
#include "display.h"
#include "lvgl_ui.h"

extern Configuration        Config;

static const char *TAG = "WiFi";

bool        WiFiConnected           = false;
bool        WiFiStationMode         = false;
bool        WiFiEcoMode             = false;    // WiFi sleeping, waiting for retry (network unavailable)
bool        WiFiUserDisabled        = false;    // User manually disabled WiFi (no retry)
uint32_t    noClientsTime           = 0;
uint32_t    previousWiFiMillis      = 0;
uint32_t    lastWiFiDebug           = 0;
uint32_t    lastWiFiRetry           = 0;
const uint32_t WIFI_RETRY_INTERVAL  = 30 * 60 * 1000;  // 30 minutes (battery saving)
const uint32_t WIFI_CONNECT_TIMEOUT = 10 * 1000;       // 10 seconds per network at boot (non-blocking)
const uint32_t WIFI_RECONNECT_TIMEOUT = 30 * 1000;    // 30 seconds for reconnection attempts
const int      WIFI_MAX_RETRIES     = 3;               // Retry 3 times before eco mode
int            wifiRetryCount       = 0;               // Current retry counter

// Non-blocking connection state
bool        wifiConnecting          = false;    // Currently trying to connect
uint32_t    wifiConnectStartTime    = 0;        // When current attempt started
size_t      wifiCurrentNetworkIndex = 0;        // Which network we're trying
bool        wifiIsReconnecting      = false;    // True if this is a reconnect (uses longer timeout)


namespace WIFI_Utils {

    // Forward declarations
    bool tryConnectToNetwork(const WiFi_AP& network);
    void startConnectionAttempt(size_t networkIndex);
    void handleConnectionTimeout();
    void onConnectionSuccess();
    void onAllNetworksFailed();

    static bool wifiInitialized = false;

    void checkWiFi() {
        if (!wifiInitialized) {
            wifiInitialized = true;
            ESP_LOGI(TAG, "Initialized from config: %s", WiFiUserDisabled ? "disabled" : "enabled");
        }

        // User disabled WiFi manually - do nothing
        if (WiFiUserDisabled) {
            return;
        }

        // Handle non-blocking connection in progress
        if (wifiConnecting) {
            uint32_t timeout = wifiIsReconnecting ? WIFI_RECONNECT_TIMEOUT : WIFI_CONNECT_TIMEOUT;

            if (WiFi.status() == WL_CONNECTED) {
                // Connection succeeded!
                wifiConnecting = false;
                onConnectionSuccess();
            } else if ((millis() - wifiConnectStartTime) >= timeout) {
                // Timeout - try next network or give up
                handleConnectionTimeout();
            }
            return;
        }

        // Eco mode: WiFi is off, waiting for periodic retry
        if (WiFiEcoMode && WiFiStationMode) {
            uint32_t elapsed = millis() - lastWiFiRetry;
            if (elapsed >= WIFI_RETRY_INTERVAL) {
                ESP_LOGI(TAG, "Eco mode: retrying after %u ms", elapsed);
                WiFiEcoMode = false;
                wifiIsReconnecting = true;
                startStationMode();
                lastWiFiRetry = millis();
            }
            return;
        }

        if (WiFiConnected) {
            if (millis() - lastWiFiDebug >= 10000) {
                ESP_LOGD(TAG, "status=%d RSSI=%d", WiFi.status(), WiFi.RSSI());
                lastWiFiDebug = millis();
            }
            if ((WiFi.status() != WL_CONNECTED) && ((millis() - previousWiFiMillis) >= 30000)) {
                wifiRetryCount++;
                ESP_LOGW(TAG, "Connection lost, reconnecting (attempt %d/%d)...", wifiRetryCount, WIFI_MAX_RETRIES);
                WiFi.disconnect();
                wifiIsReconnecting = true;
                wifiCurrentNetworkIndex = 0;
                startConnectionAttempt(0);
                return;  // Will be handled in non-blocking section above
            }
        }
    }

    // Start a non-blocking connection attempt to a specific network
    void startConnectionAttempt(size_t networkIndex) {
        // Find next valid network
        while (networkIndex < Config.wifiAPs.size() && Config.wifiAPs[networkIndex].ssid == "") {
            networkIndex++;
        }

        if (networkIndex >= Config.wifiAPs.size()) {
            // No more networks to try
            onAllNetworksFailed();
            return;
        }

        wifiCurrentNetworkIndex = networkIndex;
        wifiConnecting = true;
        wifiConnectStartTime = millis();

        const WiFi_AP& network = Config.wifiAPs[networkIndex];
        uint32_t timeout = wifiIsReconnecting ? WIFI_RECONNECT_TIMEOUT : WIFI_CONNECT_TIMEOUT;
        ESP_LOGI(TAG, "Connecting to '%s' (timeout %ds, non-blocking)...",
            network.ssid.c_str(), timeout / 1000);
        WiFi.begin(network.ssid.c_str(), network.password.c_str());
    }

    // Called when connection timeout occurs
    void handleConnectionTimeout() {
        ESP_LOGW(TAG, "Timeout connecting to '%s'",
            Config.wifiAPs[wifiCurrentNetworkIndex].ssid.c_str());
        esp_wifi_disconnect();

        // Try next network
        startConnectionAttempt(wifiCurrentNetworkIndex + 1);
    }

    // Called when connection succeeds
    void onConnectionSuccess() {
        WiFiConnected = true;
        WiFiEcoMode = false;
        wifiRetryCount = 0;
        wifiIsReconnecting = false;
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);  // Enable modem sleep for coexistence

        // Use public DNS servers (Cloudflare + Google) for reliable resolution
        // DHCP-provided DNS on some routers is slow or unreliable for the ESP32
        IPAddress dns1(1, 1, 1, 1);       // Cloudflare
        IPAddress dns2(8, 8, 8, 8);       // Google
        WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), dns1, dns2);

        ESP_LOGI(TAG, "Connected to '%s'! IP=%s RSSI=%d dBm DNS=1.1.1.1/8.8.8.8",
            Config.wifiAPs[wifiCurrentNetworkIndex].ssid.c_str(),
            WiFi.localIP().toString().c_str(), WiFi.RSSI());
        WEB_Utils::setup();
    }

    // Called when all networks failed to connect
    void onAllNetworksFailed() {
        wifiConnecting = false;
        wifiRetryCount++;
        ESP_LOGW(TAG, "All networks failed (attempt %d/%d)", wifiRetryCount, WIFI_MAX_RETRIES);

        if (wifiRetryCount >= WIFI_MAX_RETRIES) {
            WiFiConnected = false;
            WiFiEcoMode = true;
            wifiRetryCount = 0;
            lastWiFiRetry = millis();
            ESP_LOGW(TAG, "Max retries reached, entering eco mode (retry in 30 min)");
            #ifdef USE_LVGL_UI
                LVGL_UI::showWiFiEcoMode();
            #else
                displayShow("", " WiFi Eco Mode", "  Retry in 30 min", "", "", "", 1000);
            #endif
            esp_wifi_stop();
        } else {
            // Retry from first network
            wifiCurrentNetworkIndex = 0;
            startConnectionAttempt(0);
        }
    }

    void startBlockingWebConfig() {
        String apName = "LoRa-Tracker-AP";

        displayShow(" LoRa APRS", "    ** WEB-CONF **", "", "WiFiAP: " + apName, "IP    : 192.168.4.1", "", 0);
        ESP_LOGW(TAG, "WebConfiguration Started!");

        WiFi.mode(WIFI_MODE_NULL);
        WiFi.mode(WIFI_AP);
        WiFi.softAP(apName.c_str(), Config.wifiAutoAP.password);

        WEB_Utils::setup();

        while (true) {
            if (WiFi.softAPgetStationNum() > 0) {
                noClientsTime = 0;
            } else {
                if (noClientsTime == 0) {
                    noClientsTime = millis();
                } else if ((millis() - noClientsTime) > Config.wifiAutoAP.timeout * 60 * 1000) {
                    ESP_LOGW(TAG, "WebConfiguration Stopped!");
                    displayShow("", "", "  STOPPING WiFi AP", "", "", "", 2000);
                    Config.wifiAutoAP.active = false;
                    Config.writeFile();
                    WiFi.softAPdisconnect(true);
                    ESP.restart();
                }
            }
        }
    }

    bool tryConnectToNetwork(const WiFi_AP& network) {
        if (network.ssid == "") return false;

        unsigned long start = millis();
        ESP_LOGI(TAG, "Connecting to '%s' (timeout %ds)...", network.ssid.c_str(), WIFI_CONNECT_TIMEOUT / 1000);
        WiFi.begin(network.ssid.c_str(), network.password.c_str());

        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            esp_task_wdt_reset();  // Reset watchdog during connection attempts
            delay(500);
            if ((millis() - start) > WIFI_CONNECT_TIMEOUT) {
                // Timeout - properly stop this connection attempt
                ESP_LOGW(TAG, "Timeout after %lu ms, status=%d", millis() - start, WiFi.status());
                esp_wifi_disconnect();
                delay(100);
                break;
            }
        }

        if (WiFi.status() == WL_CONNECTED) {
            ESP_LOGI(TAG, "Connected! RSSI=%d dBm", WiFi.RSSI());
            return true;
        }
        return false;
    }

    void startStationMode() {
        WiFiStationMode = true;

        String hostName = "Tracker-";
        if (Config.beacons.size() > 0) {
            hostName += Config.beacons[0].callsign;
        }

        // Force clean WiFi restart (needed after esp_wifi_stop in eco mode)
        WiFi.mode(WIFI_OFF);
        delay(50);  // Reduced delay for faster boot
        WiFi.mode(WIFI_STA);
        WiFi.setHostname(hostName.c_str());
        delay(50);  // Reduced delay for faster boot

        // Start non-blocking connection attempt
        wifiCurrentNetworkIndex = 0;
        startConnectionAttempt(0);
    }

    bool needsWebConfig() {
        // Check with bounds safety - if vectors are empty, we need web config
        if (Config.beacons.size() == 0) {
            return true;
        }
        return (Config.wifiAutoAP.active ||
                Config.beacons[0].callsign == "NOCALL-7");
    }

    void setup() {
        // btStop() removed for WiFi/BLE coexistence
        // Modem sleep (WIFI_PS_MIN_MODEM) is enabled after connection

        #ifdef USE_LVGL_UI
            // WiFi no longer starts at boot — manual activation only via Settings.
            // First boot web-conf (NOCALL/no WiFi) is handled by main after LVGL init.
            WiFiUserDisabled = true;
        #else
            // Blocking web-conf mode if enabled, callsign NOCALL, or no WiFi network configured
            if (needsWebConfig()) {
                startBlockingWebConfig();
                // Never returns here - reboot after config
            }
            // Station Mode: connect to the configured WiFi network (if enabled)
            WiFiUserDisabled = !Config.wifiEnabled;
            if (Config.wifiEnabled) {
                startStationMode();
            }
        #endif
    }

    void stop() {
        ESP_LOGI(TAG, "Stopping WiFi for BLE coexistence");
        WiFi.disconnect(true);
        esp_wifi_stop();
        WiFiConnected = false;
        wifiConnecting = false;
        WiFiEcoMode = false;
    }

    bool isConnected() {
        return WiFiConnected;
    }

    // Non-blocking AP mode for LVGL UI
    bool startAPModeNonBlocking() {
        String apName = "LoRa-Tracker-AP";

        ESP_LOGW(TAG, "Starting AP Mode: %s", apName.c_str());

        // Stop any existing WiFi connection
        WiFi.disconnect(true);
        delay(100);

        // Configure AP mode
        WiFi.mode(WIFI_MODE_NULL);
        delay(100);
        WiFi.mode(WIFI_AP);
        delay(100);

        bool success = WiFi.softAP(apName.c_str(), Config.wifiAutoAP.password);
        if (success) {
            ESP_LOGI(TAG, "AP Started - IP: %s", WiFi.softAPIP().toString().c_str());
            WEB_Utils::setup();
            WiFiConnected = false;
            WiFiStationMode = false;
            return true;
        } else {
            ESP_LOGE(TAG, "Failed to start AP");
            return false;
        }
    }

    String getAPName() {
        return "LoRa-Tracker-AP";
    }

    String getStatusLine() {
        if (!WiFiStationMode) {
            return "";
        }
        if (WiFiConnected) {
            return "WiFi< " + WiFi.localIP().toString();
        }
        if (WiFiEcoMode) {
            return "WiFi< Eco (sleep)";
        }
        return "WiFi< No connection";
    }

}
