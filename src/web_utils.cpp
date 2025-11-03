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

// 解决Arduino min/max宏与C++标准库的冲突
#undef min
#undef max

#include <ArduinoJson.h>
#include "configuration.h"
#include "display.h"
#include "utils.h"

#ifdef PLATFORM_ESP32
#include "web_utils.h"

namespace WEB_Utils {

    AsyncWebServer server(80);

    void handleNotFound(AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(404, "text/plain", "Not found");
        response->addHeader("Cache-Control", "max-age=3600");
        request->send(response);
    }

    void handleStatus(AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "OK");
    }

    void handleHome(AsyncWebServerRequest *request) {

        AsyncWebServerResponse *response = request->beginResponse(200, "text/html", (const uint8_t*)web_index_html, web_index_html_len);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
    }

    void handleFavicon(AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(200, "image/x-icon", (const uint8_t*)favicon_data, favicon_data_len);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
    }

    void handleReadConfiguration(AsyncWebServerRequest *request) {

        File file = SPIFFS.open("/tracker_conf.json");
        
        String fileContent;
        while(file.available()){
            fileContent += String((char)file.read());
        }

        request->send(200, "application/json", fileContent);
    }

    void handleReceivedPackets(AsyncWebServerRequest *request) {
        StaticJsonDocument<2048> data;

        String buffer;

        serializeJson(data, buffer);
        request->send(200, "application/json", buffer);
    }

    void setup() {
        // Web server setup for ESP32 platform
        server.onNotFound(handleNotFound);
        server.on("/", HTTP_GET, handleHome);
        server.on("/favicon.ico", HTTP_GET, handleFavicon);
        server.on("/status", HTTP_GET, handleStatus);
        server.on("/readconf", HTTP_GET, handleReadConfiguration);
        server.on("/rxpackets", HTTP_GET, handleReceivedPackets);
        server.begin();
    }

    void loop() {
        // Web server loop
    }
}

#else

// 非ESP32平台的空实现
#include "platform_compat.h"
namespace WEB_Utils {
    void setup() {
        // 非ESP32平台不支持Web服务器功能
    }
    
    void loop() {
        // 非ESP32平台不支持Web服务器功能
    }
}

#endif