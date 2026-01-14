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

#include <ArduinoJson.h>
#include "configuration.h"
#include "web_utils.h"
#include "display.h"
#include "utils.h"

extern Configuration               Config;

extern const char web_index_html[] asm("_binary_data_embed_index_html_gz_start");
extern const char web_index_html_end[] asm("_binary_data_embed_index_html_gz_end");
extern const size_t web_index_html_len = web_index_html_end - web_index_html;

extern const char web_style_css[] asm("_binary_data_embed_style_css_gz_start");
extern const char web_style_css_end[] asm("_binary_data_embed_style_css_gz_end");
extern const size_t web_style_css_len = web_style_css_end - web_style_css;

extern const char web_script_js[] asm("_binary_data_embed_script_js_gz_start");
extern const char web_script_js_end[] asm("_binary_data_embed_script_js_gz_end");
extern const size_t web_script_js_len = web_script_js_end - web_script_js;

extern const char web_bootstrap_css[] asm("_binary_data_embed_bootstrap_css_gz_start");
extern const char web_bootstrap_css_end[] asm("_binary_data_embed_bootstrap_css_gz_end");
extern const size_t web_bootstrap_css_len = web_bootstrap_css_end - web_bootstrap_css;

extern const char web_bootstrap_js[] asm("_binary_data_embed_bootstrap_js_gz_start");
extern const char web_bootstrap_js_end[] asm("_binary_data_embed_bootstrap_js_gz_end");
extern const size_t web_bootstrap_js_len = web_bootstrap_js_end - web_bootstrap_js;

