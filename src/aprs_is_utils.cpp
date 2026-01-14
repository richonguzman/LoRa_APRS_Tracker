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

#include <WiFi.h>
#include <logger.h>
#include "aprs_is_utils.h"
#include "configuration.h"
#include "wifi_utils.h"


extern Configuration    Config;
extern Beacon*          currentBeacon;
extern String           versionNumber;
extern logging::Logger  logger;

WiFiClient              aprsIsClient;
bool                    aprsIsConnected     = false;
bool                    passcodeValid       = false;
uint32_t                lastConnectionTry   = 0;


namespace APRS_IS_Utils {

    void setup() {
        // Nothing to initialize, connection is done when WiFi is available
    }

    void connect() {
        if (!Config.aprs_is.active || !WIFI_Utils::isConnected()) {
            return;
        }

        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "APRS-IS", "Connecting to %s:%d",
                   Config.aprs_is.server.c_str(), Config.aprs_is.port);

        uint8_t count = 0;
        while (!aprsIsClient.connect(Config.aprs_is.server.c_str(), Config.aprs_is.port) && count < 5) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "APRS-IS", "Connection attempt %d failed", count + 1);
            delay(1000);
            aprsIsClient.stop();
            aprsIsClient.flush();
            count++;
        }

        if (count == 5) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "APRS-IS", "Failed to connect after %d attempts", count);
            aprsIsConnected = false;
            passcodeValid = false;
        } else {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "APRS-IS", "Connected to server");

            // Send authentication
            String aprsAuth = "user ";
            aprsAuth += currentBeacon->callsign;
            aprsAuth += " pass ";
            aprsAuth += Config.aprs_is.passcode;
            aprsAuth += " vers CA2RXU-Tracker ";
            aprsAuth += versionNumber;

            aprsIsClient.print(aprsAuth + "\r\n");
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "APRS-IS", "Auth sent: %s", aprsAuth.c_str());

            // Wait for server response to validate passcode
            uint32_t startWait = millis();
            while (millis() - startWait < 5000) {
                if (aprsIsClient.available()) {
                    String response = aprsIsClient.readStringUntil('\n');
                    response.trim();
                    logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "APRS-IS", "Server: %s", response.c_str());

                    if (response.indexOf("verified") != -1 && response.indexOf("unverified") == -1) {
                        passcodeValid = true;
                        aprsIsConnected = true;
                        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "APRS-IS", "Passcode verified");
                        break;
                    } else if (response.indexOf("unverified") != -1) {
                        passcodeValid = false;
                        aprsIsConnected = true;  // Connected but read-only
                        logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "APRS-IS", "Passcode invalid - read-only mode");
                        break;
                    }
                }
                delay(100);
            }
        }
        lastConnectionTry = millis();
    }

    void upload(const String& packet) {
        if (!aprsIsConnected || !aprsIsClient.connected()) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "APRS-IS", "Not connected, cannot upload");
            return;
        }

        if (!passcodeValid) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "APRS-IS", "Passcode invalid, cannot upload");
            return;
        }

        aprsIsClient.print(packet + "\r\n");
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "APRS-IS", "Uploaded: %s", packet.c_str());
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
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "APRS-IS", "Disconnected (WiFi lost)");
            }
            return;
        }

        // Check if connection was lost
        if (aprsIsConnected && !aprsIsClient.connected()) {
            aprsIsConnected = false;
            passcodeValid = false;
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "APRS-IS", "Connection lost");
        }

        // Try to reconnect every 30 seconds if not connected
        if (!aprsIsConnected && (millis() - lastConnectionTry > 30000)) {
            connect();
        }
    }

}
