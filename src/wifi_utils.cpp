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
#include "configuration.h"
#include "web_utils.h"
#include "display.h"


extern      Configuration       Config;
extern      logging::Logger     logger;

uint32_t    noClientsTime        = 0;


namespace WIFI_Utils {

    void startAutoAP() {
        WiFi.mode(WIFI_MODE_NULL);
        WiFi.mode(WIFI_AP);
        WiFi.softAP("LoRaTracker-AP", Config.wifiAP.password);
    }

    void checkIfWiFiAP() {
        if (Config.wifiAP.active || Config.beacons[0].callsign == "NOCALL-7"){
            displayShow(" LoRa APRS", "    ** WEB-CONF **","", "WiFiAP:LoRaTracker-AP", "IP    :   192.168.4.1","");
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "Main", "WebConfiguration Started!");
            startAutoAP();
            WEB_Utils::setup();
            while (true) {
                if (WiFi.softAPgetStationNum() > 0) {
                    noClientsTime = 0;
                } else {
                    if (noClientsTime == 0) {
                        noClientsTime = millis();
                    } else if ((millis() - noClientsTime) > 2 * 60 * 1000) {
                        logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "Main", "WebConfiguration Stopped!");
                        displayShow("", "", "  STOPPING WiFi AP", 2000);
                        Config.wifiAP.active = false;
                        Config.writeFile();
                        WiFi.softAPdisconnect(true);
                        ESP.restart();
                    }
                }
            }
        } else {
            WiFi.mode(WIFI_OFF);
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Main", "WiFi controller stopped");
        }
    }
}