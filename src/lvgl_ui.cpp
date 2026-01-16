/* LVGL UI for T-Deck Plus
 * Touchscreen-based user interface using LVGL library
 */

#ifdef USE_LVGL_UI

#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <TinyGPS++.h>
#include <APRSPacketLib.h>
#include <WiFi.h>
#define TOUCH_MODULES_GT911
#include <TouchLib.h>
#include <Wire.h>
#include "lvgl_ui.h"
#include "board_pinout.h"
#include "configuration.h"
#include "battery_utils.h"

// External data sources
extern Configuration Config;
extern int myBeaconsIndex;
extern TinyGPSPlus gps;
extern bool WiFiConnected;
extern String batteryVoltage;
extern APRSPacket lastReceivedPacket;
extern bool sendUpdate;  // Set to true to trigger beacon transmission

// Display dimensions
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// LVGL buffer size (use partial buffer to save memory, full buffer in PSRAM)
#define LVGL_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT)

// External TFT instance from display.cpp
extern TFT_eSPI tft;

// External touch module address (found by I2C scan in utils.cpp)
extern uint8_t touchModuleAddress;

// Touch controller - static instance, initialized in setup()
static TouchLib touch(Wire, BOARD_I2C_SDA, BOARD_I2C_SCL, 0x00);
static bool touchInitialized = false;

// Touch calibration (same as touch_utils.cpp)
static const int16_t xCalibratedMin = 5;
static const int16_t xCalibratedMax = 314;
static const int16_t yCalibratedMin = 6;
static const int16_t yCalibratedMax = 233;

// LVGL display buffer (in PSRAM)
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* buf1 = nullptr;
static lv_color_t* buf2 = nullptr;

// LVGL display and input drivers
static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;

// UI Elements
static lv_obj_t* screen_main = nullptr;
static lv_obj_t* label_callsign = nullptr;
static lv_obj_t* label_gps = nullptr;
static lv_obj_t* label_battery = nullptr;
static lv_obj_t* label_lora = nullptr;
static lv_obj_t* label_wifi = nullptr;
static lv_obj_t* label_time = nullptr;

// LVGL tick tracking
static uint32_t last_tick = 0;

// Display flush callback
static bool first_flush = true;
static void disp_flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    if (first_flush) {
        Serial.printf("[LVGL] First flush: %dx%d at (%d,%d)\n", w, h, area->x1, area->y1);
        first_flush = false;
    }

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t*)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(drv);
}

