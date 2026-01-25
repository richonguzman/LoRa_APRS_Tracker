/* LVGL UI Dashboard Module
 * Main dashboard screen with status bar, content area, and button bar
 */

#ifdef USE_LVGL_UI

#include "ui_dashboard.h"
#include "ui_common.h"
#include "ui_settings.h"
#include "ui_popups.h"
#include "ui_map_manager.h"
#include "lvgl_ui.h"

#include <Arduino.h>
#include <WiFi.h>
#include <lvgl.h>

#include "battery_utils.h"
#include "ble_utils.h"
#include "configuration.h"
#include "custom_characters.h"
#include "storage_utils.h"
#include "utils.h"
#include <TimeLib.h>
#include <algorithm>
#include <vector>

// External configuration and state
extern Configuration Config;
extern uint8_t myBeaconsIndex;
extern int myBeaconsSize;
extern bool WiFiConnected;
extern bool WiFiEcoMode;
extern bool WiFiUserDisabled;
extern bool bluetoothActive;
extern bool bluetoothConnected;
extern bool sendUpdate;
extern uint8_t loraIndex;
extern int loraIndexSize;

// APRS symbols (defined in lvgl_ui.cpp)
extern const char *symbolArray[];
extern const int symbolArraySize;
extern const uint8_t *symbolsAPRS[];

// Screen dimensions
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// APRS symbol canvas dimensions
#define APRS_CANVAS_WIDTH SYMBOL_WIDTH
#define APRS_CANVAS_HEIGHT SYMBOL_HEIGHT

