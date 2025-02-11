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

        //  Beacons
        for (int i = 0; i < 3; i++) {
            Config.beacons[i].callsign              = request->getParam("beacons." + String(i) + ".callsign", true)->value();
            Config.beacons[i].symbol                = request->getParam("beacons." + String(i) + ".symbol", true)->value();
            Config.beacons[i].overlay               = request->getParam("beacons." + String(i) + ".overlay", true)->value();
            Config.beacons[i].micE                  = request->getParam("beacons." + String(i) + ".micE", true)->value();
            Config.beacons[i].comment               = request->getParam("beacons." + String(i) + ".comment", true)->value();

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
        Config.simplifiedTrackerMode            = request->hasParam("simplifiedTrackerMode", true);
        Config.sendCommentAfterXBeacons         = request->getParam("sendCommentAfterXBeacons", true)->value().toInt();
        Config.path                             = request->getParam("path", true)->value();
        Config.nonSmartBeaconRate               = request->getParam("nonSmartBeaconRate", true)->value().toInt();
        Config.rememberStationTime              = request->getParam("rememberStationTime", true)->value().toInt();
        Config.standingUpdateTime               = request->getParam("standingUpdateTime", true)->value().toInt();
        Config.sendAltitude                     = request->hasParam("sendAltitude", true);
        Config.disableGPS                       = request->hasParam("disableGPS", true);
        Config.email                            = request->getParam("email", true)->value();

        //  Display
        Config.display.showSymbol               = request->hasParam("display.showSymbol", true);
        if (request->hasParam("display.ecoMode", true)) {
            Config.display.ecoMode = true;
            if (request->hasParam("display.timeout", true)) {
                Config.display.timeout = request->getParam("display.timeout", true)->value().toInt();
            }
        } else {
            Config.display.ecoMode = false;
        }
        Config.display.turn180                  = request->hasParam("display.turn180", true);

        //  Battery
        Config.battery.sendVoltage              = request->hasParam("battery.sendVoltage", true);
        Config.battery.voltageAsTelemetry       = request->hasParam("battery.voltageAsTelemetry", true);
        Config.battery.sendVoltageAlways        = request->hasParam("battery.sendVoltageAlways", true);
        Config.battery.monitorVoltage           = request->hasParam("battery.monitorVoltage", true);
        Config.battery.sleepVoltage             = request->getParam("battery.sleepVoltage", true)->value().toFloat();

        //  Winlink
        Config.winlink.password                 = request->getParam("winlink.password", true)->value();
        
        //  Wx Telemtry
        Config.wxsensor.active                  = request->hasParam("wxsensor.active", true);
        Config.wxsensor.temperatureCorrection   = request->getParam("wxsensor.temperatureCorrection", true)->value().toFloat();
        Config.wxsensor.sendTelemetry           = request->hasParam("wxsensor.sendTelemetry", true);

        //  Notification
        Config.notification.ledTx               = request->hasParam("notification.ledTx", true);
        Config.notification.ledTxPin            = request->getParam("notification.ledTxPin", true)->value().toInt();
        Config.notification.ledMessage          = request->hasParam("notification.ledMessage", true);
        Config.notification.ledMessagePin       = request->getParam("notification.ledMessagePin", true)->value().toInt();
        Config.notification.ledFlashlight       = request->hasParam("notification.ledFlashlight", true);
        Config.notification.ledFlashlightPin    = request->getParam("notification.ledFlashlightPin", true)->value().toInt();
        Config.notification.buzzerActive        = request->hasParam("notification.buzzerActive", true);
        Config.notification.buzzerPinTone       = request->getParam("notification.buzzerPinTone", true)->value().toInt();
        Config.notification.buzzerPinVcc        = request->getParam("notification.buzzerPinVcc", true)->value().toInt();
        Config.notification.bootUpBeep          = request->hasParam("notification.bootUpBeep", true);
        Config.notification.txBeep              = request->hasParam("notification.txBeep", true);
        Config.notification.messageRxBeep       = request->hasParam("notification.messageRxBeep", true);
        Config.notification.stationBeep         = request->hasParam("notification.stationBeep", true);
        Config.notification.lowBatteryBeep      = request->hasParam("notification.lowBatteryBeep", true);
        Config.notification.shutDownBeep        = request->hasParam("notification.shutDownBeep", true);

        // LORA
        for (int i = 0; i < 3; i++) {
            Config.loraTypes[i].frequency       = request->getParam("lora." + String(i) + ".frequency", true)->value().toDouble();
            Config.loraTypes[i].spreadingFactor = request->getParam("lora." + String(i) + ".spreadingFactor", true)->value().toInt();
            Config.loraTypes[i].codingRate4     = request->getParam("lora." + String(i) + ".codingRate4", true)->value().toInt();
        }

        //  Bluetooth
        Config.bluetooth.active                 = request->hasParam("bluetooth.active", true);
        Config.bluetooth.deviceName             = request->getParam("bluetooth.deviceName", true)->value();
        Config.bluetooth.useBLE                 = request->hasParam("bluetooth.useBLE", true);
        Config.bluetooth.useKISS                = request->hasParam("bluetooth.useKISS", true);

        //  PTT Trigger
        Config.ptt.active                       = request->hasParam("ptt.active", true);
        Config.ptt.io_pin                       = request->getParam("ptt.io_pin", true)->value().toInt();
        Config.ptt.preDelay                     = request->getParam("ptt.preDelay", true)->value().toInt();
        Config.ptt.postDelay                    = request->getParam("ptt.postDelay", true)->value().toInt();
        Config.ptt.reverse                      = request->hasParam("ptt.reverse", true);

        //  WiFi AP
        Config.wifiAP.password                  = request->getParam("wifiAP.password", true)->value();
        Config.wifiAP.active                    = false; // when Configuration is finished Tracker returns to normal mode.        

        Config.writeFile();

        AsyncWebServerResponse *response        = request->beginResponse(302, "text/html", "");
        response->addHeader("Location", "/");
        request->send(response);
        displayToggle(false);
        delay(500);
        ESP.restart();
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

        server.begin();
    }

}