// Touch read callback
static uint32_t lastTouchDebug = 0;
static void touch_read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    if (touchInitialized && touch.read()) {
        TP_Point t = touch.getPoint(0);
        // X and Y are swapped and Y is inverted because TFT screen is rotated
        uint16_t x = map(t.y, xCalibratedMin, xCalibratedMax, 0, SCREEN_WIDTH);
        uint16_t y = SCREEN_HEIGHT - map(t.x, yCalibratedMin, yCalibratedMax, 0, SCREEN_HEIGHT);
        data->state = LV_INDEV_STATE_PR;
        data->point.x = x;
        data->point.y = y;
        // Debug: print touch coordinates
        if (millis() - lastTouchDebug > 500) {
            Serial.printf("[LVGL Touch] x=%d y=%d (raw: %d,%d)\n", x, y, t.x, t.y);
            lastTouchDebug = millis();
        }
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

// Button event callbacks
static void btn_beacon_clicked(lv_event_t* e) {
    sendUpdate = true;
    Serial.println("[LVGL] BEACON button pressed - sending beacon");
}

// Create the main dashboard screen
static void create_dashboard() {
    // Create main screen
    screen_main = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_main, lv_color_hex(0x1a1a2e), 0);

    // Status bar at top
    lv_obj_t* status_bar = lv_obj_create(screen_main);
    lv_obj_set_size(status_bar, SCREEN_WIDTH, 30);
    lv_obj_set_pos(status_bar, 0, 0);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_radius(status_bar, 0, 0);
    lv_obj_set_style_pad_all(status_bar, 5, 0);
    lv_obj_set_flex_flow(status_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Callsign label (left)
    label_callsign = lv_label_create(status_bar);
    lv_label_set_text(label_callsign, "NOCALL");
    lv_obj_set_style_text_color(label_callsign, lv_color_hex(0x00ff88), 0);
    lv_obj_set_style_text_font(label_callsign, &lv_font_montserrat_14, 0);

    // Time label (center)
    label_time = lv_label_create(status_bar);
    lv_label_set_text(label_time, "--:--:--");
    lv_obj_set_style_text_color(label_time, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_14, 0);

    // Battery label (right)
    label_battery = lv_label_create(status_bar);
    lv_label_set_text(label_battery, "-- %");
    lv_obj_set_style_text_color(label_battery, lv_color_hex(0xffd700), 0);
    lv_obj_set_style_text_font(label_battery, &lv_font_montserrat_14, 0);

    // Main content area
    lv_obj_t* content = lv_obj_create(screen_main);
    lv_obj_set_size(content, SCREEN_WIDTH - 10, SCREEN_HEIGHT - 80);
    lv_obj_set_pos(content, 5, 35);
    lv_obj_set_style_bg_color(content, lv_color_hex(0x0f0f23), 0);
    lv_obj_set_style_border_color(content, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_radius(content, 8, 0);
    lv_obj_set_style_pad_all(content, 10, 0);

    // GPS info
    label_gps = lv_label_create(content);
    lv_label_set_text(label_gps, "GPS: Acquiring...\nLat: ---.----\nLon: ---.----\nAlt: ----m  Spd: ---km/h");
    lv_obj_set_style_text_color(label_gps, lv_color_hex(0x00d4ff), 0);
    lv_obj_set_style_text_font(label_gps, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(label_gps, 0, 0);

    // LoRa info
    label_lora = lv_label_create(content);
    lv_label_set_text(label_lora, "LoRa: Ready\nLast RX: ---");
    lv_obj_set_style_text_color(label_lora, lv_color_hex(0xff6b6b), 0);
    lv_obj_set_style_text_font(label_lora, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(label_lora, 0, 80);

    // WiFi info
    label_wifi = lv_label_create(content);
    lv_label_set_text(label_wifi, "WiFi: Disconnected");
    lv_obj_set_style_text_color(label_wifi, lv_color_hex(0xc792ea), 0);
    lv_obj_set_style_text_font(label_wifi, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(label_wifi, 0, 120);

    // Bottom button bar
    lv_obj_t* btn_bar = lv_obj_create(screen_main);
    lv_obj_set_size(btn_bar, SCREEN_WIDTH, 40);
    lv_obj_set_pos(btn_bar, 0, SCREEN_HEIGHT - 40);
    lv_obj_set_style_bg_color(btn_bar, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_border_width(btn_bar, 0, 0);
    lv_obj_set_style_radius(btn_bar, 0, 0);
    lv_obj_set_style_pad_all(btn_bar, 5, 0);
    lv_obj_set_flex_flow(btn_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Beacon button
    lv_obj_t* btn_beacon = lv_btn_create(btn_bar);
    lv_obj_set_size(btn_beacon, 90, 30);
    lv_obj_set_style_bg_color(btn_beacon, lv_color_hex(0x00ff88), 0);
    lv_obj_add_event_cb(btn_beacon, btn_beacon_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_beacon = lv_label_create(btn_beacon);
    lv_label_set_text(lbl_beacon, "BEACON");
    lv_obj_center(lbl_beacon);
    lv_obj_set_style_text_color(lbl_beacon, lv_color_hex(0x000000), 0);

    // Messages button
    lv_obj_t* btn_msg = lv_btn_create(btn_bar);
    lv_obj_set_size(btn_msg, 90, 30);
    lv_obj_set_style_bg_color(btn_msg, lv_color_hex(0x00d4ff), 0);
    lv_obj_t* lbl_msg = lv_label_create(btn_msg);
    lv_label_set_text(lbl_msg, "MSG");
    lv_obj_center(lbl_msg);
    lv_obj_set_style_text_color(lbl_msg, lv_color_hex(0x000000), 0);

    // Settings button
    lv_obj_t* btn_settings = lv_btn_create(btn_bar);
    lv_obj_set_size(btn_settings, 90, 30);
    lv_obj_set_style_bg_color(btn_settings, lv_color_hex(0xc792ea), 0);
    lv_obj_t* lbl_settings = lv_label_create(btn_settings);
    lv_label_set_text(lbl_settings, "SETUP");
    lv_obj_center(lbl_settings);
    lv_obj_set_style_text_color(lbl_settings, lv_color_hex(0x000000), 0);

    // Load the screen
    lv_scr_load(screen_main);
}

namespace LVGL_UI {

    void setup() {
        Serial.println("[LVGL] Initializing...");

        // Ensure backlight is on
        #ifdef BOARD_BL_PIN
            pinMode(BOARD_BL_PIN, OUTPUT);
            digitalWrite(BOARD_BL_PIN, HIGH);
        #endif

        // Re-init TFT for LVGL
        tft.init();
        tft.setRotation(1);  // Landscape, keyboard at bottom

        // Initialize LVGL
        lv_init();

        // Initialize tick counter
        last_tick = millis();

        // Allocate display buffers in PSRAM
        #ifdef BOARD_HAS_PSRAM
            buf1 = (lv_color_t*)ps_malloc(LVGL_BUF_SIZE * sizeof(lv_color_t));
            buf2 = (lv_color_t*)ps_malloc(LVGL_BUF_SIZE * sizeof(lv_color_t));
            Serial.println("[LVGL] Using PSRAM for display buffers");
        #else
            buf1 = (lv_color_t*)malloc(LVGL_BUF_SIZE * sizeof(lv_color_t));
            buf2 = nullptr;
            Serial.println("[LVGL] Using RAM for display buffer");
        #endif

        if (!buf1) {
            Serial.println("[LVGL] ERROR: Failed to allocate display buffer!");
            return;
        }

        // Initialize display buffer
        lv_disp_draw_buf_init(&draw_buf, buf1, buf2, LVGL_BUF_SIZE);

        // Initialize display driver
        lv_disp_drv_init(&disp_drv);
        disp_drv.hor_res = SCREEN_WIDTH;
        disp_drv.ver_res = SCREEN_HEIGHT;
        disp_drv.flush_cb = disp_flush_cb;
        disp_drv.draw_buf = &draw_buf;
        disp_drv.full_refresh = (buf2 != nullptr) ? 1 : 0;  // Full refresh if double buffered
        lv_disp_drv_register(&disp_drv);

        // Initialize touch input
        if (touchModuleAddress != 0x00) {
            Serial.printf("[LVGL] Touch module found at 0x%02X\n", touchModuleAddress);
            if (touchModuleAddress == 0x14) {
                touch = TouchLib(Wire, BOARD_I2C_SDA, BOARD_I2C_SCL, GT911_SLAVE_ADDRESS2);
            } else if (touchModuleAddress == 0x5D) {
                touch = TouchLib(Wire, BOARD_I2C_SDA, BOARD_I2C_SCL, GT911_SLAVE_ADDRESS1);
            }
            touch.init();
            touchInitialized = true;

            // Register LVGL input device
            lv_indev_drv_init(&indev_drv);
            indev_drv.type = LV_INDEV_TYPE_POINTER;
            indev_drv.read_cb = touch_read_cb;
            lv_indev_drv_register(&indev_drv);
            Serial.println("[LVGL] Touch input registered");
        } else {
            Serial.println("[LVGL] No touch module detected");
        }

        // Create the UI
        create_dashboard();

        // Force initial refresh
        lv_obj_invalidate(lv_scr_act());
        lv_refr_now(NULL);
        Serial.println("[LVGL] Forced initial refresh");

        Serial.println("[LVGL] UI Ready");
    }

    static uint32_t last_data_update = 0;
    static String last_callsign = "";

    void loop() {
        // Update LVGL tick
        uint32_t now = millis();
        uint32_t elapsed = now - last_tick;
        lv_tick_inc(elapsed);
        last_tick = now;

        // Handle LVGL tasks
        lv_timer_handler();

        // Update data every second
        if (now - last_data_update >= 1000) {
            last_data_update = now;

            // Update callsign if changed
            Beacon* currentBeacon = &Config.beacons[myBeaconsIndex];
            if (currentBeacon->callsign != last_callsign) {
                last_callsign = currentBeacon->callsign;
                updateCallsign(last_callsign.c_str());
            }

            // Update GPS data
            if (gps.location.isValid()) {
                updateGPS(
                    gps.location.lat(),
                    gps.location.lng(),
                    gps.altitude.meters(),
                    gps.speed.kmph(),
                    gps.satellites.value()
                );
            }

            // Update time from GPS
            if (gps.time.isValid()) {
                updateTime(gps.time.hour(), gps.time.minute(), gps.time.second());
            }

            // Update battery
            if (batteryVoltage.length() > 0) {
                float voltage = batteryVoltage.toFloat();
                int percent = (int)(((voltage - 3.0) / (4.2 - 3.0)) * 100);
                if (percent > 100) percent = 100;
                if (percent < 0) percent = 0;
                updateBattery(percent, voltage);
            }

            // Update WiFi status
            updateWiFi(WiFiConnected, WiFiConnected ? WiFi.RSSI() : 0);

            // Update LoRa (last received packet)
            if (lastReceivedPacket.sender.length() > 0) {
                updateLoRa(lastReceivedPacket.sender.c_str(), lastReceivedPacket.rssi);
            }
        }
    }

    void updateGPS(double lat, double lng, double alt, double speed, int sats) {
        if (label_gps) {
            char buf[128];
            snprintf(buf, sizeof(buf),
                "GPS: %d sats\nLat: %.4f\nLon: %.4f\nAlt: %.0fm  Spd: %.0fkm/h",
                sats, lat, lng, alt, speed);
            lv_label_set_text(label_gps, buf);
        }
    }

    void updateBattery(int percent, float voltage) {
        if (label_battery) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d%%", percent);
            lv_label_set_text(label_battery, buf);

            // Change color based on level
            if (percent > 50) {
                lv_obj_set_style_text_color(label_battery, lv_color_hex(0x00ff88), 0);
            } else if (percent > 20) {
                lv_obj_set_style_text_color(label_battery, lv_color_hex(0xffd700), 0);
            } else {
                lv_obj_set_style_text_color(label_battery, lv_color_hex(0xff6b6b), 0);
            }
        }
    }

    void updateLoRa(const char* lastRx, int rssi) {
        if (label_lora) {
            char buf[64];
            snprintf(buf, sizeof(buf), "LoRa: Ready\nLast RX: %s (%ddBm)", lastRx, rssi);
            lv_label_set_text(label_lora, buf);
        }
    }

    void updateWiFi(bool connected, int rssi) {
        if (label_wifi) {
            if (connected) {
                char buf[32];
                snprintf(buf, sizeof(buf), "WiFi: Connected (%ddBm)", rssi);
                lv_label_set_text(label_wifi, buf);
                lv_obj_set_style_text_color(label_wifi, lv_color_hex(0x00ff88), 0);
            } else {
                lv_label_set_text(label_wifi, "WiFi: Disconnected");
                lv_obj_set_style_text_color(label_wifi, lv_color_hex(0xc792ea), 0);
            }
        }
    }

    void showMessage(const char* from, const char* message) {
        // TODO: Implement message popup
    }

    void updateCallsign(const char* callsign) {
        if (label_callsign) {
            lv_label_set_text(label_callsign, callsign);
        }
    }

    void updateTime(int hour, int minute, int second) {
        if (label_time) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hour, minute, second);
            lv_label_set_text(label_time, buf);
        }
    }

    // TX msgbox
    static lv_obj_t* tx_msgbox = nullptr;
    static lv_timer_t* tx_popup_timer = nullptr;

    static void hide_tx_popup(lv_timer_t* timer) {
        if (tx_msgbox) {
            lv_msgbox_close(tx_msgbox);
            tx_msgbox = nullptr;
        }
        tx_popup_timer = nullptr;
    }

    void showTxPacket(const char* packet) {
        Serial.printf("[LVGL] showTxPacket called: %s\n", packet);

        // Close existing msgbox if any
        if (tx_msgbox) {
            lv_msgbox_close(tx_msgbox);
            tx_msgbox = nullptr;
        }
        if (tx_popup_timer) {
            lv_timer_del(tx_popup_timer);
            tx_popup_timer = nullptr;
        }

        // Create message box on active screen
        tx_msgbox = lv_msgbox_create(lv_scr_act(), "<<< TX >>>", packet, NULL, false);
        lv_obj_set_size(tx_msgbox, 280, 120);
        lv_obj_set_style_bg_color(tx_msgbox, lv_color_hex(0x002200), 0);
        lv_obj_set_style_bg_opa(tx_msgbox, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(tx_msgbox, lv_color_hex(0x00ff88), 0);
        lv_obj_set_style_border_width(tx_msgbox, 3, 0);
        lv_obj_set_style_text_color(tx_msgbox, lv_color_hex(0x00ff88), 0);
        lv_obj_center(tx_msgbox);

        // Force immediate refresh
        lv_refr_now(NULL);

        // Auto-close after 3 seconds
        tx_popup_timer = lv_timer_create(hide_tx_popup, 3000, NULL);
        lv_timer_set_repeat_count(tx_popup_timer, 1);

        Serial.println("[LVGL] TX msgbox created and refreshed");
    }

}

#endif // USE_LVGL_UI
