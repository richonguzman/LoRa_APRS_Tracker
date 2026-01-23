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

#include <logger.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_task_wdt.h>
#include "configuration.h"
#include "wifi_utils.h"
#include "web_utils.h"
#include "display.h"
#include "lvgl_ui.h"

extern Configuration        Config;
extern logging::Logger      logger;

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
        // Initialize WiFiUserDisabled from config on first call
        if (!wifiInitialized) {
            WiFiUserDisabled = !Config.wifiEnabled;
            wifiInitialized = true;
            Serial.printf("[WiFi] Initialized from config: %s\n", Config.wifiEnabled ? "enabled" : "disabled");
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
                Serial.printf("[WiFi] Eco mode: retrying after %u ms\n", elapsed);
                WiFiEcoMode = false;
                wifiIsReconnecting = true;
                startStationMode();
                lastWiFiRetry = millis();
            }
            return;
        }

        if (WiFiConnected) {
            if (millis() - lastWiFiDebug >= 10000) {
                Serial.printf("[WiFi] status=%d RSSI=%d\n", WiFi.status(), WiFi.RSSI());
                lastWiFiDebug = millis();
            }
            if ((WiFi.status() != WL_CONNECTED) && ((millis() - previousWiFiMillis) >= 30000)) {
                wifiRetryCount++;
                Serial.printf("[WiFi] Connection lost, reconnecting (attempt %d/%d)...\n", wifiRetryCount, WIFI_MAX_RETRIES);
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
        Serial.printf("[WiFi] Connecting to '%s' (timeout %ds, non-blocking)...\n",
            network.ssid.c_str(), timeout / 1000);
        WiFi.begin(network.ssid.c_str(), network.password.c_str());
    }

    // Called when connection timeout occurs
    void handleConnectionTimeout() {
        Serial.printf("[WiFi] Timeout connecting to '%s'\n",
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
        Serial.printf("[WiFi] Connected to '%s'! IP=%s RSSI=%d dBm\n",
            Config.wifiAPs[wifiCurrentNetworkIndex].ssid.c_str(),
            WiFi.localIP().toString().c_str(), WiFi.RSSI());
        WEB_Utils::setup();
    }

    // Called when all networks failed to connect
    void onAllNetworksFailed() {
        wifiConnecting = false;
        wifiRetryCount++;
        Serial.printf("[WiFi] All networks failed (attempt %d/%d)\n", wifiRetryCount, WIFI_MAX_RETRIES);

        if (wifiRetryCount >= WIFI_MAX_RETRIES) {
            WiFiConnected = false;
            WiFiEcoMode = true;
            wifiRetryCount = 0;
            lastWiFiRetry = millis();
            Serial.println("[WiFi] Max retries reached, entering eco mode (retry in 30 min)");
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
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "Main", "WebConfiguration Started!");

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
                    logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "Main", "WebConfiguration Stopped!");
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
        Serial.printf("\n[WiFi] Connecting to '%s' (timeout %ds)... ", network.ssid.c_str(), WIFI_CONNECT_TIMEOUT / 1000);
        WiFi.begin(network.ssid.c_str(), network.password.c_str());

        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print('.');
            esp_task_wdt_reset();  // Reset watchdog during connection attempts
            delay(500);
            if ((millis() - start) > WIFI_CONNECT_TIMEOUT) {
                // Timeout - properly stop this connection attempt
                Serial.printf("\n[WiFi] Timeout after %lu ms, status=%d\n", millis() - start, WiFi.status());
                esp_wifi_disconnect();
                delay(100);
                break;
            }
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\n[WiFi] Connected! RSSI=%d dBm\n", WiFi.RSSI());
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
        if (Config.beacons.size() == 0 || Config.wifiAPs.size() == 0) {
            return true;
        }
        return (Config.wifiAutoAP.active ||
                Config.beacons[0].callsign == "NOCALL-7" ||
                Config.wifiAPs[0].ssid == "");
    }

    void setup() {
        // btStop() removed for WiFi/BLE coexistence
        // Modem sleep (WIFI_PS_MIN_MODEM) is enabled after connection

        #ifdef USE_LVGL_UI
            // Pour LVGL, on laisse le main gérer le web-conf après init LVGL
            if (Config.wifiEnabled && !needsWebConfig()) {
                startStationMode();
            }
            WiFiUserDisabled = !Config.wifiEnabled;
            // Si needsWebConfig(), le main affichera l'écran LVGL web-conf
        #else
            // Mode web-conf bloquant si activé, callsign NOCALL, ou pas de réseau WiFi configuré
            if (needsWebConfig()) {
                startBlockingWebConfig();
                // Ne revient jamais ici - reboot après config
            }
            // Mode Station: connexion au réseau WiFi configuré (si activé)
            WiFiUserDisabled = !Config.wifiEnabled;
            if (Config.wifiEnabled) {
                startStationMode();
            }
        #endif
    }

    bool isConnected() {
        return WiFiConnected;
    }

    // Non-blocking AP mode for LVGL UI
    bool startAPModeNonBlocking() {
        String apName = "LoRa-Tracker-AP";

        logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "WiFi", "Starting AP Mode: %s", apName.c_str());

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
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "WiFi", "AP Started - IP: %s", WiFi.softAPIP().toString().c_str());
            WEB_Utils::setup();
            WiFiConnected = false;
            WiFiStationMode = false;
            return true;
        } else {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "WiFi", "Failed to start AP");
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