// Declare external symbols for the embedded image data
extern const unsigned char favicon_data[] asm("_binary_data_embed_favicon_png_gz_start");
extern const unsigned char favicon_data_end[] asm("_binary_data_embed_favicon_png_gz_end");
extern const size_t favicon_data_len = favicon_data_end - favicon_data;

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
        Serial.printf("[WebServer] GET / from %s\n", request->client()->remoteIP().toString().c_str());
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

    void handleWriteConfiguration(AsyncWebServerRequest *request) {
        Serial.println("Got new config from www");

        auto getParamStringSafe = [&](const String& name, const String& defaultValue = "") -> String {
            if (request->hasParam(name, true)) {
                return request->getParam(name, true)->value();
            }
            return defaultValue;
        };

        auto getParamIntSafe = [&](const String& name, int defaultValue = 0) -> int {
            if (request->hasParam(name, true)) {
                return request->getParam(name, true)->value().toInt();
            }
            return defaultValue;
        };

        auto getParamFloatSafe = [&](const String& name, float defaultValue = 0.0) -> float {
            if (request->hasParam(name, true)) {
                return request->getParam(name, true)->value().toFloat();
            }
            return defaultValue;
        };

        auto getParamDoubleSafe = [&](const String& name, double defaultValue = 0.0) -> double {
            if (request->hasParam(name, true)) {
                return request->getParam(name, true)->value().toDouble();
            }
            return defaultValue;
        };

        //  Beacons
        for (int i = 0; i < 3; i++) {
            Config.beacons[i].callsign      = request->getParam("beacons." + String(i) + ".callsign", true)->value();
            Config.beacons[i].symbol        = request->getParam("beacons." + String(i) + ".symbol", true)->value();
            Config.beacons[i].overlay       = request->getParam("beacons." + String(i) + ".overlay", true)->value();
            Config.beacons[i].micE          = request->getParam("beacons." + String(i) + ".micE", true)->value();
            Config.beacons[i].comment       = request->getParam("beacons." + String(i) + ".comment", true)->value();
            Config.beacons[i].status        = request->getParam("beacons." + String(i) + ".status", true)->value();
            Config.beacons[i].profileLabel  = request->getParam("beacons." + String(i) + ".profileLabel", true)->value();

            String paramGpsEcoMode = "beacons." + String(i) + ".gpsEcoMode";
            if (request->hasParam(paramGpsEcoMode, true)) {
                String paramGpsEcoModeValue = request->getParam(paramGpsEcoMode, true)->value();
                if (paramGpsEcoModeValue == "1") {
                    Config.beacons[i].gpsEcoMode = true;
                } else {
                    Config.beacons[i].gpsEcoMode = false;
                }
            } else {
                Config.beacons[i].gpsEcoMode = false;
            }
            String paramSmartBeaconActive = "beacons." + String(i) + ".smartBeaconActive";
            if (request->hasParam(paramSmartBeaconActive, true)) {
                String paramSmartBeaconActiveValue = request->getParam(paramSmartBeaconActive, true)->value();
                if (paramSmartBeaconActiveValue == "1") {
                    Config.beacons[i].smartBeaconActive = true;
                } else {
                    Config.beacons[i].smartBeaconActive = false;
                }
            } else {
                Config.beacons[i].smartBeaconActive = false;
            }
            Config.beacons[i].smartBeaconSetting    = request->getParam("beacons." + String(i) + ".smartBeaconSetting", true)->value().toInt();
        }
        
        //  Station Config
        Config.path                             = getParamStringSafe("path", Config.path);
        Config.sendCommentAfterXBeacons         = getParamIntSafe("sendCommentAfterXBeacons", Config.sendCommentAfterXBeacons);
        Config.nonSmartBeaconRate               = getParamIntSafe("nonSmartBeaconRate", Config.nonSmartBeaconRate);
        Config.standingUpdateTime               = getParamIntSafe("standingUpdateTime", Config.standingUpdateTime);
        Config.email                            = getParamStringSafe("email", Config.email);
        Config.rememberStationTime              = getParamIntSafe("rememberStationTime", Config.rememberStationTime);
        Config.sendAltitude                     = request->hasParam("sendAltitude", true);
        Config.disableGPS                       = request->hasParam("disableGPS", true);        
        Config.simplifiedTrackerMode            = request->hasParam("simplifiedTrackerMode", true);

        //  Display
        Config.display.ecoMode                  = request->hasParam("display.alwaysOn", true);
        if (!Config.display.ecoMode) {
            Config.display.timeout              = getParamIntSafe("display.timeout", Config.display.timeout);
        }
        Config.display.turn180                  = request->hasParam("display.turn180", true);
        Config.display.showSymbol               = request->hasParam("display.showSymbol", true);

        //  Bluetooth
        Config.bluetooth.active                 = request->hasParam("bluetooth.active", true);
        if (Config.bluetooth.active) {
            Config.bluetooth.deviceName         = getParamStringSafe("bluetooth.deviceName", Config.bluetooth.deviceName);
            Config.bluetooth.useBLE             = request->hasParam("bluetooth.useBLE", true);
            Config.bluetooth.useKISS            = request->hasParam("bluetooth.useKISS", true);
        }

        //  APRS-IS
        Config.aprs_is.active                   = request->hasParam("aprs_is.active", true);
        if (Config.aprs_is.active) {
            Config.aprs_is.server               = getParamStringSafe("aprs_is.server", Config.aprs_is.server);
            Config.aprs_is.port                 = getParamIntSafe("aprs_is.port", Config.aprs_is.port);
            Config.aprs_is.passcode             = getParamStringSafe("aprs_is.passcode", Config.aprs_is.passcode);
        }

        // LORA
        for (int i = 0; i < 4; i++) {
            Config.loraTypes[i].frequency       = getParamDoubleSafe("lora." + String(i) + ".frequency", Config.loraTypes[i].frequency);
            Config.loraTypes[i].spreadingFactor = getParamIntSafe("lora." + String(i) + ".spreadingFactor", Config.loraTypes[i].spreadingFactor);
            Config.loraTypes[i].codingRate4     = getParamIntSafe("lora." + String(i) + ".codingRate4", Config.loraTypes[i].codingRate4);
            Config.loraTypes[i].signalBandwidth = getParamIntSafe("lora." + String(i) + ".signalBandwidth", Config.loraTypes[i].signalBandwidth);
            Config.loraTypes[i].dataRate        = getParamIntSafe("lora." + String(i) + ".dataRate", Config.loraTypes[i].dataRate);
        }

        //  Battery
        Config.battery.sendVoltage              = request->hasParam("battery.sendVoltage", true);
        if (Config.battery.sendVoltage) {
            Config.battery.voltageAsTelemetry   = request->hasParam("battery.voltageAsTelemetry", true);
            Config.battery.sendVoltageAlways    = request->hasParam("battery.sendVoltageAlways", true);
        }
        Config.battery.monitorVoltage           = request->hasParam("battery.monitorVoltage", true);
        if (Config.battery.monitorVoltage) Config.battery.sleepVoltage = getParamFloatSafe("battery.sleepVoltage", Config.battery.sleepVoltage);

        //  Telemetry
        Config.telemetry.active                 = request->hasParam("telemetry.active", true);
        if (Config.telemetry.active) {
            Config.telemetry.sendTelemetry          = request->hasParam("telemetry.sendTelemetry", true);
            Config.telemetry.temperatureCorrection  = getParamFloatSafe("telemetry.temperatureCorrection", Config.telemetry.temperatureCorrection);
        }

        //  Winlink
        Config.winlink.password                 = getParamStringSafe("winlink.password", Config.winlink.password);

        //  WiFi Network
        while (Config.wifiAPs.size() < 2) {
            WiFi_AP wifiap;
            Config.wifiAPs.push_back(wifiap);
        }
        Config.wifiAPs[0].ssid                  = getParamStringSafe("wifi.AP.0.ssid", Config.wifiAPs[0].ssid);
        Config.wifiAPs[0].password              = getParamStringSafe("wifi.AP.0.password", Config.wifiAPs[0].password);
        Config.wifiAPs[1].ssid                  = getParamStringSafe("wifi.AP.1.ssid", Config.wifiAPs[1].ssid);
        Config.wifiAPs[1].password              = getParamStringSafe("wifi.AP.1.password", Config.wifiAPs[1].password);

        //  WiFi Auto AP
        Config.wifiAutoAP.password              = getParamStringSafe("wifi.autoAP.password", Config.wifiAutoAP.password);
        Config.wifiAutoAP.active                = false;    // Exit web-conf mode after validation

        //  Notification
        Config.notification.ledTx               = request->hasParam("notification.ledTx", true);
        if (Config.notification.ledTx) Config.notification.ledTxPin = getParamIntSafe("notification.ledTxPin", Config.notification.ledTxPin);
        Config.notification.ledMessage          = request->hasParam("notification.ledMessage", true);
        if (Config.notification.ledMessage) Config.notification.ledMessagePin = getParamIntSafe("notification.ledMessagePin", Config.notification.ledMessagePin);
        Config.notification.buzzerActive        = request->hasParam("notification.buzzerActive", true);
        if (Config.notification.buzzerActive) {
            Config.notification.buzzerPinTone   = getParamIntSafe("notification.buzzerPinTone", Config.notification.buzzerPinTone);
            Config.notification.buzzerPinVcc    = getParamIntSafe("notification.buzzerPinVcc", Config.notification.buzzerPinVcc);
            Config.notification.bootUpBeep      = request->hasParam("notification.bootUpBeep", true);
            Config.notification.txBeep          = request->hasParam("notification.txBeep", true);
            Config.notification.messageRxBeep   = request->hasParam("notification.messageRxBeep", true);
            Config.notification.stationBeep     = request->hasParam("notification.stationBeep", true);
            Config.notification.lowBatteryBeep  = request->hasParam("notification.lowBatteryBeep", true);
            Config.notification.shutDownBeep    = request->hasParam("notification.shutDownBeep", true);
        }
        Config.notification.ledFlashlight           = request->hasParam("notification.ledFlashlight", true);
        if (Config.notification.ledFlashlight) Config.notification.ledFlashlightPin = getParamIntSafe("notification.ledFlashlightPin", Config.notification.ledFlashlightPin);

        //  PTT Trigger
        Config.ptt.active                       = request->hasParam("ptt.active", true);
        if (Config.ptt.active) {
            Config.ptt.reverse                  = request->hasParam("ptt.reverse", true);
            Config.ptt.io_pin                   = getParamIntSafe("ptt.io_pin", Config.ptt.io_pin);
            Config.ptt.preDelay                 = getParamIntSafe("ptt.preDelay", Config.ptt.preDelay);
            Config.ptt.postDelay                = getParamIntSafe("ptt.postDelay", Config.ptt.postDelay);
        }

        bool saveSuccess = Config.writeFile();

        if (saveSuccess) {
            Serial.println("Configuration saved successfully");
            AsyncWebServerResponse *response = request->beginResponse(302, "text/html", "");
            response->addHeader("Location", "/?success=1");
            request->send(response);
            
            displayToggle(false);
            delay(500);
            ESP.restart();
        } else {
            Serial.println("Error saving configuration!");
            String errorPage = "<!DOCTYPE html><html><head><title>Error</title></head><body>";
            errorPage += "<h1>Configuration Error:</h1>";
            errorPage += "<p>Couldn't save new configuration. Please try again.</p>";
            errorPage += "<a href='/'>Back</a></body></html>";
            
            AsyncWebServerResponse *response = request->beginResponse(500, "text/html", errorPage);
            request->send(response);
        }
    }

    void handleAction(AsyncWebServerRequest *request) {
        String type = request->getParam("type", false)->value();

        if (type == "send-beacon") {
            //lastBeaconTx = 0;

            request->send(200, "text/plain", "Beacon will be sent in a while");
        } else if (type == "reboot") {
            displayToggle(false);
            ESP.restart();
        } else {
            request->send(404, "text/plain", "Not Found");
        }
    }

    void handleStyle(AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(200, "text/css", (const uint8_t*)web_style_css, web_style_css_len);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
    }

    void handleScript(AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(200, "text/javascript", (const uint8_t*)web_script_js, web_script_js_len);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
    }

    void handleBootstrapStyle(AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(200, "text/css", (const uint8_t*)web_bootstrap_css, web_bootstrap_css_len);
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "max-age=3600");
        request->send(response);
    }

    void handleBootstrapScript(AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(200, "text/javascript", (const uint8_t*)web_bootstrap_js, web_bootstrap_js_len);
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "max-age=3600");
        request->send(response);
    }

    void setup() {
        server.on("/", HTTP_GET, handleHome);
        server.on("/status", HTTP_GET, handleStatus);
        //server.on("/received-packets.json", HTTP_GET, handleReceivedPackets);
        server.on("/configuration.json", HTTP_GET, handleReadConfiguration);
        server.on("/configuration.json", HTTP_POST, handleWriteConfiguration);
        server.on("/action", HTTP_POST, handleAction);
        server.on("/style.css", HTTP_GET, handleStyle);
        server.on("/script.js", HTTP_GET, handleScript);
        server.on("/bootstrap.css", HTTP_GET, handleBootstrapStyle);
        server.on("/bootstrap.js", HTTP_GET, handleBootstrapScript);
        server.on("/favicon.png", HTTP_GET, handleFavicon);

        server.onNotFound(handleNotFound);

        server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            Serial.printf("[WebServer] Body: %s %s (%d bytes)\n", request->methodToString(), request->url().c_str(), total);
        });

        server.begin();
        Serial.println("[WebServer] Started on port 80");
    }

}