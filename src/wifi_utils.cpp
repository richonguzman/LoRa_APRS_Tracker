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
const uint32_t WIFI_RETRY_INTERVAL  = 30 * 60 * 1000;  // 30 minutes


namespace WIFI_Utils {

    // Forward declaration
    bool tryConnectToNetwork(const WiFi_AP& network);

    void checkWiFi() {
        // User disabled WiFi manually - do nothing
        if (WiFiUserDisabled) {
            return;
        }

        // Eco mode: WiFi is off, waiting for periodic retry
        if (WiFiEcoMode && WiFiStationMode) {
            uint32_t elapsed = millis() - lastWiFiRetry;
            if (elapsed >= WIFI_RETRY_INTERVAL) {
                Serial.printf("[WiFi] Eco mode: retrying after %u ms\n", elapsed);
                WiFiEcoMode = false;
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
                Serial.println("[WiFi] Connection lost, reconnecting...");
                WiFi.disconnect();
                // Try each configured network
                bool reconnected = false;
                for (size_t i = 0; i < Config.wifiAPs.size(); i++) {
                    if (Config.wifiAPs[i].ssid != "" && tryConnectToNetwork(Config.wifiAPs[i])) {
                        reconnected = true;
                        break;
                    }
                }
                if (reconnected) {
                    WiFiConnected = true;
                    WiFiEcoMode = false;
                } else {
                    // Enter eco mode: turn off WiFi and retry later
                    WiFiConnected = false;
                    WiFiEcoMode = true;
                    lastWiFiRetry = millis();
                    Serial.println("[WiFi] Entering eco mode, retry in 30 min");
                    // Proper WiFi shutdown sequence using ESP-IDF API
                    esp_wifi_disconnect();
                    delay(100);
                    esp_wifi_stop();
                    delay(100);
                }
                previousWiFiMillis = millis();
            }
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
        Serial.print("\nConnecting to WiFi '");
        Serial.print(network.ssid);
        Serial.print("' ");
        WiFi.begin(network.ssid.c_str(), network.password.c_str());

        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print('.');
            delay(500);
            if ((millis() - start) > 15000) {
                // Timeout - properly stop this connection attempt
                esp_wifi_disconnect();
                delay(100);
                break;
            }
        }

        return WiFi.status() == WL_CONNECTED;
    }

    void startStationMode() {
        WiFiStationMode = true;

        String hostName = "Tracker-";
        if (Config.beacons.size() > 0) {
            hostName += Config.beacons[0].callsign;
        }

        // Force clean WiFi restart (needed after esp_wifi_stop in eco mode)
        WiFi.mode(WIFI_OFF);
        delay(100);
        WiFi.mode(WIFI_STA);
        WiFi.setHostname(hostName.c_str());
        delay(100);

        // Try each configured network
        bool connected = false;
        for (size_t i = 0; i < Config.wifiAPs.size() && !connected; i++) {
            if (Config.wifiAPs[i].ssid != "") {
                connected = tryConnectToNetwork(Config.wifiAPs[i]);
            }
        }

        if (connected) {
            WiFiConnected = true;
            WiFiEcoMode = false;
            // Enable modem sleep for WiFi/BLE coexistence (required for coex to work)
            esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
            Serial.print("\nConnected as ");
            Serial.print(WiFi.localIP());
            Serial.print(" / MAC Address: ");
            Serial.println(WiFi.macAddress());
            WEB_Utils::setup();
        } else {
            WiFiConnected = false;
            WiFiEcoMode = true;
            lastWiFiRetry = millis();
            Serial.println("\nNot connected to WiFi! Entering eco mode, retry in 30 min");
            #ifdef USE_LVGL_UI
                LVGL_UI::showWiFiEcoMode();
            #else
                displayShow("", " WiFi Eco Mode", "  Retry in 1 min", "", "", "", 1000);
            #endif
            // Proper WiFi shutdown
            esp_wifi_stop();
        }
    }

    bool needsWebConfig() {
        return (Config.wifiAutoAP.active ||
                Config.beacons[0].callsign == "NOCALL-7" ||
                Config.wifiAPs.size() == 0 ||
                Config.wifiAPs[0].ssid == "");
    }

    void setup() {
        // btStop() removed for WiFi/BLE coexistence
        // Modem sleep (WIFI_PS_MIN_MODEM) is enabled after connection

        #ifdef USE_LVGL_UI
            // Pour LVGL, on laisse le main gérer le web-conf après init LVGL
            if (!needsWebConfig()) {
                startStationMode();
            }
            // Si needsWebConfig(), le main affichera l'écran LVGL web-conf
        #else
            // Mode web-conf bloquant si activé, callsign NOCALL, ou pas de réseau WiFi configuré
            if (needsWebConfig()) {
                startBlockingWebConfig();
                // Ne revient jamais ici - reboot après config
            }
            // Mode Station: connexion au réseau WiFi configuré
            startStationMode();
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
