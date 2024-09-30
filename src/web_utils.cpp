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

            if (request->hasParam("beacons." + String(i) + ".gpsEcoMode", true)) {
                Config.beacons[i].gpsEcoMode = true; // Checkbox is checked
            } else {
                Config.beacons[i].gpsEcoMode = false; // Checkbox is unchecked
            }

            // Handle smartBeaconActive
            if (request->hasParam("beacons." + String(i) + ".smartBeaconActive", true)) {
                Config.beacons[i].smartBeaconActive = true; // Checkbox is checked
            } else {
                Config.beacons[i].smartBeaconActive = false; // Checkbox is unchecked
            }

            /*if (request->hasParam("beacons." + String(i) + ".gpsEcoMode", true)) {
                Serial.println("Beacon " + String(i) + ": GPS Eco Mode is checked.");
            } else {
                Serial.println("Beacon " + String(i) + ": GPS Eco Mode is unchecked.");
            }*/
            //Config.beacons[i].gpsEcoMode            = request->hasParam("beacons." + String(i) + ".gpsEcoMode", true);
            //Config.beacons[i].smartBeaconActive     = request->hasParam("beacons." + String(i) + ".smartBeaconActive", true);
            Config.beacons[i].smartBeaconSetting    = request->getParam("beacons." + String(i) + ".smartBeaconSetting", true)->value().toInt();
        }
        

        /*Config.beacons[0].callsign              = request->getParam("beacons.0.callsign", true)->value();
        Config.beacons[0].symbol                = request->getParam("beacons.0.symbol", true)->value();
        Config.beacons[0].overlay               = request->getParam("beacons.0.overlay", true)->value();
        Config.beacons[0].micE                  = request->getParam("beacons.0.micE", true)->value();
        Config.beacons[0].comment               = request->getParam("beacons.0.comment", true)->value();
        Config.beacons[0].gpsEcoMode            = request->hasParam("beacons.0.gpsEcoMode", true);
        Config.beacons[0].smartBeaconActive     = request->hasParam("beacons.0.smartBeaconActive", true);
        Config.beacons[0].smartBeaconSetting    = request->getParam("beacons.0.smartBeaconSetting", true)->value().toInt();

        Config.beacons[1].callsign              = request->getParam("beacons.1.callsign", true)->value();
        Config.beacons[1].symbol                = request->getParam("beacons.1.symbol", true)->value();
        Config.beacons[1].overlay               = request->getParam("beacons.1.overlay", true)->value();
        Config.beacons[1].micE                  = request->getParam("beacons.1.micE", true)->value();
        Config.beacons[1].comment               = request->getParam("beacons.1.comment", true)->value();
        Config.beacons[1].gpsEcoMode            = request->hasParam("beacons.1.gpsEcoMode", true);
        Config.beacons[1].smartBeaconActive     = request->hasParam("beacons.1.smartBeaconActive", true);
        Config.beacons[1].smartBeaconSetting    = request->getParam("beacons.1.smartBeaconSetting", true)->value().toInt();

        Config.beacons[2].callsign              = request->getParam("beacons.2.callsign", true)->value();
        Config.beacons[2].symbol                = request->getParam("beacons.2.symbol", true)->value();
        Config.beacons[2].overlay               = request->getParam("beacons.2.overlay", true)->value();
        Config.beacons[2].micE                  = request->getParam("beacons.2.micE", true)->value();
        Config.beacons[2].comment               = request->getParam("beacons.2.comment", true)->value();
        Config.beacons[2].gpsEcoMode            = request->hasParam("beacons.2.gpsEcoMode", true);
        Config.beacons[2].smartBeaconActive     = request->hasParam("beacons.2.smartBeaconActive", true);
        Config.beacons[2].smartBeaconSetting    = request->getParam("beacons.2.smartBeaconSetting", true)->value().toInt();*/
        
        //  Station Config
        Config.simplifiedTrackerMode            = request->hasParam("simplifiedTrackerMode", true);
        Config.sendCommentAfterXBeacons         = request->getParam("sendCommentAfterXBeacons", true)->value().toInt();
        Config.path                             = request->getParam("path", true)->value();
        Config.nonSmartBeaconRate               = request->getParam("nonSmartBeaconRate", true)->value().toInt();
        Config.rememberStationTime              = request->getParam("rememberStationTime", true)->value().toInt();
        Config.standingUpdateTime               = request->getParam("standingUpdateTime", true)->value().toInt();
        Config.sendAltitude                     = request->hasParam("sendAltitude", true);
        Config.disableGPS                       = request->hasParam("disableGPS", true);

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

        //  Winlink
        Config.winlink.password                 = request->getParam("winlink.password", true)->value();
        
        //  Wx Telemtry
        Config.bme.active                       = request->hasParam("bme.active", true);
        Config.bme.temperatureCorrection        = request->getParam("bme.temperatureCorrection", true)->value().toFloat();
        Config.bme.sendTelemetry                = request->hasParam("bme.sendTelemetry", true);

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
        Config.loraTypes[0].frequency           = request->getParam("lora.0.frequency", true)->value().toDouble();
        Config.loraTypes[0].spreadingFactor     = request->getParam("lora.0.spreadingFactor", true)->value().toInt();
        Config.loraTypes[0].codingRate4         = request->getParam("lora.0.codingRate4", true)->value().toInt();

        Config.loraTypes[1].frequency           = request->getParam("lora.1.frequency", true)->value().toDouble();
        Config.loraTypes[1].spreadingFactor     = request->getParam("lora.1.spreadingFactor", true)->value().toInt();
        Config.loraTypes[1].codingRate4         = request->getParam("lora.1.codingRate4", true)->value().toInt();

        Config.loraTypes[2].frequency           = request->getParam("lora.2.frequency", true)->value().toDouble();
        Config.loraTypes[2].spreadingFactor     = request->getParam("lora.2.spreadingFactor", true)->value().toInt();
        Config.loraTypes[2].codingRate4         = request->getParam("lora.2.codingRate4", true)->value().toInt();

        //  Bluetooth
        Config.bluetooth.active                 = request->hasParam("bluetooth.active", true);
        Config.bluetooth.type                   = request->getParam("bluetooth.type", true)->value().toInt();

        //  PTT Trigger
        Config.ptt.active                       = request->hasParam("ptt.active", true);
        Config.ptt.io_pin                       = request->getParam("ptt.io_pin", true)->value().toInt();
        Config.ptt.preDelay                     = request->getParam("ptt.preDelay", true)->value().toInt();
        Config.ptt.postDelay                    = request->getParam("ptt.postDelay", true)->value().toInt();
        Config.ptt.reverse                      = request->hasParam("ptt.reverse", true);

        //  WiFi AP
        Config.wifiAP.password                  = request->getParam("wifiAP.password", true)->value();
        //Config.wifiAP.active                    = false; // when Configuration is finished Tracker returns to normal mode.        

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