namespace UIDashboard {

// Dashboard screen and labels
static lv_obj_t *screen_main = nullptr;
static lv_obj_t *label_callsign = nullptr;
static lv_obj_t *label_gps = nullptr;
static lv_obj_t *label_lora = nullptr;
static lv_obj_t *label_time = nullptr;
static lv_obj_t *aprs_symbol_canvas = nullptr;
static lv_color_t *aprs_symbol_buf = nullptr;

// Last RX stations
static lv_obj_t *label_last_rx = nullptr;

// Status bar icons
static lv_obj_t *icon_wifi = nullptr;
static lv_obj_t *icon_bluetooth = nullptr;
static lv_obj_t *icon_battery = nullptr;
static lv_obj_t *label_battery_pct = nullptr;

// Forward declarations for button callbacks
static void btn_beacon_clicked(lv_event_t *e);
static void btn_setup_clicked(lv_event_t *e);
static void btn_msg_clicked(lv_event_t *e);
static void btn_map_clicked(lv_event_t *e);

void init() {
    // Initialize dashboard module (nothing to do yet)
}

lv_obj_t* getMainScreen() {
    return screen_main;
}

void drawAPRSSymbol(const char *symbolStr) {
    if (!aprs_symbol_canvas || !aprs_symbol_buf)
        return;

    // Extract symbol character from full format (e.g., "/>" or "\>" or ">")
    // Symbol is always second char in 2-char format, first char in 1-char format
    char symbolChar[2] = {0, 0};
    if (symbolStr && strlen(symbolStr) >= 2) {
        symbolChar[0] = symbolStr[1]; // Second character is the symbol
    } else if (symbolStr && strlen(symbolStr) >= 1) {
        symbolChar[0] = symbolStr[0];
    }

    // Find symbol index
    int symbolIndex = -1;
    for (int i = 0; i < symbolArraySize; i++) {
        if (strcmp(symbolChar, symbolArray[i]) == 0) {
            symbolIndex = i;
            break;
        }
    }

    // Clear canvas with dark background
    lv_canvas_fill_bg(aprs_symbol_canvas, lv_color_hex(0x16213e), LV_OPA_COVER);

    if (symbolIndex < 0)
        return; // Symbol not found

    const uint8_t *bitMap = symbolsAPRS[symbolIndex];
    lv_color_t white = lv_color_hex(0xffffff);

    // Draw bitmap 1:1
    for (int y = 0; y < SYMBOL_HEIGHT; y++) {
        for (int x = 0; x < SYMBOL_WIDTH; x++) {
            int byteIndex = (y * ((SYMBOL_WIDTH + 7) / 8)) + (x / 8);
            int bitIndex = 7 - (x % 8);
            if (bitMap[byteIndex] & (1 << bitIndex)) {
                lv_canvas_set_px_color(aprs_symbol_canvas, x, y, white);
            }
        }
    }
    lv_obj_invalidate(aprs_symbol_canvas);
}

// Button event callbacks
static void btn_beacon_clicked(lv_event_t *e) {
    sendUpdate = true;
    Serial.println("[LVGL] BEACON button pressed - requesting beacon");
    UIPopups::showBeaconPending();
}

static void btn_setup_clicked(lv_event_t *e) {
    Serial.println("[LVGL] SETUP button pressed");
    UIPopups::closeAll();
    UISettings::openSetup();
}

static void btn_msg_clicked(lv_event_t *e) {
    Serial.println("[LVGL] MSG button pressed");
    UIPopups::closeAll();
    LVGL_UI::openMessagesScreen();
}

static void btn_map_clicked(lv_event_t *e) {
    Serial.println("[LVGL] MAP button pressed");
    Serial.printf("[LVGL-DEBUG] Free heap before MAP: %u bytes\n", ESP.getFreeHeap());
    UIPopups::closeAll();
    Serial.println("[LVGL-DEBUG] Popups closed");

    // Recreate map screen each time to update positions
    if (UIMapManager::screen_map) {
        Serial.println("[LVGL-DEBUG] Deleting old screen_map");
        lv_obj_del(UIMapManager::screen_map);
        UIMapManager::screen_map = nullptr;
    }
    Serial.println("[LVGL-DEBUG] Creating new map screen");
    UIMapManager::create_map_screen();
    Serial.println("[LVGL-DEBUG] Map screen created, loading animation");
    lv_scr_load_anim(UIMapManager::screen_map, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
    Serial.println("[LVGL-DEBUG] btn_map_clicked DONE");
}

// Public button action functions
void onBeaconClicked() { btn_beacon_clicked(nullptr); }
void onMsgClicked() { btn_msg_clicked(nullptr); }
void onMapClicked() { btn_map_clicked(nullptr); }
void onSetupClicked() { btn_setup_clicked(nullptr); }

void createDashboard() {
    // Create main screen
    screen_main = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_main, lv_color_hex(0x1a1a2e), 0);

    // Status bar at top
    lv_obj_t *status_bar = lv_obj_create(screen_main);
    lv_obj_set_size(status_bar, SCREEN_WIDTH, 30);
    lv_obj_set_pos(status_bar, 0, 0);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_radius(status_bar, 0, 0);
    lv_obj_set_style_pad_all(status_bar, 5, 0);
    lv_obj_set_flex_flow(status_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_bar, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Callsign label (left)
    label_callsign = lv_label_create(status_bar);
    lv_label_set_text(label_callsign, "NOCALL");
    lv_obj_set_style_text_color(label_callsign, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(label_callsign, &lv_font_montserrat_14, 0);

    // APRS symbol canvas (center)
    aprs_symbol_buf = (lv_color_t *)malloc(
        APRS_CANVAS_WIDTH * APRS_CANVAS_HEIGHT * sizeof(lv_color_t));
    if (aprs_symbol_buf) {
        aprs_symbol_canvas = lv_canvas_create(status_bar);
        lv_canvas_set_buffer(aprs_symbol_canvas, aprs_symbol_buf, APRS_CANVAS_WIDTH,
                             APRS_CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);
        lv_obj_set_size(aprs_symbol_canvas, APRS_CANVAS_WIDTH, APRS_CANVAS_HEIGHT);
        // Draw initial symbol from current beacon
        Beacon *currentBeacon = &Config.beacons[myBeaconsIndex];
        String fullSymbol = currentBeacon->overlay + currentBeacon->symbol;
        drawAPRSSymbol(fullSymbol.c_str());
    }

    // Date/Time label
    label_time = lv_label_create(status_bar);
    lv_label_set_text(label_time, "--/-- --:--");
    lv_obj_set_style_text_color(label_time, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_14, 0);

    // WiFi icon (hidden by default, shown when connected)
    icon_wifi = lv_label_create(status_bar);
    lv_label_set_text(icon_wifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(icon_wifi, lv_color_hex(0x00ff00), 0);
    lv_obj_add_flag(icon_wifi, LV_OBJ_FLAG_HIDDEN);

    // Bluetooth icon (hidden by default, shown when connected)
    icon_bluetooth = lv_label_create(status_bar);
    lv_label_set_text(icon_bluetooth, LV_SYMBOL_BLUETOOTH);
    lv_obj_set_style_text_color(icon_bluetooth, lv_color_hex(0x00ff00), 0);
    lv_obj_add_flag(icon_bluetooth, LV_OBJ_FLAG_HIDDEN);

    // Battery icon + percentage
    icon_battery = lv_label_create(status_bar);
    lv_label_set_text(icon_battery, LV_SYMBOL_BATTERY_FULL);
    lv_obj_set_style_text_color(icon_battery, lv_color_hex(0x00ff00), 0);

    label_battery_pct = lv_label_create(status_bar);
    lv_label_set_text(label_battery_pct, "--%");
    lv_obj_set_style_text_color(label_battery_pct, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(label_battery_pct, &lv_font_montserrat_12, 0);

    // Main content area
    lv_obj_t *content = lv_obj_create(screen_main);
    lv_obj_set_size(content, SCREEN_WIDTH - 10, SCREEN_HEIGHT - 80);
    lv_obj_set_pos(content, 5, 35);
    lv_obj_set_style_bg_color(content, lv_color_hex(0x0f0f23), 0);
    lv_obj_set_style_border_color(content, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_radius(content, 8, 0);
    lv_obj_set_style_pad_all(content, 10, 0);

    // GPS info
    label_gps = lv_label_create(content);
    lv_label_set_text(label_gps, "GPS: -- sat  Loc: --------\nLat: --.----  Lon: "
                                 "--.----\nAlt: ---- m  Spd: --- km/h");
    lv_obj_set_style_text_color(label_gps, lv_color_hex(0x759a9e), 0);
    lv_obj_set_style_text_font(label_gps, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(label_gps, 0, 0);

    // LoRa info
    label_lora = lv_label_create(content);
    char lora_init[64];
    float freq = Config.loraTypes[loraIndex].frequency / 1000000.0;
    int rate = Config.loraTypes[loraIndex].dataRate;
    snprintf(lora_init, sizeof(lora_init), "LoRa: %.3f MHz  %d bps", freq, rate);
    lv_label_set_text(label_lora, lora_init);
    lv_obj_set_style_text_color(label_lora, lv_color_hex(0xff6b6b), 0);
    lv_obj_set_style_text_font(label_lora, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(label_lora, 0, 55);

    // Last RX stations (4 max)
    label_last_rx = lv_label_create(content);
    lv_label_set_text(label_last_rx, "Last RX:\n---");
    lv_obj_set_style_text_color(label_last_rx, lv_color_hex(0xffcc00), 0);
    lv_obj_set_style_text_font(label_last_rx, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(label_last_rx, 0, 80);

    // Bottom button bar
    lv_obj_t *btn_bar = lv_obj_create(screen_main);
    lv_obj_set_size(btn_bar, SCREEN_WIDTH, 40);
    lv_obj_set_pos(btn_bar, 0, SCREEN_HEIGHT - 40);
    lv_obj_set_style_bg_color(btn_bar, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_border_width(btn_bar, 0, 0);
    lv_obj_set_style_radius(btn_bar, 0, 0);
    lv_obj_set_style_pad_all(btn_bar, 5, 0);
    lv_obj_set_flex_flow(btn_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_bar, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Beacon button (APRS red)
    lv_obj_t *btn_beacon = lv_btn_create(btn_bar);
    lv_obj_set_size(btn_beacon, 70, 30);
    lv_obj_set_style_bg_color(btn_beacon, lv_color_hex(0xcc0000), 0);
    lv_obj_add_event_cb(btn_beacon, btn_beacon_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_beacon = lv_label_create(btn_beacon);
    lv_label_set_text(lbl_beacon, "BCN");
    lv_obj_center(lbl_beacon);
    lv_obj_set_style_text_color(lbl_beacon, lv_color_hex(0xffffff), 0);

    // Messages button (APRS blue)
    lv_obj_t *btn_msg = lv_btn_create(btn_bar);
    lv_obj_set_size(btn_msg, 70, 30);
    lv_obj_set_style_bg_color(btn_msg, lv_color_hex(0x0066cc), 0);
    lv_obj_add_event_cb(btn_msg, btn_msg_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_msg = lv_label_create(btn_msg);
    lv_label_set_text(lbl_msg, "MSG");
    lv_obj_center(lbl_msg);
    lv_obj_set_style_text_color(lbl_msg, lv_color_hex(0xffffff), 0);

    // Map button (green)
    lv_obj_t *btn_map = lv_btn_create(btn_bar);
    lv_obj_set_size(btn_map, 70, 30);
    lv_obj_set_style_bg_color(btn_map, lv_color_hex(0x009933), 0);
    lv_obj_add_event_cb(btn_map, btn_map_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_map = lv_label_create(btn_map);
    lv_label_set_text(lbl_map, "MAP");
    lv_obj_center(lbl_map);
    lv_obj_set_style_text_color(lbl_map, lv_color_hex(0xffffff), 0);

    // Settings button
    lv_obj_t *btn_settings = lv_btn_create(btn_bar);
    lv_obj_set_size(btn_settings, 70, 30);
    lv_obj_set_style_bg_color(btn_settings, lv_color_hex(0xc792ea), 0);
    lv_obj_add_event_cb(btn_settings, btn_setup_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_settings = lv_label_create(btn_settings);
    lv_label_set_text(lbl_settings, "SET");
    lv_obj_center(lbl_settings);
    lv_obj_set_style_text_color(lbl_settings, lv_color_hex(0x000000), 0);

    // Load the screen
    lv_scr_load(screen_main);
}

// Update functions
void updateGPS(double lat, double lng, double alt, double speed, int sats, double hdop) {
    if (label_gps) {
        char buf[128];
        const char *locator = Utils::getMaidenheadLocator(lat, lng, 8);

        // Determine HDOP quality indicator
        const char *hdopState = "";
        if (hdop > 5.0) {
            hdopState = "X"; // Bad precision
        } else if (hdop > 2.0 && hdop < 5.0) {
            hdopState = "-"; // Medium precision
        } else if (hdop <= 2.0) {
            hdopState = "+"; // Good precision
        }

        snprintf(buf, sizeof(buf),
                 "GPS: %d%s sat  Loc: %s\nLat: %.4f  Lon: %.4f\nAlt: %.0f m  "
                 "Spd: %.0f km/h",
                 sats, hdopState, locator, lat, lng, alt, speed);
        lv_label_set_text(label_gps, buf);
    }
}

void updateBattery(int percent, float voltage) {
    // Update battery icon
    if (icon_battery) {
        // Select icon based on level
        if (percent > 85) {
            lv_label_set_text(icon_battery, LV_SYMBOL_BATTERY_FULL);
        } else if (percent > 60) {
            lv_label_set_text(icon_battery, LV_SYMBOL_BATTERY_3);
        } else if (percent > 35) {
            lv_label_set_text(icon_battery, LV_SYMBOL_BATTERY_2);
        } else if (percent > 10) {
            lv_label_set_text(icon_battery, LV_SYMBOL_BATTERY_1);
        } else {
            lv_label_set_text(icon_battery, LV_SYMBOL_BATTERY_EMPTY);
        }

        // Change color based on level
        if (percent > 50) {
            lv_obj_set_style_text_color(icon_battery, lv_color_hex(0x00ff00), 0); // Green
        } else if (percent > 20) {
            lv_obj_set_style_text_color(icon_battery, lv_color_hex(0xffa500), 0); // Orange
        } else {
            lv_obj_set_style_text_color(icon_battery, lv_color_hex(0xff6b6b), 0); // Red
        }
    }

    // Update battery percentage
    if (label_battery_pct) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", percent);
        lv_label_set_text(label_battery_pct, buf);
    }
}

void updateLoRa(const char *lastRx, int rssi) {
    (void)lastRx; // Now handled by updateLastRx
    (void)rssi;
    // Just refresh LoRa freq/rate info
    refreshLoRaInfo();
}

void refreshLoRaInfo() {
    if (label_lora) {
        char buf[64];
        float freq = Config.loraTypes[loraIndex].frequency / 1000000.0;
        int rate = Config.loraTypes[loraIndex].dataRate;
        snprintf(buf, sizeof(buf), "LoRa: %.3f MHz  %d bps", freq, rate);
        lv_label_set_text(label_lora, buf);
    }
}

void updateLastRx() {
    if (!label_last_rx) return;

    const std::vector<StationStats> &stations = STORAGE_Utils::getStationStats();

    if (stations.empty()) {
        lv_label_set_text(label_last_rx, "Last RX:\n---");
        return;
    }

    // Sort by lastHeard (most recent first)
    std::vector<size_t> indices(stations.size());
    for (size_t i = 0; i < stations.size(); i++) indices[i] = i;
    std::sort(indices.begin(), indices.end(), [&stations](size_t a, size_t b) {
        return stations[a].lastHeard > stations[b].lastHeard;
    });

    // Build display string (max 3 stations)
    String text = "Last RX:";
    size_t count = (indices.size() < 3) ? indices.size() : 3;
    char line[80];

    for (size_t i = 0; i < count; i++) {
        const StationStats &s = stations[indices[i]];
        snprintf(line, sizeof(line), "\n%02d:%02d:%02d   %-9s   RSSI:%d   SNR:%.1f",
                 hour(s.lastHeard), minute(s.lastHeard), second(s.lastHeard),
                 s.callsign.c_str(), s.lastRssi, s.lastSnr);
        text += line;
    }

    lv_label_set_text(label_last_rx, text.c_str());
}

void updateWiFi(bool connected, int rssi) {
    if (icon_wifi) {
        // Show icon only if WiFi is connected
        if (connected && !WiFiUserDisabled && !WiFiEcoMode) {
            lv_obj_clear_flag(icon_wifi, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(icon_wifi, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void updateCallsign(const char *callsign) {
    if (label_callsign) {
        lv_label_set_text(label_callsign, callsign);
    }
}

void updateTime(int day, int month, int year, int hour, int minute, int second) {
    (void)year;   // Unused
    (void)second; // Unused
    if (label_time) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d/%02d %02d:%02d", day, month, hour, minute);
        lv_label_set_text(label_time, buf);
    }
}

void updateBluetooth() {
    if (icon_bluetooth) {
        // Show icon only if BT is connected
        if (bluetoothActive && !BLE_Utils::isSleeping() && bluetoothConnected) {
            lv_obj_clear_flag(icon_bluetooth, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(icon_bluetooth, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void returnToDashboard() {
    if (screen_main) {
        lv_scr_load_anim(screen_main, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
    }
}

// Provide label access for UISettings (callsign, wifi labels)
lv_obj_t* getLabelCallsign() { return label_callsign; }
lv_obj_t* getLabelWifi() { return nullptr; } // Removed from dashboard, now icon only

} // namespace UIDashboard

#endif // USE_LVGL_UI
