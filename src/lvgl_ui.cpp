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
#include <esp_wifi.h>
#define TOUCH_MODULES_GT911
#include <TouchLib.h>
#include <Wire.h>
#include "lvgl_ui.h"
#include "board_pinout.h"
#include "configuration.h"
#include "battery_utils.h"
#include "lora_utils.h"
#include "station_utils.h"
#include "msg_utils.h"
#include "notification_utils.h"
#include "custom_characters.h"
#include "ble_utils.h"
#include "wifi_utils.h"
#include "storage_utils.h"

// APRS symbol mapping (same as display.cpp)
static const char* symbolArray[] = { "[", ">", "j", "b", "<", "s", "u", "R", "v", "(", ";", "-", "k",
                                     "C", "a", "Y", "O", "'", "=", "y", "U", "p", "_", ")"};
static const int symbolArraySize = sizeof(symbolArray)/sizeof(symbolArray[0]);
static const uint8_t* symbolsAPRS[] = {runnerSymbol, carSymbol, jeepSymbol, bikeSymbol, motorcycleSymbol, shipSymbol,
                                       truck18Symbol, recreationalVehicleSymbol, vanSymbol, carsateliteSymbol, tentSymbol,
                                       houseSymbol, truckSymbol, canoeSymbol, ambulanceSymbol, yatchSymbol, baloonSymbol,
                                       aircraftSymbol, trainSymbol, yagiSymbol, busSymbol, dogSymbol, wxSymbol, wheelchairSymbol};
#define SYMBOL_WIDTH 16
#define SYMBOL_HEIGHT 14

// External data sources
extern Configuration Config;
extern uint8_t myBeaconsIndex;
extern int myBeaconsSize;
extern TinyGPSPlus gps;
extern bool WiFiConnected;
extern bool WiFiEcoMode;
extern bool WiFiUserDisabled;
extern bool bluetoothActive;
extern bool bluetoothConnected;
extern String batteryVoltage;
extern APRSPacket lastReceivedPacket;
extern bool sendUpdate;  // Set to true to trigger beacon transmission
extern uint8_t loraIndex;
extern int loraIndexSize;
extern bool displayEcoMode;
extern uint8_t screenBrightness;

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

// UI Elements - Main screen
static lv_obj_t* screen_main = nullptr;
static lv_obj_t* label_callsign = nullptr;
static lv_obj_t* label_gps = nullptr;
static lv_obj_t* label_battery = nullptr;
static lv_obj_t* label_lora = nullptr;
static lv_obj_t* label_wifi = nullptr;
static lv_obj_t* label_bluetooth = nullptr;
static lv_obj_t* label_storage = nullptr;
static lv_obj_t* label_time = nullptr;
static lv_obj_t* aprs_symbol_canvas = nullptr;
static lv_color_t* aprs_symbol_buf = nullptr;

// UI Elements - Setup screen
static lv_obj_t* screen_setup = nullptr;

// UI Elements - Frequency selection screen
static lv_obj_t* screen_freq = nullptr;

// UI Elements - Callsign selection screen
static lv_obj_t* screen_callsign = nullptr;

// UI Elements - Display settings screen
static lv_obj_t* screen_display = nullptr;

// UI Elements - Messages screen
static lv_obj_t* screen_msg = nullptr;

// UI Elements - LoRa Speed screen
static lv_obj_t* screen_speed = nullptr;

// UI Elements - Sound settings screen
static lv_obj_t* screen_sound = nullptr;

// UI Elements - WiFi settings screen
static lv_obj_t* screen_wifi = nullptr;
static lv_obj_t* wifi_switch = nullptr;
static lv_obj_t* wifi_status_label = nullptr;
static lv_obj_t* wifi_ip_row = nullptr;
static lv_obj_t* wifi_ip_label = nullptr;
static lv_obj_t* wifi_rssi_row = nullptr;
static lv_obj_t* wifi_rssi_label = nullptr;
static lv_timer_t* wifi_update_timer = nullptr;

// UI Elements - Bluetooth settings screen
static lv_obj_t* screen_bluetooth = nullptr;
static lv_obj_t* bluetooth_switch = nullptr;
static lv_obj_t* bluetooth_status_label = nullptr;
static lv_obj_t* bluetooth_device_label = nullptr;
static lv_obj_t* bluetooth_device_row = nullptr;
static lv_timer_t* bluetooth_update_timer = nullptr;

// Current selection tracking (for highlight updates)
static lv_obj_t* current_freq_btn = nullptr;
static lv_obj_t* current_speed_btn = nullptr;
static lv_obj_t* current_callsign_btn = nullptr;

// LVGL tick tracking
static uint32_t last_tick = 0;

// Track if LVGL display is already initialized
static bool lvgl_display_initialized = false;

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

// Forward declarations
static void create_setup_screen();
static void create_freq_screen();
static void create_callsign_screen();
static void create_display_screen();
static void create_msg_screen();
static void create_speed_screen();
static void create_sound_screen();
static void create_wifi_screen();
static void create_bluetooth_screen();

// Draw APRS symbol on canvas
// Symbol size: 16x14 pixels (no scaling to keep it compact)
#define APRS_CANVAS_WIDTH SYMBOL_WIDTH
#define APRS_CANVAS_HEIGHT SYMBOL_HEIGHT

static void drawAPRSSymbol(const char* symbolChar) {
    if (!aprs_symbol_canvas || !aprs_symbol_buf) return;

    // Find symbol index
    int symbolIndex = -1;
    for (int i = 0; i < symbolArraySize; i++) {
        if (strcmp(symbolChar, symbolArray[i]) == 0) {
            symbolIndex = i;
            break;
        }
    }

    // Clear canvas with transparent/dark background
    lv_canvas_fill_bg(aprs_symbol_canvas, lv_color_hex(0x16213e), LV_OPA_COVER);

    if (symbolIndex < 0) return;  // Symbol not found

    const uint8_t* bitMap = symbolsAPRS[symbolIndex];
    lv_color_t white = lv_color_hex(0xffffff);  // White like callsign

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
static void btn_beacon_clicked(lv_event_t* e) {
    sendUpdate = true;
    Serial.println("[LVGL] BEACON button pressed - sending beacon");
}

static void btn_setup_clicked(lv_event_t* e) {
    Serial.println("[LVGL] SETUP button pressed");
    if (!screen_setup) {
        create_setup_screen();
    }
    lv_scr_load_anim(screen_setup, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
}

static void btn_back_clicked(lv_event_t* e) {
    Serial.println("[LVGL] BACK button pressed");
    lv_scr_load_anim(screen_main, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
}

static void btn_back_to_setup_clicked(lv_event_t* e) {
    Serial.println("[LVGL] BACK to SETUP button pressed");
    lv_scr_load_anim(screen_setup, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
}

static void btn_wifi_back_clicked(lv_event_t* e) {
    Serial.println("[LVGL] WiFi BACK button pressed");
    // Stop WiFi update timer
    if (wifi_update_timer) {
        lv_timer_del(wifi_update_timer);
        wifi_update_timer = nullptr;
    }
    lv_scr_load_anim(screen_setup, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
}

static void btn_msg_clicked(lv_event_t* e) {
    Serial.println("[LVGL] MSG button pressed");
    if (!screen_msg) {
        create_msg_screen();
    }
    lv_scr_load_anim(screen_msg, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
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
    lv_obj_set_style_text_color(label_callsign, lv_color_hex(0xffffff), 0);  // White
    lv_obj_set_style_text_font(label_callsign, &lv_font_montserrat_14, 0);

    // APRS symbol canvas (center) - bitmap symbol scaled 2x
    aprs_symbol_buf = (lv_color_t*)malloc(APRS_CANVAS_WIDTH * APRS_CANVAS_HEIGHT * sizeof(lv_color_t));
    if (aprs_symbol_buf) {
        aprs_symbol_canvas = lv_canvas_create(status_bar);
        lv_canvas_set_buffer(aprs_symbol_canvas, aprs_symbol_buf, APRS_CANVAS_WIDTH, APRS_CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);
        lv_obj_set_size(aprs_symbol_canvas, APRS_CANVAS_WIDTH, APRS_CANVAS_HEIGHT);
        // Draw initial symbol from current beacon
        Beacon* currentBeacon = &Config.beacons[myBeaconsIndex];
        drawAPRSSymbol(currentBeacon->symbol.c_str());
    }

    // Date/Time label (right)
    label_time = lv_label_create(status_bar);
    lv_label_set_text(label_time, "--/--/---- --:--:-- UTC");
    lv_obj_set_style_text_color(label_time, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_14, 0);

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
    lv_label_set_text(label_gps, "GPS: -- sat Lat: --.---- Lon: --.----\nAlt: ---- m  Spd: --- km/h");
    lv_obj_set_style_text_color(label_gps, lv_color_hex(0x00d4ff), 0);
    lv_obj_set_style_text_font(label_gps, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(label_gps, 0, 0);

    // LoRa info
    label_lora = lv_label_create(content);
    char lora_init[96];
    float freq = Config.loraTypes[loraIndex].frequency / 1000000.0;
    int rate = Config.loraTypes[loraIndex].dataRate;
    snprintf(lora_init, sizeof(lora_init), "LoRa: %.3f MHz  %d bps\nLast RX: ---", freq, rate);
    lv_label_set_text(label_lora, lora_init);
    lv_obj_set_style_text_color(label_lora, lv_color_hex(0xff6b6b), 0);
    lv_obj_set_style_text_font(label_lora, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(label_lora, 0, 40);

    // WiFi info
    label_wifi = lv_label_create(content);
    lv_label_set_text(label_wifi, "WiFi: ---");
    lv_obj_set_style_text_color(label_wifi, lv_color_hex(0x00d4ff), 0);  // Cyan like GPS
    lv_obj_set_style_text_font(label_wifi, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(label_wifi, 0, 80);

    // Bluetooth info
    label_bluetooth = lv_label_create(content);
    if (!bluetoothActive) {
        lv_label_set_text(label_bluetooth, "BT: Disabled");
        lv_obj_set_style_text_color(label_bluetooth, lv_color_hex(0x666666), 0);  // Gray
    } else if (bluetoothConnected) {
        String addr = BLE_Utils::getConnectedDeviceAddress();
        if (addr.length() > 0) {
            String btText = "BT: > " + addr;
            lv_label_set_text(label_bluetooth, btText.c_str());
        } else {
            lv_label_set_text(label_bluetooth, "BT: Connected");
        }
        lv_obj_set_style_text_color(label_bluetooth, lv_color_hex(0xc792ea), 0);  // Purple
    } else {
        lv_label_set_text(label_bluetooth, "BT: Waiting...");
        lv_obj_set_style_text_color(label_bluetooth, lv_color_hex(0xffa500), 0);  // Orange
    }
    lv_obj_set_style_text_font(label_bluetooth, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(label_bluetooth, 0, 100);

    // Battery info
    label_battery = lv_label_create(content);
    lv_label_set_text(label_battery, "Bat: --.-- V (--%)");
    lv_obj_set_style_text_color(label_battery, lv_color_hex(0xff6b6b), 0);  // Red/coral color
    lv_obj_set_style_text_font(label_battery, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(label_battery, 0, 120);

    // Storage info
    label_storage = lv_label_create(content);
    String storageInfo = "Storage: " + STORAGE_Utils::getStorageType();
    if (STORAGE_Utils::isSDAvailable()) {
        storageInfo += " (" + String(STORAGE_Utils::getTotalBytes() / (1024*1024)) + "MB)";
    }
    lv_label_set_text(label_storage, storageInfo.c_str());
    lv_obj_set_style_text_color(label_storage, lv_color_hex(0xffcc00), 0);  // Yellow/gold
    lv_obj_set_style_text_font(label_storage, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(label_storage, 0, 140);

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
    lv_obj_add_event_cb(btn_msg, btn_msg_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_msg = lv_label_create(btn_msg);
    lv_label_set_text(lbl_msg, "MSG");
    lv_obj_center(lbl_msg);
    lv_obj_set_style_text_color(lbl_msg, lv_color_hex(0x000000), 0);

    // Settings button
    lv_obj_t* btn_settings = lv_btn_create(btn_bar);
    lv_obj_set_size(btn_settings, 90, 30);
    lv_obj_set_style_bg_color(btn_settings, lv_color_hex(0xc792ea), 0);
    lv_obj_add_event_cb(btn_settings, btn_setup_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_settings = lv_label_create(btn_settings);
    lv_label_set_text(lbl_settings, "SETUP");
    lv_obj_center(lbl_settings);
    lv_obj_set_style_text_color(lbl_settings, lv_color_hex(0x000000), 0);

    // Load the screen
    lv_scr_load(screen_main);
}

// Setup menu item callbacks
static void setup_item_callsign(lv_event_t* e) {
    Serial.println("[LVGL] Setup: Callsign selected");
    // Recreate screen each time to update current selection highlight
    if (screen_callsign) {
        lv_obj_del(screen_callsign);
        screen_callsign = nullptr;
        current_callsign_btn = nullptr;
    }
    create_callsign_screen();
    lv_scr_load_anim(screen_callsign, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
}

static void setup_item_frequency(lv_event_t* e) {
    Serial.println("[LVGL] Setup: Frequency selected");
    // Recreate screen each time to update current selection highlight
    if (screen_freq) {
        lv_obj_del(screen_freq);
        screen_freq = nullptr;
        current_freq_btn = nullptr;  // Reset pointer to deleted object
    }
    create_freq_screen();
    lv_scr_load_anim(screen_freq, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
}

static void setup_item_speed(lv_event_t* e) {
    Serial.println("[LVGL] Setup: Speed selected");
    // Recreate screen each time to update current selection highlight
    if (screen_speed) {
        lv_obj_del(screen_speed);
        screen_speed = nullptr;
        current_speed_btn = nullptr;  // Reset pointer to deleted object
    }
    create_speed_screen();
    lv_scr_load_anim(screen_speed, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
}

static void setup_item_display(lv_event_t* e) {
    Serial.println("[LVGL] Setup: Display selected");
    if (!screen_display) {
        create_display_screen();
    }
    lv_scr_load_anim(screen_display, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
}

static void setup_item_sound(lv_event_t* e) {
    Serial.println("[LVGL] Setup: Sound selected");
    if (!screen_sound) {
        create_sound_screen();
    }
    lv_scr_load_anim(screen_sound, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
}

static void setup_item_wifi(lv_event_t* e) {
    Serial.println("[LVGL] Setup: WiFi selected");
    // Recreate screen each time to update status
    if (screen_wifi) {
        // Stop timer before deleting screen
        if (wifi_update_timer) {
            lv_timer_del(wifi_update_timer);
            wifi_update_timer = nullptr;
        }
        lv_obj_del(screen_wifi);
        screen_wifi = nullptr;
        wifi_switch = nullptr;
        wifi_status_label = nullptr;
        wifi_ip_row = nullptr;
        wifi_ip_label = nullptr;
        wifi_rssi_row = nullptr;
        wifi_rssi_label = nullptr;
    }
    create_wifi_screen();
    lv_scr_load_anim(screen_wifi, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
}

static void setup_item_bluetooth(lv_event_t* e) {
    Serial.println("[LVGL] Setup: Bluetooth selected");
    // Recreate screen each time to update status
    if (screen_bluetooth) {
        // Stop timer before deleting screen
        if (bluetooth_update_timer) {
            lv_timer_del(bluetooth_update_timer);
            bluetooth_update_timer = nullptr;
        }
        lv_obj_del(screen_bluetooth);
        screen_bluetooth = nullptr;
        bluetooth_switch = nullptr;
        bluetooth_status_label = nullptr;
        bluetooth_device_label = nullptr;
        bluetooth_device_row = nullptr;
    }
    create_bluetooth_screen();
    lv_scr_load_anim(screen_bluetooth, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
}

static void setup_item_reboot(lv_event_t* e) {
    Serial.println("[LVGL] Setup: Reboot selected");
    ESP.restart();
}

// Web-Conf screen - blocking mode with LVGL display
static lv_obj_t* screen_webconf = nullptr;
static bool webconf_reboot_requested = false;

static void webconf_reboot_cb(lv_event_t* e) {
    webconf_reboot_requested = true;
}

static void setup_item_webconf(lv_event_t* e) {
    Serial.println("[LVGL] Setup: Web-Conf Mode selected - entering blocking mode");

    // Create web-conf screen
    screen_webconf = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_webconf, lv_color_hex(0x1a1a2e), 0);

    // Title bar (orange for web-conf)
    lv_obj_t* title_bar = lv_obj_create(screen_webconf);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, 35);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0xff6b35), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "Web Configuration");
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_center(title);

    // Content area
    lv_obj_t* content = lv_obj_create(screen_webconf);
    lv_obj_set_size(content, SCREEN_WIDTH - 20, SCREEN_HEIGHT - 55);
    lv_obj_set_pos(content, 10, 45);
    lv_obj_set_style_bg_color(content, lv_color_hex(0x0f0f23), 0);
    lv_obj_set_style_border_color(content, lv_color_hex(0xff6b35), 0);
    lv_obj_set_style_radius(content, 8, 0);
    lv_obj_set_style_pad_all(content, 15, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Starting message
    lv_obj_t* msg1 = lv_label_create(content);
    lv_label_set_text(msg1, "Starting WiFi Access Point...");
    lv_obj_set_style_text_color(msg1, lv_color_hex(0xffa500), 0);
    lv_obj_set_style_text_font(msg1, &lv_font_montserrat_14, 0);

    // Load screen and refresh
    lv_scr_load(screen_webconf);
    lv_timer_handler();

    // Start AP mode
    bool success = WIFI_Utils::startAPModeNonBlocking();

    // Update screen with result
    lv_obj_clean(content);

    if (success) {
        String apName = WIFI_Utils::getAPName();

        lv_obj_t* lbl_status = lv_label_create(content);
        lv_label_set_text(lbl_status, "WiFi AP Active");
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(0x00ff88), 0);
        lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_18, 0);

        lv_obj_t* lbl_ssid = lv_label_create(content);
        String ssidText = "SSID: " + apName;
        lv_label_set_text(lbl_ssid, ssidText.c_str());
        lv_obj_set_style_text_color(lbl_ssid, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(lbl_ssid, &lv_font_montserrat_14, 0);

        lv_obj_t* lbl_ip = lv_label_create(content);
        lv_label_set_text(lbl_ip, "IP: 192.168.4.1");
        lv_obj_set_style_text_color(lbl_ip, lv_color_hex(0x00d4ff), 0);
        lv_obj_set_style_text_font(lbl_ip, &lv_font_montserrat_18, 0);

        lv_obj_t* lbl_info = lv_label_create(content);
        lv_label_set_text(lbl_info, "Connect to WiFi AP\nOpen http://192.168.4.1");
        lv_obj_set_style_text_color(lbl_info, lv_color_hex(0xaaaaaa), 0);
        lv_obj_set_style_text_font(lbl_info, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_align(lbl_info, LV_TEXT_ALIGN_CENTER, 0);

        // Reboot button
        lv_obj_t* btn_reboot = lv_btn_create(content);
        lv_obj_set_size(btn_reboot, 120, 40);
        lv_obj_set_style_bg_color(btn_reboot, lv_color_hex(0xff6b6b), 0);
        lv_obj_add_event_cb(btn_reboot, webconf_reboot_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t* lbl_btn = lv_label_create(btn_reboot);
        lv_label_set_text(lbl_btn, "Reboot");
        lv_obj_center(lbl_btn);

        // Force complete screen refresh
        lv_refr_now(NULL);
        lv_timer_handler();
        delay(100);
        lv_refr_now(NULL);

        // *** BLOCKING LOOP - Only LVGL and touch handled ***
        Serial.println("[LVGL] Entering Web-Conf blocking loop");
        webconf_reboot_requested = false;

        while (!webconf_reboot_requested) {
            // Update LVGL tick
            uint32_t now = millis();
            lv_tick_inc(now - last_tick);
            last_tick = now;

            // Handle LVGL (display + touch)
            lv_timer_handler();

            // Yield to system
            yield();

            // Small delay
            delay(10);
        }

        // Reboot requested
        Serial.println("[LVGL] Rebooting from Web-Conf mode");
        ESP.restart();

    } else {
        lv_obj_t* lbl_error = lv_label_create(content);
        lv_label_set_text(lbl_error, "Failed to start AP!");
        lv_obj_set_style_text_color(lbl_error, lv_color_hex(0xff6b6b), 0);
        lv_obj_set_style_text_font(lbl_error, &lv_font_montserrat_18, 0);

        lv_obj_t* lbl_info = lv_label_create(content);
        lv_label_set_text(lbl_info, "Touch to go back");
        lv_obj_set_style_text_color(lbl_info, lv_color_hex(0xaaaaaa), 0);

        lv_timer_handler();
        delay(3000);

        // Go back to setup
        lv_scr_load_anim(screen_setup, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
    }
}

// Create the setup screen
static void create_setup_screen() {
    screen_setup = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_setup, lv_color_hex(0x1a1a2e), 0);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen_setup);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, 35);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0xc792ea), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 5, 0);

    // Back button
    lv_obj_t* btn_back = lv_btn_create(title_bar);
    lv_obj_set_size(btn_back, 60, 25);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x16213e), 0);
    lv_obj_add_event_cb(btn_back, btn_back_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "< BACK");
    lv_obj_center(lbl_back);

    // Title
    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "SETUP");
    lv_obj_set_style_text_color(title, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 20, 0);

    // Menu list
    lv_obj_t* list = lv_list_create(screen_setup);
    lv_obj_set_size(list, SCREEN_WIDTH - 10, SCREEN_HEIGHT - 45);
    lv_obj_set_pos(list, 5, 40);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x0f0f23), 0);
    lv_obj_set_style_border_color(list, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_radius(list, 8, 0);

    // Menu items
    lv_obj_t* btn;

    btn = lv_list_add_btn(list, LV_SYMBOL_CALL, "Callsign");
    lv_obj_add_event_cb(btn, setup_item_callsign, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_btn(list, LV_SYMBOL_WIFI, "LoRa Frequency");
    lv_obj_add_event_cb(btn, setup_item_frequency, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_btn(list, LV_SYMBOL_SHUFFLE, "LoRa Speed");
    lv_obj_add_event_cb(btn, setup_item_speed, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_btn(list, LV_SYMBOL_SETTINGS, "Display");
    lv_obj_add_event_cb(btn, setup_item_display, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_btn(list, LV_SYMBOL_AUDIO, "Sound");
    lv_obj_add_event_cb(btn, setup_item_sound, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_btn(list, LV_SYMBOL_WIFI, "WiFi");
    lv_obj_add_event_cb(btn, setup_item_wifi, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_btn(list, LV_SYMBOL_BLUETOOTH, "Bluetooth");
    lv_obj_add_event_cb(btn, setup_item_bluetooth, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_btn(list, LV_SYMBOL_UPLOAD, "Web-Conf Mode");
    lv_obj_add_event_cb(btn, setup_item_webconf, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_btn(list, LV_SYMBOL_REFRESH, "Reboot");
    lv_obj_add_event_cb(btn, setup_item_reboot, LV_EVENT_CLICKED, NULL);

    Serial.println("[LVGL] Setup screen created");
}

// Timer callback to navigate back to setup
static void nav_to_setup_timer_cb(lv_timer_t* timer) {
    lv_scr_load_anim(screen_setup, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
    lv_timer_del(timer);
}

// Frequency item selection callback
static void freq_item_clicked(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_current_target(e);
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    Serial.printf("[LVGL] Frequency %d selected\n", index);
    LoRa_Utils::requestFrequencyChange(index);

    // Reset previous selection to black
    if (current_freq_btn && current_freq_btn != btn) {
        lv_obj_set_style_bg_color(current_freq_btn, lv_color_hex(0x0f0f23), 0);
        lv_obj_set_style_text_color(current_freq_btn, lv_color_hex(0xffffff), 0);
    }

    // Highlight new selection immediately
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x00ff88), 0);
    lv_obj_set_style_text_color(btn, lv_color_hex(0x000000), 0);
    current_freq_btn = btn;

    // Navigate back after 600ms
    lv_timer_create(nav_to_setup_timer_cb, 600, NULL);
}

// Create the frequency selection screen
static void create_freq_screen() {
    screen_freq = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_freq, lv_color_hex(0x1a1a2e), 0);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen_freq);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, 35);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x00d4ff), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 5, 0);

    // Back button
    lv_obj_t* btn_back = lv_btn_create(title_bar);
    lv_obj_set_size(btn_back, 60, 25);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x16213e), 0);
    lv_obj_add_event_cb(btn_back, btn_back_to_setup_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "< BACK");
    lv_obj_center(lbl_back);

    // Title
    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "LoRa Frequency");
    lv_obj_set_style_text_color(title, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 20, 0);

    // Frequency list
    lv_obj_t* list = lv_list_create(screen_freq);
    lv_obj_set_size(list, SCREEN_WIDTH - 10, SCREEN_HEIGHT - 45);
    lv_obj_set_pos(list, 5, 40);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x0f0f23), 0);
    lv_obj_set_style_border_color(list, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_radius(list, 8, 0);

    // Add frequency options from Config (skip US - index 3)
    for (int i = 0; i < loraIndexSize && i < (int)Config.loraTypes.size(); i++) {
        if (i == 3) continue;  // Skip US frequency

        char buf[64];
        float freq = Config.loraTypes[i].frequency / 1000000.0;

        // Get region name
        const char* region;
        switch (i) {
            case 0: region = "EU/WORLD"; break;
            case 1: region = "POLAND"; break;
            case 2: region = "UK"; break;
            default: region = "CUSTOM"; break;
        }

        snprintf(buf, sizeof(buf), "%s - %.3f MHz", region, freq);

        lv_obj_t* btn = lv_list_add_btn(list, LV_SYMBOL_WIFI, buf);
        lv_obj_add_event_cb(btn, freq_item_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        // Set style for all items explicitly
        if (i == loraIndex) {
            // Highlight current selection
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x00ff88), 0);
            lv_obj_set_style_text_color(btn, lv_color_hex(0x000000), 0);
            current_freq_btn = btn;  // Track current selection
        } else {
            // Default style for non-selected items
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x0f0f23), 0);
            lv_obj_set_style_text_color(btn, lv_color_hex(0xffffff), 0);
        }
    }

    Serial.println("[LVGL] Frequency screen created");
}

// Speed item selection callback
static void speed_item_clicked(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_current_target(e);
    int dataRate = (int)(intptr_t)lv_event_get_user_data(e);
    Serial.printf("[LVGL] Speed %d bps selected\n", dataRate);
    LoRa_Utils::requestDataRateChange(dataRate);

    // Reset previous selection to black
    if (current_speed_btn && current_speed_btn != btn) {
        lv_obj_set_style_bg_color(current_speed_btn, lv_color_hex(0x0f0f23), 0);
        lv_obj_set_style_text_color(current_speed_btn, lv_color_hex(0xffffff), 0);
    }

    // Highlight new selection immediately
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x00ff88), 0);
    lv_obj_set_style_text_color(btn, lv_color_hex(0x000000), 0);
    current_speed_btn = btn;

    // Navigate back after 600ms
    lv_timer_create(nav_to_setup_timer_cb, 600, NULL);
}

// Create the speed selection screen
static void create_speed_screen() {
    screen_speed = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_speed, lv_color_hex(0x1a1a2e), 0);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen_speed);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, 35);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0xff6b6b), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 5, 0);

    // Back button
    lv_obj_t* btn_back = lv_btn_create(title_bar);
    lv_obj_set_size(btn_back, 60, 25);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x16213e), 0);
    lv_obj_add_event_cb(btn_back, btn_back_to_setup_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "< BACK");
    lv_obj_center(lbl_back);

    // Title
    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "LoRa Speed");
    lv_obj_set_style_text_color(title, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 20, 0);

    // Speed list
    lv_obj_t* list = lv_list_create(screen_speed);
    lv_obj_set_size(list, SCREEN_WIDTH - 10, SCREEN_HEIGHT - 45);
    lv_obj_set_pos(list, 5, 40);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x0f0f23), 0);
    lv_obj_set_style_border_color(list, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_radius(list, 8, 0);

    // Available data rates with descriptions
    struct SpeedOption {
        int dataRate;
        const char* desc;
    };
    const SpeedOption speeds[] = {
        {1200, "1200 bps (SF9, Fast)"},
        {610,  "610 bps (SF10)"},
        {300,  "300 bps (SF12, Long range)"},
        {244,  "244 bps (SF12)"},
        {209,  "209 bps (SF12)"},
        {183,  "183 bps (SF12, Longest)"}
    };

    int currentDataRate = Config.loraTypes[loraIndex].dataRate;

    for (int i = 0; i < 6; i++) {
        lv_obj_t* btn = lv_list_add_btn(list, LV_SYMBOL_SHUFFLE, speeds[i].desc);
        lv_obj_add_event_cb(btn, speed_item_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)speeds[i].dataRate);

        // Set style for all items explicitly
        if (speeds[i].dataRate == currentDataRate) {
            // Highlight current selection
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x00ff88), 0);
            lv_obj_set_style_text_color(btn, lv_color_hex(0x000000), 0);
            current_speed_btn = btn;  // Track current selection
        } else {
            // Default style for non-selected items
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x0f0f23), 0);
            lv_obj_set_style_text_color(btn, lv_color_hex(0xffffff), 0);
        }
    }

    Serial.println("[LVGL] Speed screen created");
}

// Callsign item selection callback
static void callsign_item_clicked(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_current_target(e);
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    Serial.printf("[LVGL] Callsign %d selected\n", index);
    myBeaconsIndex = index;
    STATION_Utils::saveIndex(0, myBeaconsIndex);
    // Update the callsign label on main screen
    lv_label_set_text(label_callsign, Config.beacons[myBeaconsIndex].callsign.c_str());
    // Update APRS symbol for new beacon
    drawAPRSSymbol(Config.beacons[myBeaconsIndex].symbol.c_str());

    // Reset previous selection to black
    if (current_callsign_btn && current_callsign_btn != btn) {
        lv_obj_set_style_bg_color(current_callsign_btn, lv_color_hex(0x0f0f23), 0);
        lv_obj_set_style_text_color(current_callsign_btn, lv_color_hex(0xffffff), 0);
    }

    // Highlight new selection immediately
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x00ff88), 0);
    lv_obj_set_style_text_color(btn, lv_color_hex(0x000000), 0);
    current_callsign_btn = btn;

    // Navigate back after 600ms
    lv_timer_create(nav_to_setup_timer_cb, 600, NULL);
}

// Create the callsign selection screen
static void create_callsign_screen() {
    screen_callsign = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_callsign, lv_color_hex(0x1a1a2e), 0);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen_callsign);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, 35);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x00ff88), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 5, 0);

    // Back button
    lv_obj_t* btn_back = lv_btn_create(title_bar);
    lv_obj_set_size(btn_back, 60, 25);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x16213e), 0);
    lv_obj_add_event_cb(btn_back, btn_back_to_setup_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "< BACK");
    lv_obj_center(lbl_back);

    // Title
    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "Callsign");
    lv_obj_set_style_text_color(title, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 20, 0);

    // Callsign list
    lv_obj_t* list = lv_list_create(screen_callsign);
    lv_obj_set_size(list, SCREEN_WIDTH - 10, SCREEN_HEIGHT - 45);
    lv_obj_set_pos(list, 5, 40);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x0f0f23), 0);
    lv_obj_set_style_border_color(list, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_radius(list, 8, 0);

    // Add callsign options from Config
    for (int i = 0; i < myBeaconsSize && i < (int)Config.beacons.size(); i++) {
        String label = Config.beacons[i].callsign;
        if (Config.beacons[i].profileLabel.length() > 0) {
            label += " (" + Config.beacons[i].profileLabel + ")";
        }

        lv_obj_t* btn = lv_list_add_btn(list, LV_SYMBOL_CALL, label.c_str());
        lv_obj_add_event_cb(btn, callsign_item_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        // Set style for all items explicitly
        if (i == myBeaconsIndex) {
            // Highlight current selection
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x00ff88), 0);
            lv_obj_set_style_text_color(btn, lv_color_hex(0x000000), 0);
            current_callsign_btn = btn;  // Track current selection
        } else {
            // Default style for non-selected items
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x0f0f23), 0);
            lv_obj_set_style_text_color(btn, lv_color_hex(0xffffff), 0);
        }
    }

    Serial.println("[LVGL] Callsign screen created");
}

// Display settings callbacks
static lv_obj_t* eco_switch = nullptr;
static lv_obj_t* brightness_slider = nullptr;
static lv_obj_t* brightness_label = nullptr;

// Brightness range
static const uint8_t BRIGHT_MIN = 50;
static const uint8_t BRIGHT_MAX = 255;

static void eco_switch_changed(lv_event_t* e) {
    lv_obj_t* sw = lv_event_get_target(e);
    displayEcoMode = lv_obj_has_state(sw, LV_STATE_CHECKED);
    Serial.printf("[LVGL] ECO Mode: %s\n", displayEcoMode ? "ON" : "OFF");
}

static void brightness_slider_changed(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    screenBrightness = (uint8_t)lv_slider_get_value(slider);

    #ifdef BOARD_BL_PIN
        analogWrite(BOARD_BL_PIN, screenBrightness);
    #endif

    // Update label with percentage
    if (brightness_label) {
        int pct = (screenBrightness - BRIGHT_MIN) * 100 / (BRIGHT_MAX - BRIGHT_MIN);
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", pct);
        lv_label_set_text(brightness_label, buf);
    }
}

static void brightness_slider_released(lv_event_t* e) {
    // Save only when released
    STATION_Utils::saveIndex(2, screenBrightness);
    Serial.printf("[LVGL] Brightness saved: %d\n", screenBrightness);
}

// Create the display settings screen
static void create_display_screen() {
    screen_display = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_display, lv_color_hex(0x1a1a2e), 0);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen_display);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, 35);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0xffd700), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 5, 0);

    // Back button
    lv_obj_t* btn_back = lv_btn_create(title_bar);
    lv_obj_set_size(btn_back, 60, 25);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x16213e), 0);
    lv_obj_add_event_cb(btn_back, btn_back_to_setup_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "< BACK");
    lv_obj_center(lbl_back);

    // Title
    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "Display");
    lv_obj_set_style_text_color(title, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 20, 0);

    // Content area
    lv_obj_t* content = lv_obj_create(screen_display);
    lv_obj_set_size(content, SCREEN_WIDTH - 10, SCREEN_HEIGHT - 45);
    lv_obj_set_pos(content, 5, 40);
    lv_obj_set_style_bg_color(content, lv_color_hex(0x0f0f23), 0);
    lv_obj_set_style_border_color(content, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_radius(content, 8, 0);
    lv_obj_set_style_pad_all(content, 15, 0);

    // ECO Mode row
    lv_obj_t* eco_row = lv_obj_create(content);
    lv_obj_set_size(eco_row, lv_pct(100), 50);
    lv_obj_set_pos(eco_row, 0, 0);
    lv_obj_set_style_bg_opa(eco_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(eco_row, 0, 0);
    lv_obj_set_style_pad_all(eco_row, 0, 0);

    lv_obj_t* eco_label = lv_label_create(eco_row);
    lv_label_set_text(eco_label, "ECO Mode");
    lv_obj_set_style_text_color(eco_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(eco_label, &lv_font_montserrat_18, 0);
    lv_obj_align(eco_label, LV_ALIGN_LEFT_MID, 0, 0);

    eco_switch = lv_switch_create(eco_row);
    lv_obj_set_size(eco_switch, 60, 30);
    lv_obj_align(eco_switch, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(eco_switch, lv_color_hex(0x333333), LV_PART_MAIN);  // Off track
    lv_obj_set_style_bg_color(eco_switch, lv_color_hex(0x00ff88), LV_PART_INDICATOR | LV_STATE_CHECKED);  // On track
    lv_obj_set_style_bg_color(eco_switch, lv_color_hex(0xffffff), LV_PART_KNOB);  // Knob
    if (displayEcoMode) {
        lv_obj_add_state(eco_switch, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(eco_switch, eco_switch_changed, LV_EVENT_VALUE_CHANGED, NULL);

    // Brightness row - label on top, slider below
    lv_obj_t* bright_row = lv_obj_create(content);
    lv_obj_set_size(bright_row, lv_pct(100), 70);
    lv_obj_set_pos(bright_row, 0, 55);
    lv_obj_set_style_bg_opa(bright_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bright_row, 0, 0);
    lv_obj_set_style_pad_all(bright_row, 0, 0);

    // Title and value on same line
    lv_obj_t* bright_title = lv_label_create(bright_row);
    lv_label_set_text(bright_title, "Brightness");
    lv_obj_set_style_text_color(bright_title, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(bright_title, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(bright_title, 0, 0);

    // Value label (percentage)
    brightness_label = lv_label_create(bright_row);
    int pct = (screenBrightness - BRIGHT_MIN) * 100 / (BRIGHT_MAX - BRIGHT_MIN);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    lv_label_set_text(brightness_label, buf);
    lv_obj_set_style_text_color(brightness_label, lv_color_hex(0xffd700), 0);
    lv_obj_set_style_text_font(brightness_label, &lv_font_montserrat_14, 0);
    lv_obj_align(brightness_label, LV_ALIGN_TOP_RIGHT, 0, 0);

    // Slider (reduced width to fit within frame with knob)
    brightness_slider = lv_slider_create(bright_row);
    lv_obj_set_size(brightness_slider, lv_pct(90), 20);
    lv_obj_align(brightness_slider, LV_ALIGN_TOP_MID, 0, 30);
    lv_slider_set_range(brightness_slider, BRIGHT_MIN, BRIGHT_MAX);
    lv_slider_set_value(brightness_slider, screenBrightness, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0x444466), LV_PART_MAIN);  // Visible track
    lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0x00d4ff), LV_PART_INDICATOR);  // Cyan filled
    lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0xffffff), LV_PART_KNOB);  // White knob
    lv_obj_add_event_cb(brightness_slider, brightness_slider_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(brightness_slider, brightness_slider_released, LV_EVENT_RELEASED, NULL);

    Serial.println("[LVGL] Display settings screen created");
}

// Sound settings callbacks
static lv_obj_t* sound_switch = nullptr;
static lv_obj_t* volume_slider = nullptr;
static lv_obj_t* volume_label = nullptr;

static void sound_switch_changed(lv_event_t* e) {
    lv_obj_t* sw = lv_event_get_target(e);
    Config.notification.buzzerActive = lv_obj_has_state(sw, LV_STATE_CHECKED);
    Serial.printf("[LVGL] Sound: %s\n", Config.notification.buzzerActive ? "ON" : "OFF");
}

static void volume_slider_changed(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    Config.notification.volume = lv_slider_get_value(slider);

    if (volume_label) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", Config.notification.volume);
        lv_label_set_text(volume_label, buf);
    }
}

static void volume_slider_released(lv_event_t* e) {
    // Play confirmation beep at selected volume
    if (Config.notification.buzzerActive) {
        NOTIFICATION_Utils::playTone(1000, 100);
    }
}

static void tx_beep_changed(lv_event_t* e) {
    lv_obj_t* sw = lv_event_get_target(e);
    Config.notification.txBeep = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

static void rx_beep_changed(lv_event_t* e) {
    lv_obj_t* sw = lv_event_get_target(e);
    Config.notification.messageRxBeep = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

static void station_beep_changed(lv_event_t* e) {
    lv_obj_t* sw = lv_event_get_target(e);
    Config.notification.stationBeep = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

// Create the sound settings screen
static void create_sound_screen() {
    screen_sound = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_sound, lv_color_hex(0x1a1a2e), 0);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen_sound);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, 35);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0xff6b6b), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 5, 0);

    // Back button
    lv_obj_t* btn_back = lv_btn_create(title_bar);
    lv_obj_set_size(btn_back, 60, 25);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x16213e), 0);
    lv_obj_add_event_cb(btn_back, btn_back_to_setup_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "< BACK");
    lv_obj_center(lbl_back);

    // Title
    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "Sound");
    lv_obj_set_style_text_color(title, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 20, 0);

    // Content area (scrollable)
    lv_obj_t* content = lv_obj_create(screen_sound);
    lv_obj_set_size(content, SCREEN_WIDTH - 10, SCREEN_HEIGHT - 45);
    lv_obj_set_pos(content, 5, 40);
    lv_obj_set_style_bg_color(content, lv_color_hex(0x0f0f23), 0);
    lv_obj_set_style_border_color(content, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_radius(content, 8, 0);
    lv_obj_set_style_pad_all(content, 10, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Sound ON/OFF row
    lv_obj_t* sound_row = lv_obj_create(content);
    lv_obj_set_size(sound_row, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(sound_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sound_row, 0, 0);
    lv_obj_set_style_pad_all(sound_row, 0, 0);

    lv_obj_t* sound_label = lv_label_create(sound_row);
    lv_label_set_text(sound_label, "Sound");
    lv_obj_set_style_text_color(sound_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(sound_label, &lv_font_montserrat_14, 0);
    lv_obj_align(sound_label, LV_ALIGN_LEFT_MID, 0, 0);

    sound_switch = lv_switch_create(sound_row);
    lv_obj_set_size(sound_switch, 50, 25);
    lv_obj_align(sound_switch, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(sound_switch, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sound_switch, lv_color_hex(0x00ff88), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sound_switch, lv_color_hex(0xffffff), LV_PART_KNOB);
    if (Config.notification.buzzerActive) {
        lv_obj_add_state(sound_switch, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(sound_switch, sound_switch_changed, LV_EVENT_VALUE_CHANGED, NULL);

    // Volume row
    lv_obj_t* vol_row = lv_obj_create(content);
    lv_obj_set_size(vol_row, lv_pct(100), 50);
    lv_obj_set_style_bg_opa(vol_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(vol_row, 0, 0);
    lv_obj_set_style_pad_all(vol_row, 0, 0);

    lv_obj_t* vol_title = lv_label_create(vol_row);
    lv_label_set_text(vol_title, "Volume");
    lv_obj_set_style_text_color(vol_title, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(vol_title, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(vol_title, 0, 0);

    volume_label = lv_label_create(vol_row);
    char vol_buf[8];
    snprintf(vol_buf, sizeof(vol_buf), "%d%%", Config.notification.volume);
    lv_label_set_text(volume_label, vol_buf);
    lv_obj_set_style_text_color(volume_label, lv_color_hex(0xff6b6b), 0);
    lv_obj_set_style_text_font(volume_label, &lv_font_montserrat_14, 0);
    lv_obj_align(volume_label, LV_ALIGN_TOP_RIGHT, 0, 0);

    volume_slider = lv_slider_create(vol_row);
    lv_obj_set_size(volume_slider, lv_pct(90), 15);
    lv_obj_align(volume_slider, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_slider_set_range(volume_slider, 0, 100);
    lv_slider_set_value(volume_slider, Config.notification.volume, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(volume_slider, lv_color_hex(0x444466), LV_PART_MAIN);  // Visible track
    lv_obj_set_style_bg_color(volume_slider, lv_color_hex(0xff6b6b), LV_PART_INDICATOR);  // Red filled
    lv_obj_set_style_bg_color(volume_slider, lv_color_hex(0xffffff), LV_PART_KNOB);  // White knob
    lv_obj_add_event_cb(volume_slider, volume_slider_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(volume_slider, volume_slider_released, LV_EVENT_RELEASED, NULL);

    // TX Beep row
    lv_obj_t* tx_row = lv_obj_create(content);
    lv_obj_set_size(tx_row, lv_pct(100), 35);
    lv_obj_set_style_bg_opa(tx_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tx_row, 0, 0);
    lv_obj_set_style_pad_all(tx_row, 0, 0);

    lv_obj_t* tx_label = lv_label_create(tx_row);
    lv_label_set_text(tx_label, "TX Beep");
    lv_obj_set_style_text_color(tx_label, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(tx_label, &lv_font_montserrat_14, 0);
    lv_obj_align(tx_label, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* tx_sw = lv_switch_create(tx_row);
    lv_obj_set_size(tx_sw, 45, 22);
    lv_obj_align(tx_sw, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(tx_sw, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(tx_sw, lv_color_hex(0x00ff88), LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (Config.notification.txBeep) lv_obj_add_state(tx_sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(tx_sw, tx_beep_changed, LV_EVENT_VALUE_CHANGED, NULL);

    // RX Message Beep row
    lv_obj_t* rx_row = lv_obj_create(content);
    lv_obj_set_size(rx_row, lv_pct(100), 35);
    lv_obj_set_style_bg_opa(rx_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rx_row, 0, 0);
    lv_obj_set_style_pad_all(rx_row, 0, 0);

    lv_obj_t* rx_label = lv_label_create(rx_row);
    lv_label_set_text(rx_label, "Message Beep");
    lv_obj_set_style_text_color(rx_label, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(rx_label, &lv_font_montserrat_14, 0);
    lv_obj_align(rx_label, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* rx_sw = lv_switch_create(rx_row);
    lv_obj_set_size(rx_sw, 45, 22);
    lv_obj_align(rx_sw, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(rx_sw, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(rx_sw, lv_color_hex(0x00ff88), LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (Config.notification.messageRxBeep) lv_obj_add_state(rx_sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(rx_sw, rx_beep_changed, LV_EVENT_VALUE_CHANGED, NULL);

    // Station Beep row
    lv_obj_t* sta_row = lv_obj_create(content);
    lv_obj_set_size(sta_row, lv_pct(100), 35);
    lv_obj_set_style_bg_opa(sta_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sta_row, 0, 0);
    lv_obj_set_style_pad_all(sta_row, 0, 0);

    lv_obj_t* sta_label = lv_label_create(sta_row);
    lv_label_set_text(sta_label, "Station Beep");
    lv_obj_set_style_text_color(sta_label, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(sta_label, &lv_font_montserrat_14, 0);
    lv_obj_align(sta_label, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* sta_sw = lv_switch_create(sta_row);
    lv_obj_set_size(sta_sw, 45, 22);
    lv_obj_align(sta_sw, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(sta_sw, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sta_sw, lv_color_hex(0x00ff88), LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (Config.notification.stationBeep) lv_obj_add_state(sta_sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sta_sw, station_beep_changed, LV_EVENT_VALUE_CHANGED, NULL);

    Serial.println("[LVGL] Sound settings screen created");
}

// WiFi switch callback
static void wifi_switch_changed(lv_event_t* e) {
    lv_obj_t* sw = lv_event_get_target(e);
    bool is_on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    extern uint32_t lastWiFiRetry;

    if (is_on) {
        // Turn WiFi ON (user re-enabled)
        Serial.println("[LVGL] WiFi: User enabled");
        WiFiUserDisabled = false;
        WiFiEcoMode = true;  // Set eco mode so checkWiFi() will retry
        // Force immediate WiFi retry on next checkWiFi() call
        lastWiFiRetry = 0;

        if (wifi_status_label) {
            lv_label_set_text(wifi_status_label, "Reconnecting...");
            lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xffa500), 0);
        }
    } else {
        // Turn WiFi OFF (user disabled manually - no retry)
        Serial.println("[LVGL] WiFi: User disabled");
        WiFiUserDisabled = true;
        esp_wifi_disconnect();
        delay(100);
        esp_wifi_stop();
        WiFiConnected = false;
        WiFiEcoMode = false;  // Not eco mode, completely off

        if (wifi_status_label) {
            lv_label_set_text(wifi_status_label, "OFF (disabled)");
            lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xff6b6b), 0);
        }

        // Update main screen WiFi label
        if (label_wifi) {
            lv_label_set_text(label_wifi, "WiFi: OFF");
            lv_obj_set_style_text_color(label_wifi, lv_color_hex(0xff6b6b), 0);
        }
    }
}

// Update WiFi screen status (called by timer)
static void update_wifi_screen_status() {
    if (!screen_wifi) return;

    // Update switch state
    if (wifi_switch) {
        if (WiFiUserDisabled) {
            lv_obj_clear_state(wifi_switch, LV_STATE_CHECKED);
        } else {
            lv_obj_add_state(wifi_switch, LV_STATE_CHECKED);
        }
    }

    // Update status label
    if (wifi_status_label) {
        if (WiFiUserDisabled) {
            lv_label_set_text(wifi_status_label, "Disabled");
            lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xff6b6b), 0);
        } else if (WiFiConnected) {
            lv_label_set_text(wifi_status_label, "Connected");
            lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0x00ff88), 0);
        } else if (WiFiEcoMode) {
            lv_label_set_text(wifi_status_label, "Eco (retry)");
            lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xffa500), 0);
        } else {
            lv_label_set_text(wifi_status_label, "Connecting...");
            lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xffa500), 0);
        }
    }

    // Update IP row visibility and value
    if (wifi_ip_row) {
        if (WiFiConnected && !WiFiUserDisabled) {
            lv_obj_clear_flag(wifi_ip_row, LV_OBJ_FLAG_HIDDEN);
            if (wifi_ip_label) {
                lv_label_set_text(wifi_ip_label, WiFi.localIP().toString().c_str());
            }
        } else {
            lv_obj_add_flag(wifi_ip_row, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Update RSSI row visibility and value
    if (wifi_rssi_row) {
        if (WiFiConnected && !WiFiUserDisabled) {
            lv_obj_clear_flag(wifi_rssi_row, LV_OBJ_FLAG_HIDDEN);
            if (wifi_rssi_label) {
                char rssi_buf[16];
                snprintf(rssi_buf, sizeof(rssi_buf), "%d dBm", WiFi.RSSI());
                lv_label_set_text(wifi_rssi_label, rssi_buf);
            }
        } else {
            lv_obj_add_flag(wifi_rssi_row, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// Timer callback for WiFi screen updates
static void wifi_screen_timer_cb(lv_timer_t* timer) {
    update_wifi_screen_status();
}

// Create the WiFi settings screen
static void create_wifi_screen() {
    screen_wifi = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_wifi, lv_color_hex(0x1a1a2e), 0);

    // Title bar (cyan like WiFi info)
    lv_obj_t* title_bar = lv_obj_create(screen_wifi);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, 35);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x00d4ff), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 5, 0);

    // Back button (with timer cleanup)
    lv_obj_t* btn_back = lv_btn_create(title_bar);
    lv_obj_set_size(btn_back, 60, 25);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x16213e), 0);
    lv_obj_add_event_cb(btn_back, btn_wifi_back_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "< BACK");
    lv_obj_center(lbl_back);

    // Title
    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "WiFi");
    lv_obj_set_style_text_color(title, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 20, 0);

    // Content area
    lv_obj_t* content = lv_obj_create(screen_wifi);
    lv_obj_set_size(content, SCREEN_WIDTH - 10, SCREEN_HEIGHT - 45);
    lv_obj_set_pos(content, 5, 40);
    lv_obj_set_style_bg_color(content, lv_color_hex(0x0f0f23), 0);
    lv_obj_set_style_border_color(content, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_radius(content, 8, 0);
    lv_obj_set_style_pad_all(content, 10, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // WiFi ON/OFF row
    lv_obj_t* wifi_row = lv_obj_create(content);
    lv_obj_set_size(wifi_row, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(wifi_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wifi_row, 0, 0);
    lv_obj_set_style_pad_all(wifi_row, 0, 0);

    lv_obj_t* wifi_label = lv_label_create(wifi_row);
    lv_label_set_text(wifi_label, "WiFi");
    lv_obj_set_style_text_color(wifi_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(wifi_label, &lv_font_montserrat_14, 0);
    lv_obj_align(wifi_label, LV_ALIGN_LEFT_MID, 0, 0);

    wifi_switch = lv_switch_create(wifi_row);
    lv_obj_set_size(wifi_switch, 50, 25);
    lv_obj_align(wifi_switch, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(wifi_switch, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(wifi_switch, lv_color_hex(0x00ff88), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(wifi_switch, lv_color_hex(0xffffff), LV_PART_KNOB);
    // Set switch state based on current WiFi state
    if (!WiFiUserDisabled) {
        lv_obj_add_state(wifi_switch, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(wifi_switch, wifi_switch_changed, LV_EVENT_VALUE_CHANGED, NULL);

    // Status row
    lv_obj_t* status_row = lv_obj_create(content);
    lv_obj_set_size(status_row, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(status_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(status_row, 0, 0);
    lv_obj_set_style_pad_all(status_row, 0, 0);

    lv_obj_t* status_title = lv_label_create(status_row);
    lv_label_set_text(status_title, "Status:");
    lv_obj_set_style_text_color(status_title, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(status_title, &lv_font_montserrat_14, 0);
    lv_obj_align(status_title, LV_ALIGN_LEFT_MID, 0, 0);

    wifi_status_label = lv_label_create(status_row);
    if (WiFiUserDisabled) {
        lv_label_set_text(wifi_status_label, "OFF (disabled)");
        lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xff6b6b), 0);
    } else if (WiFiConnected) {
        lv_label_set_text(wifi_status_label, "Connected");
        lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0x00ff88), 0);
    } else if (WiFiEcoMode) {
        lv_label_set_text(wifi_status_label, "Eco mode (retry)");
        lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xffa500), 0);
    } else {
        lv_label_set_text(wifi_status_label, "Connecting...");
        lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xffa500), 0);
    }
    lv_obj_set_style_text_font(wifi_status_label, &lv_font_montserrat_14, 0);
    lv_obj_align(wifi_status_label, LV_ALIGN_RIGHT_MID, 0, 0);

    // IP Address row (always create, hide if not connected)
    wifi_ip_row = lv_obj_create(content);
    lv_obj_set_size(wifi_ip_row, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(wifi_ip_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wifi_ip_row, 0, 0);
    lv_obj_set_style_pad_all(wifi_ip_row, 0, 0);

    lv_obj_t* ip_title = lv_label_create(wifi_ip_row);
    lv_label_set_text(ip_title, "IP:");
    lv_obj_set_style_text_color(ip_title, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(ip_title, &lv_font_montserrat_14, 0);
    lv_obj_align(ip_title, LV_ALIGN_LEFT_MID, 0, 0);

    wifi_ip_label = lv_label_create(wifi_ip_row);
    lv_label_set_text(wifi_ip_label, "---");
    lv_obj_set_style_text_color(wifi_ip_label, lv_color_hex(0x00d4ff), 0);
    lv_obj_set_style_text_font(wifi_ip_label, &lv_font_montserrat_14, 0);
    lv_obj_align(wifi_ip_label, LV_ALIGN_RIGHT_MID, 0, 0);

    // RSSI row (always create, hide if not connected)
    wifi_rssi_row = lv_obj_create(content);
    lv_obj_set_size(wifi_rssi_row, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(wifi_rssi_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wifi_rssi_row, 0, 0);
    lv_obj_set_style_pad_all(wifi_rssi_row, 0, 0);

    lv_obj_t* rssi_title = lv_label_create(wifi_rssi_row);
    lv_label_set_text(rssi_title, "Signal:");
    lv_obj_set_style_text_color(rssi_title, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(rssi_title, &lv_font_montserrat_14, 0);
    lv_obj_align(rssi_title, LV_ALIGN_LEFT_MID, 0, 0);

    wifi_rssi_label = lv_label_create(wifi_rssi_row);
    lv_label_set_text(wifi_rssi_label, "---");
    lv_obj_set_style_text_color(wifi_rssi_label, lv_color_hex(0x00d4ff), 0);
    lv_obj_set_style_text_font(wifi_rssi_label, &lv_font_montserrat_14, 0);
    lv_obj_align(wifi_rssi_label, LV_ALIGN_RIGHT_MID, 0, 0);

    // Initial update
    update_wifi_screen_status();

    // Start update timer (every 1 second)
    wifi_update_timer = lv_timer_create(wifi_screen_timer_cb, 1000, NULL);

    Serial.println("[LVGL] WiFi settings screen created");
}

// Update Bluetooth screen status (called by timer)
static void update_bluetooth_screen_status() {
    if (!screen_bluetooth) return;

    // Update switch state
    if (bluetooth_switch) {
        if (bluetoothActive) {
            lv_obj_add_state(bluetooth_switch, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(bluetooth_switch, LV_STATE_CHECKED);
        }
    }

    // Update status label
    if (bluetooth_status_label) {
        if (!bluetoothActive) {
            lv_label_set_text(bluetooth_status_label, "OFF");
            lv_obj_set_style_text_color(bluetooth_status_label, lv_color_hex(0xff6b6b), 0);
        } else if (bluetoothConnected) {
            lv_label_set_text(bluetooth_status_label, "Connected");
            lv_obj_set_style_text_color(bluetooth_status_label, lv_color_hex(0x00ff88), 0);
        } else {
            lv_label_set_text(bluetooth_status_label, "Waiting...");
            lv_obj_set_style_text_color(bluetooth_status_label, lv_color_hex(0xffa500), 0);
        }
    }

    // Update device row visibility and content
    if (bluetooth_device_row) {
        if (bluetoothActive && bluetoothConnected) {
            lv_obj_clear_flag(bluetooth_device_row, LV_OBJ_FLAG_HIDDEN);
            if (bluetooth_device_label) {
                // Prefer name over address
                String name = BLE_Utils::getConnectedDeviceName();
                if (name.length() > 0) {
                    lv_label_set_text(bluetooth_device_label, name.c_str());
                } else {
                    String addr = BLE_Utils::getConnectedDeviceAddress();
                    lv_label_set_text(bluetooth_device_label, addr.c_str());
                }
            }
        } else {
            lv_obj_add_flag(bluetooth_device_row, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// Timer callback for Bluetooth screen updates
static void bluetooth_screen_timer_cb(lv_timer_t* timer) {
    update_bluetooth_screen_status();
}

// Back button for Bluetooth screen (stops timer)
static void btn_bluetooth_back_clicked(lv_event_t* e) {
    Serial.println("[LVGL] Bluetooth BACK button pressed");
    if (bluetooth_update_timer) {
        lv_timer_del(bluetooth_update_timer);
        bluetooth_update_timer = nullptr;
    }
    lv_scr_load_anim(screen_setup, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
}

// Bluetooth switch callback
static void bluetooth_switch_changed(lv_event_t* e) {
    lv_obj_t* sw = lv_event_get_target(e);
    bool is_on = lv_obj_has_state(sw, LV_STATE_CHECKED);

    if (is_on) {
        // Turn Bluetooth ON
        Serial.println("[LVGL] Bluetooth: Turning ON");
        bluetoothActive = true;
        if (Config.bluetooth.useBLE) {
            BLE_Utils::setup();
        }

        if (bluetooth_status_label) {
            lv_label_set_text(bluetooth_status_label, "ON (waiting)");
            lv_obj_set_style_text_color(bluetooth_status_label, lv_color_hex(0xffa500), 0);
        }
    } else {
        // Turn Bluetooth OFF
        Serial.println("[LVGL] Bluetooth: Turning OFF");
        bluetoothActive = false;
        if (Config.bluetooth.useBLE) {
            BLE_Utils::stop();
        }

        if (bluetooth_status_label) {
            lv_label_set_text(bluetooth_status_label, "OFF");
            lv_obj_set_style_text_color(bluetooth_status_label, lv_color_hex(0xff6b6b), 0);
        }
    }
}

// Create the Bluetooth settings screen
static void create_bluetooth_screen() {
    screen_bluetooth = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_bluetooth, lv_color_hex(0x1a1a2e), 0);

    // Title bar (purple for Bluetooth)
    lv_obj_t* title_bar = lv_obj_create(screen_bluetooth);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, 35);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0xc792ea), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 5, 0);

    // Back button (with timer cleanup)
    lv_obj_t* btn_back = lv_btn_create(title_bar);
    lv_obj_set_size(btn_back, 60, 25);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x16213e), 0);
    lv_obj_add_event_cb(btn_back, btn_bluetooth_back_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "< BACK");
    lv_obj_center(lbl_back);

    // Title
    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "Bluetooth");
    lv_obj_set_style_text_color(title, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 20, 0);

    // Content area
    lv_obj_t* content = lv_obj_create(screen_bluetooth);
    lv_obj_set_size(content, SCREEN_WIDTH - 10, SCREEN_HEIGHT - 45);
    lv_obj_set_pos(content, 5, 40);
    lv_obj_set_style_bg_color(content, lv_color_hex(0x0f0f23), 0);
    lv_obj_set_style_border_color(content, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_radius(content, 8, 0);
    lv_obj_set_style_pad_all(content, 10, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Bluetooth ON/OFF row
    lv_obj_t* bt_row = lv_obj_create(content);
    lv_obj_set_size(bt_row, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(bt_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bt_row, 0, 0);
    lv_obj_set_style_pad_all(bt_row, 0, 0);

    lv_obj_t* bt_label = lv_label_create(bt_row);
    lv_label_set_text(bt_label, "Bluetooth");
    lv_obj_set_style_text_color(bt_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(bt_label, &lv_font_montserrat_14, 0);
    lv_obj_align(bt_label, LV_ALIGN_LEFT_MID, 0, 0);

    bluetooth_switch = lv_switch_create(bt_row);
    lv_obj_set_size(bluetooth_switch, 50, 25);
    lv_obj_align(bluetooth_switch, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(bluetooth_switch, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bluetooth_switch, lv_color_hex(0xc792ea), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(bluetooth_switch, lv_color_hex(0xffffff), LV_PART_KNOB);
    // Set switch state
    if (bluetoothActive) {
        lv_obj_add_state(bluetooth_switch, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(bluetooth_switch, bluetooth_switch_changed, LV_EVENT_VALUE_CHANGED, NULL);

    // Status row
    lv_obj_t* status_row = lv_obj_create(content);
    lv_obj_set_size(status_row, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(status_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(status_row, 0, 0);
    lv_obj_set_style_pad_all(status_row, 0, 0);

    lv_obj_t* status_title = lv_label_create(status_row);
    lv_label_set_text(status_title, "Status:");
    lv_obj_set_style_text_color(status_title, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(status_title, &lv_font_montserrat_14, 0);
    lv_obj_align(status_title, LV_ALIGN_LEFT_MID, 0, 0);

    bluetooth_status_label = lv_label_create(status_row);
    if (!bluetoothActive) {
        lv_label_set_text(bluetooth_status_label, "OFF");
        lv_obj_set_style_text_color(bluetooth_status_label, lv_color_hex(0xff6b6b), 0);
    } else if (bluetoothConnected) {
        lv_label_set_text(bluetooth_status_label, "Connected");
        lv_obj_set_style_text_color(bluetooth_status_label, lv_color_hex(0x00ff88), 0);
    } else {
        lv_label_set_text(bluetooth_status_label, "ON (waiting)");
        lv_obj_set_style_text_color(bluetooth_status_label, lv_color_hex(0xffa500), 0);
    }
    lv_obj_set_style_text_font(bluetooth_status_label, &lv_font_montserrat_14, 0);
    lv_obj_align(bluetooth_status_label, LV_ALIGN_RIGHT_MID, 0, 0);

    // Type row
    lv_obj_t* type_row = lv_obj_create(content);
    lv_obj_set_size(type_row, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(type_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(type_row, 0, 0);
    lv_obj_set_style_pad_all(type_row, 0, 0);

    lv_obj_t* type_title = lv_label_create(type_row);
    lv_label_set_text(type_title, "Type:");
    lv_obj_set_style_text_color(type_title, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(type_title, &lv_font_montserrat_14, 0);
    lv_obj_align(type_title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* type_label = lv_label_create(type_row);
    lv_label_set_text(type_label, Config.bluetooth.useBLE ? "BLE" : "Classic");
    lv_obj_set_style_text_color(type_label, lv_color_hex(0xc792ea), 0);
    lv_obj_set_style_text_font(type_label, &lv_font_montserrat_14, 0);
    lv_obj_align(type_label, LV_ALIGN_RIGHT_MID, 0, 0);

    // Device row (visible only when connected)
    bluetooth_device_row = lv_obj_create(content);
    lv_obj_set_size(bluetooth_device_row, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(bluetooth_device_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bluetooth_device_row, 0, 0);
    lv_obj_set_style_pad_all(bluetooth_device_row, 0, 0);

    lv_obj_t* device_title = lv_label_create(bluetooth_device_row);
    lv_label_set_text(device_title, "Device:");
    lv_obj_set_style_text_color(device_title, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(device_title, &lv_font_montserrat_14, 0);
    lv_obj_align(device_title, LV_ALIGN_LEFT_MID, 0, 0);

    bluetooth_device_label = lv_label_create(bluetooth_device_row);
    if (bluetoothConnected) {
        // Prefer name over address
        String name = BLE_Utils::getConnectedDeviceName();
        if (name.length() > 0) {
            lv_label_set_text(bluetooth_device_label, name.c_str());
        } else {
            String addr = BLE_Utils::getConnectedDeviceAddress();
            lv_label_set_text(bluetooth_device_label, addr.c_str());
        }
    } else {
        lv_label_set_text(bluetooth_device_label, "");
    }
    lv_obj_set_style_text_color(bluetooth_device_label, lv_color_hex(0xc792ea), 0);
    lv_obj_set_style_text_font(bluetooth_device_label, &lv_font_montserrat_14, 0);
    lv_obj_align(bluetooth_device_label, LV_ALIGN_RIGHT_MID, 0, 0);

    // Hide device row if not connected
    if (!bluetoothActive || !bluetoothConnected) {
        lv_obj_add_flag(bluetooth_device_row, LV_OBJ_FLAG_HIDDEN);
    }

    // Start update timer (every 1 second)
    bluetooth_update_timer = lv_timer_create(bluetooth_screen_timer_cb, 1000, NULL);

    Serial.println("[LVGL] Bluetooth settings screen created");
}

// Messages screen variables
static lv_obj_t* msg_tabview = nullptr;
static lv_obj_t* list_aprs_global = nullptr;
static lv_obj_t* list_wlnk_global = nullptr;
static lv_obj_t* list_contacts_global = nullptr;
static int current_msg_type = 0;  // 0 = APRS, 1 = Winlink, 2 = Contacts

// Compose screen variables (declared early for use in callbacks)
static lv_obj_t* screen_compose = nullptr;
static lv_obj_t* compose_to_input = nullptr;
static lv_obj_t* compose_msg_input = nullptr;
static lv_obj_t* compose_keyboard = nullptr;
static lv_obj_t* current_focused_input = nullptr;
static bool compose_screen_active = false;

// Show message detail popup
static lv_obj_t* detail_msgbox = nullptr;

static void detail_msgbox_deleted_cb(lv_event_t* e) {
    detail_msgbox = nullptr;
}

static void show_message_detail(const char* msg) {
    // Close previous msgbox if it still exists
    if (detail_msgbox && lv_obj_is_valid(detail_msgbox)) {
        lv_msgbox_close(detail_msgbox);
    }
    detail_msgbox = nullptr;

    detail_msgbox = lv_msgbox_create(NULL, "Message", msg, NULL, true);
    lv_obj_set_style_bg_color(detail_msgbox, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_text_color(detail_msgbox, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_width(detail_msgbox, SCREEN_WIDTH - 40);
    lv_obj_center(detail_msgbox);

    // Track when msgbox is deleted (any way it gets destroyed)
    lv_obj_add_event_cb(detail_msgbox, detail_msgbox_deleted_cb, LV_EVENT_DELETE, NULL);
}

// Forward declaration for message list
static void populate_msg_list(lv_obj_t* list, int type);

// Confirmation popup for delete operations
static lv_obj_t* confirm_msgbox = nullptr;
static int pending_delete_msg_index = -1;  // Index of message to delete (-1 = all)
static bool msg_longpress_handled = false;

static void confirm_delete_cb(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    const char* btn_text = lv_msgbox_get_active_btn_text(confirm_msgbox);

    if (btn_text && strcmp(btn_text, "Yes") == 0) {
        if (pending_delete_msg_index == -1) {
            // Delete all messages
            Serial.printf("[LVGL] Confirmed: Delete all messages type %d\n", current_msg_type);
            MSG_Utils::deleteFile(current_msg_type);
        } else {
            // Delete single message
            Serial.printf("[LVGL] Confirmed: Delete message %d type %d\n", pending_delete_msg_index, current_msg_type);
            MSG_Utils::deleteMessageByIndex(current_msg_type, pending_delete_msg_index);
        }

        // Refresh the current list
        if (current_msg_type == 0 && list_aprs_global) {
            populate_msg_list(list_aprs_global, 0);
        } else if (current_msg_type == 1 && list_wlnk_global) {
            populate_msg_list(list_wlnk_global, 1);
        }
    }

    lv_msgbox_close(confirm_msgbox);
    confirm_msgbox = nullptr;
    pending_delete_msg_index = -1;
}

static void show_delete_confirmation(const char* message, int msg_index) {
    pending_delete_msg_index = msg_index;

    static const char* btns[] = {"Yes", "No", ""};
    confirm_msgbox = lv_msgbox_create(NULL, "Confirmation", message, btns, false);
    lv_obj_set_style_bg_color(confirm_msgbox, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_text_color(confirm_msgbox, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_width(confirm_msgbox, 220);
    lv_obj_center(confirm_msgbox);
    lv_obj_add_event_cb(confirm_msgbox, confirm_delete_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

// Message item click callback
static void msg_item_clicked(lv_event_t* e) {
    if (msg_longpress_handled) {
        msg_longpress_handled = false;
        return;
    }

    lv_obj_t* btn = lv_event_get_target(e);
    lv_obj_t* label = lv_obj_get_child(btn, 0);
    if (label) {
        const char* text = lv_label_get_text(label);
        show_message_detail(text);
    }
}

// Message item long-press - delete single message
static void msg_item_longpress(lv_event_t* e) {
    int msg_index = (int)(intptr_t)lv_event_get_user_data(e);
    Serial.printf("[LVGL] Message long-press: index %d\n", msg_index);
    msg_longpress_handled = true;
    show_delete_confirmation("Delete this message?", msg_index);
}

// Populate message list
static void populate_msg_list(lv_obj_t* list, int type) {
    lv_obj_clean(list);

    if (type == 0) {
        // APRS messages
        MSG_Utils::loadMessagesFromMemory(0);
        std::vector<String>& messages = MSG_Utils::getLoadedAPRSMessages();

        if (messages.size() == 0) {
            lv_obj_t* empty = lv_label_create(list);
            lv_label_set_text(empty, "No APRS messages");
            lv_obj_set_style_text_color(empty, lv_color_hex(0x888888), 0);
        } else {
            for (size_t i = 0; i < messages.size(); i++) {
                lv_obj_t* btn = lv_list_add_btn(list, LV_SYMBOL_ENVELOPE, messages[i].c_str());
                lv_obj_add_event_cb(btn, msg_item_clicked, LV_EVENT_CLICKED, NULL);
                lv_obj_add_event_cb(btn, msg_item_longpress, LV_EVENT_LONG_PRESSED, (void*)(intptr_t)i);
            }
        }
    } else {
        // Winlink messages
        MSG_Utils::loadMessagesFromMemory(1);
        std::vector<String>& messages = MSG_Utils::getLoadedWLNKMails();

        if (messages.size() == 0) {
            lv_obj_t* empty = lv_label_create(list);
            lv_label_set_text(empty, "No Winlink mails");
            lv_obj_set_style_text_color(empty, lv_color_hex(0x888888), 0);
        } else {
            for (size_t i = 0; i < messages.size(); i++) {
                lv_obj_t* btn = lv_list_add_btn(list, LV_SYMBOL_ENVELOPE, messages[i].c_str());
                lv_obj_add_event_cb(btn, msg_item_clicked, LV_EVENT_CLICKED, NULL);
                lv_obj_add_event_cb(btn, msg_item_longpress, LV_EVENT_LONG_PRESSED, (void*)(intptr_t)i);
            }
        }
    }
}

// Contact add/edit screen variables
static lv_obj_t* screen_contact_edit = nullptr;
static lv_obj_t* contact_callsign_input = nullptr;
static lv_obj_t* contact_name_input = nullptr;
static lv_obj_t* contact_comment_input = nullptr;
static lv_obj_t* contact_edit_keyboard = nullptr;
static lv_obj_t* contact_current_input = nullptr;
static String editing_contact_callsign = "";  // Empty = adding new, non-empty = editing

// Forward declarations for contacts
static void populate_contacts_list(lv_obj_t* list);
static void show_contact_edit_screen(const Contact* contact);

// Forward declaration
static void create_compose_screen();

// Flag to prevent click after long-press
static bool contact_longpress_handled = false;

// Contact item clicked - open compose with callsign pre-filled
static void contact_item_clicked(lv_event_t* e) {
    // Skip if this click follows a long-press
    if (contact_longpress_handled) {
        contact_longpress_handled = false;
        return;
    }

    Contact* contact = (Contact*)lv_event_get_user_data(e);
    if (!contact) return;

    Serial.printf("[LVGL] Contact clicked: %s - opening compose\n", contact->callsign.c_str());

    // Create compose screen and pre-fill recipient
    create_compose_screen();
    compose_screen_active = true;

    // Pre-fill the To field with contact's callsign
    lv_textarea_set_text(compose_to_input, contact->callsign.c_str());

    // Focus on message input (skip To field since it's already filled)
    current_focused_input = compose_msg_input;
    lv_keyboard_set_textarea(compose_keyboard, compose_msg_input);

    lv_scr_load_anim(screen_compose, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
}

// Contact long-press - open edit screen
static void contact_item_longpress(lv_event_t* e) {
    Contact* contact = (Contact*)lv_event_get_user_data(e);
    if (!contact) return;
    Serial.printf("[LVGL] Contact long-press: %s\n", contact->callsign.c_str());
    contact_longpress_handled = true;  // Block the following click event
    show_contact_edit_screen(contact);
}

// Populate contacts list
static void populate_contacts_list(lv_obj_t* list) {
    lv_obj_clean(list);

    std::vector<Contact> contacts = STORAGE_Utils::loadContacts();

    if (contacts.size() == 0) {
        lv_obj_t* empty = lv_label_create(list);
        lv_label_set_text(empty, "No contacts\nTap + to add");
        lv_obj_set_style_text_color(empty, lv_color_hex(0x888888), 0);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
    } else {
        for (size_t i = 0; i < contacts.size(); i++) {
            Contact* c = STORAGE_Utils::findContact(contacts[i].callsign);
            String display = contacts[i].callsign;
            if (contacts[i].name.length() > 0) {
                display += " - " + contacts[i].name;
            }
            if (contacts[i].comment.length() > 0) {
                display += "\n" + contacts[i].comment;
            }
            lv_obj_t* btn = lv_list_add_btn(list, LV_SYMBOL_CALL, display.c_str());
            lv_obj_add_event_cb(btn, contact_item_clicked, LV_EVENT_CLICKED, c);
            lv_obj_add_event_cb(btn, contact_item_longpress, LV_EVENT_LONG_PRESSED, c);
        }
    }
}

// Contact edit screen callbacks
static void contact_edit_input_focused(lv_event_t* e) {
    lv_obj_t* ta = lv_event_get_target(e);
    contact_current_input = ta;
    if (contact_edit_keyboard) {
        lv_keyboard_set_textarea(contact_edit_keyboard, ta);
    }
}

static void contact_edit_keyboard_event(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_obj_add_flag(contact_edit_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

static void btn_contact_save_clicked(lv_event_t* e) {
    const char* callsign = lv_textarea_get_text(contact_callsign_input);
    const char* name = lv_textarea_get_text(contact_name_input);
    const char* comment = lv_textarea_get_text(contact_comment_input);

    if (strlen(callsign) == 0) {
        show_message_detail("Callsign required!");
        return;
    }

    Contact contact;
    contact.callsign = String(callsign);
    contact.callsign.toUpperCase();
    contact.name = String(name);
    contact.comment = String(comment);

    bool success;
    if (editing_contact_callsign.length() > 0) {
        // Editing existing contact
        success = STORAGE_Utils::updateContact(editing_contact_callsign, contact);
    } else {
        // Adding new contact
        success = STORAGE_Utils::addContact(contact);
    }

    if (success) {
        Serial.printf("[LVGL] Contact saved: %s\n", contact.callsign.c_str());
        // Go back to messages screen
        lv_scr_load(screen_msg);
        // Refresh contacts list
        if (list_contacts_global) {
            populate_contacts_list(list_contacts_global);
        }
    } else {
        show_message_detail("Failed to save contact\n(duplicate callsign?)");
    }
}

static void btn_contact_delete_clicked(lv_event_t* e) {
    if (editing_contact_callsign.length() > 0) {
        bool success = STORAGE_Utils::removeContact(editing_contact_callsign);
        if (success) {
            Serial.printf("[LVGL] Contact deleted: %s\n", editing_contact_callsign.c_str());
            lv_scr_load(screen_msg);
            if (list_contacts_global) {
                populate_contacts_list(list_contacts_global);
            }
        }
    }
}

static void btn_contact_cancel_clicked(lv_event_t* e) {
    lv_scr_load(screen_msg);
}

static void btn_contact_kb_toggle_clicked(lv_event_t* e) {
    if (lv_obj_has_flag(contact_edit_keyboard, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(contact_edit_keyboard, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(contact_edit_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

// Show contact edit screen
static void show_contact_edit_screen(const Contact* contact) {
    if (contact) {
        editing_contact_callsign = contact->callsign;
    } else {
        editing_contact_callsign = "";
    }

    // Create screen if not exists
    if (!screen_contact_edit) {
        screen_contact_edit = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen_contact_edit, lv_color_hex(0x0f0f23), 0);

        // Title bar
        lv_obj_t* title_bar = lv_obj_create(screen_contact_edit);
        lv_obj_set_size(title_bar, SCREEN_WIDTH, 35);
        lv_obj_set_pos(title_bar, 0, 0);
        lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x1a1a2e), 0);
        lv_obj_set_style_border_width(title_bar, 0, 0);
        lv_obj_set_style_pad_all(title_bar, 5, 0);

        // Back button
        lv_obj_t* btn_back = lv_btn_create(title_bar);
        lv_obj_set_size(btn_back, 30, 25);
        lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x444444), 0);
        lv_obj_add_event_cb(btn_back, btn_contact_cancel_clicked, LV_EVENT_CLICKED, NULL);
        lv_obj_t* lbl_back = lv_label_create(btn_back);
        lv_label_set_text(lbl_back, LV_SYMBOL_LEFT);
        lv_obj_center(lbl_back);

        // Title
        lv_obj_t* title = lv_label_create(title_bar);
        lv_label_set_text(title, "Contact");
        lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

        // Save button
        lv_obj_t* btn_save = lv_btn_create(title_bar);
        lv_obj_set_size(btn_save, 40, 25);
        lv_obj_align(btn_save, LV_ALIGN_RIGHT_MID, -50, 0);
        lv_obj_set_style_bg_color(btn_save, lv_color_hex(0x00aa55), 0);
        lv_obj_add_event_cb(btn_save, btn_contact_save_clicked, LV_EVENT_CLICKED, NULL);
        lv_obj_t* lbl_save = lv_label_create(btn_save);
        lv_label_set_text(lbl_save, LV_SYMBOL_OK);
        lv_obj_center(lbl_save);

        // Delete button
        lv_obj_t* btn_del = lv_btn_create(title_bar);
        lv_obj_set_size(btn_del, 40, 25);
        lv_obj_align(btn_del, LV_ALIGN_RIGHT_MID, -5, 0);
        lv_obj_set_style_bg_color(btn_del, lv_color_hex(0xff4444), 0);
        lv_obj_add_event_cb(btn_del, btn_contact_delete_clicked, LV_EVENT_CLICKED, NULL);
        lv_obj_t* lbl_del = lv_label_create(btn_del);
        lv_label_set_text(lbl_del, LV_SYMBOL_TRASH);
        lv_obj_center(lbl_del);

        // Form container (scrollable for 3 fields)
        lv_obj_t* form = lv_obj_create(screen_contact_edit);
        lv_obj_set_size(form, SCREEN_WIDTH - 10, 160);
        lv_obj_set_pos(form, 5, 40);
        lv_obj_set_scrollbar_mode(form, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_set_style_bg_color(form, lv_color_hex(0x1a1a2e), 0);
        lv_obj_set_style_border_width(form, 0, 0);
        lv_obj_set_style_pad_all(form, 5, 0);
        lv_obj_set_flex_flow(form, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(form, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

        // Callsign input
        lv_obj_t* lbl_call = lv_label_create(form);
        lv_label_set_text(lbl_call, "Callsign:");
        lv_obj_set_style_text_color(lbl_call, lv_color_hex(0x00d4ff), 0);

        contact_callsign_input = lv_textarea_create(form);
        lv_obj_set_size(contact_callsign_input, lv_pct(100), 30);
        lv_textarea_set_one_line(contact_callsign_input, true);
        lv_textarea_set_placeholder_text(contact_callsign_input, "F4ABC-9");
        lv_obj_add_event_cb(contact_callsign_input, contact_edit_input_focused, LV_EVENT_FOCUSED, NULL);

        // Name input
        lv_obj_t* lbl_name = lv_label_create(form);
        lv_label_set_text(lbl_name, "Name:");
        lv_obj_set_style_text_color(lbl_name, lv_color_hex(0x00d4ff), 0);

        contact_name_input = lv_textarea_create(form);
        lv_obj_set_size(contact_name_input, lv_pct(100), 30);
        lv_textarea_set_one_line(contact_name_input, true);
        lv_textarea_set_placeholder_text(contact_name_input, "Jean");
        lv_obj_add_event_cb(contact_name_input, contact_edit_input_focused, LV_EVENT_FOCUSED, NULL);

        // Comment input
        lv_obj_t* lbl_comment = lv_label_create(form);
        lv_label_set_text(lbl_comment, "Note:");
        lv_obj_set_style_text_color(lbl_comment, lv_color_hex(0x00d4ff), 0);

        contact_comment_input = lv_textarea_create(form);
        lv_obj_set_size(contact_comment_input, lv_pct(100), 30);
        lv_textarea_set_one_line(contact_comment_input, true);
        lv_textarea_set_placeholder_text(contact_comment_input, "Paris");
        lv_obj_add_event_cb(contact_comment_input, contact_edit_input_focused, LV_EVENT_FOCUSED, NULL);

        // Keyboard toggle button
        lv_obj_t* btn_kb = lv_btn_create(screen_contact_edit);
        lv_obj_set_size(btn_kb, 40, 30);
        lv_obj_set_pos(btn_kb, SCREEN_WIDTH - 50, 165);
        lv_obj_set_style_bg_color(btn_kb, lv_color_hex(0x555555), 0);
        lv_obj_add_event_cb(btn_kb, btn_contact_kb_toggle_clicked, LV_EVENT_CLICKED, NULL);
        lv_obj_t* lbl_kb = lv_label_create(btn_kb);
        lv_label_set_text(lbl_kb, LV_SYMBOL_KEYBOARD);
        lv_obj_center(lbl_kb);

        // Virtual keyboard (hidden by default)
        contact_edit_keyboard = lv_keyboard_create(screen_contact_edit);
        lv_obj_set_size(contact_edit_keyboard, SCREEN_WIDTH, 100);
        lv_obj_align(contact_edit_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_add_flag(contact_edit_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_event_cb(contact_edit_keyboard, contact_edit_keyboard_event, LV_EVENT_ALL, NULL);
    }

    // Fill form with contact data
    if (contact) {
        lv_textarea_set_text(contact_callsign_input, contact->callsign.c_str());
        lv_textarea_set_text(contact_name_input, contact->name.c_str());
        lv_textarea_set_text(contact_comment_input, contact->comment.c_str());
    } else {
        lv_textarea_set_text(contact_callsign_input, "");
        lv_textarea_set_text(contact_name_input, "");
        lv_textarea_set_text(contact_comment_input, "");
    }

    lv_scr_load(screen_contact_edit);
}

// Tab changed callback
static void msg_tab_changed(lv_event_t* e) {
    lv_obj_t* tabview = lv_event_get_target(e);
    uint16_t tab_idx = lv_tabview_get_tab_act(tabview);

    // Validate tab index (only 0, 1, 2 are valid)
    if (tab_idx > 2) {
        Serial.printf("[LVGL] Invalid tab index %d, ignoring\n", tab_idx);
        return;
    }

    current_msg_type = (int)tab_idx;
    Serial.printf("[LVGL] Messages tab changed to %d\n", current_msg_type);

    // Refresh contacts list when tab is selected
    if (current_msg_type == 2 && list_contacts_global) {
        populate_contacts_list(list_contacts_global);
    }
}

// Delete all messages callback
static void btn_delete_msgs_clicked(lv_event_t* e) {
    Serial.printf("[LVGL] Delete all button pressed, type %d\n", current_msg_type);

    // Don't delete contacts with this button
    if (current_msg_type == 2) {
        return;
    }

    const char* msg = (current_msg_type == 0) ? "Delete all APRS messages?" : "Delete all Winlink mails?";
    show_delete_confirmation(msg, -1);  // -1 means delete all
}

// Compose screen callbacks
static void compose_keyboard_event(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_obj_add_flag(compose_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

static void compose_input_focused(lv_event_t* e) {
    lv_obj_t* ta = lv_event_get_target(e);
    current_focused_input = ta;
    lv_keyboard_set_textarea(compose_keyboard, ta);
    // Don't show keyboard automatically - user can toggle with button
}

static void btn_send_msg_clicked(lv_event_t* e) {
    const char* to = lv_textarea_get_text(compose_to_input);
    const char* msg = lv_textarea_get_text(compose_msg_input);

    if (strlen(to) > 0 && strlen(msg) > 0) {
        Serial.printf("[LVGL] Sending message to %s: %s\n", to, msg);
        MSG_Utils::addToOutputBuffer(1, String(to), String(msg));

        // Save to outbox
        String outboxPath = STORAGE_Utils::getOutboxPath();
        if (outboxPath.length() > 0) {
            // Create filename with timestamp
            String filename = outboxPath + "/" + String(to) + "_" + String(millis()) + ".txt";
            File file = STORAGE_Utils::openFile(filename, FILE_WRITE);
            if (file) {
                file.printf("To: %s\nMsg: %s\nTime: %lu\n", to, msg, millis());
                file.close();
                Serial.printf("[LVGL] Message saved to outbox: %s\n", filename.c_str());
            }
        }

        // Show confirmation popup
        LVGL_UI::showTxPacket(msg);

        // Clear inputs and go back
        lv_textarea_set_text(compose_to_input, "");
        lv_textarea_set_text(compose_msg_input, "");
        compose_screen_active = false;
        lv_scr_load_anim(screen_msg, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
    }
}

static void btn_compose_back_clicked(lv_event_t* e) {
    compose_screen_active = false;
    lv_scr_load_anim(screen_msg, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
}

static void create_compose_screen() {
    if (screen_compose) return;

    screen_compose = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_compose, lv_color_hex(0x1a1a2e), 0);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen_compose);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, 35);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x00ff88), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 5, 0);

    // Back button
    lv_obj_t* btn_back = lv_btn_create(title_bar);
    lv_obj_set_size(btn_back, 60, 25);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x16213e), 0);
    lv_obj_add_event_cb(btn_back, btn_compose_back_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "< BACK");
    lv_obj_center(lbl_back);

    // Title
    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "Compose Message");
    lv_obj_set_style_text_color(title, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 30, 0);

    // Send button
    lv_obj_t* btn_send = lv_btn_create(title_bar);
    lv_obj_set_size(btn_send, 60, 25);
    lv_obj_align(btn_send, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_bg_color(btn_send, lv_color_hex(0x16213e), 0);
    lv_obj_add_event_cb(btn_send, btn_send_msg_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_send = lv_label_create(btn_send);
    lv_label_set_text(lbl_send, "SEND");
    lv_obj_center(lbl_send);

    // "To:" label and input
    lv_obj_t* lbl_to = lv_label_create(screen_compose);
    lv_label_set_text(lbl_to, "To:");
    lv_obj_set_pos(lbl_to, 10, 45);
    lv_obj_set_style_text_color(lbl_to, lv_color_hex(0xffffff), 0);

    compose_to_input = lv_textarea_create(screen_compose);
    lv_obj_set_size(compose_to_input, SCREEN_WIDTH - 50, 30);
    lv_obj_set_pos(compose_to_input, 40, 40);
    lv_textarea_set_one_line(compose_to_input, true);
    lv_textarea_set_placeholder_text(compose_to_input, "CALLSIGN-SSID");
    lv_obj_add_event_cb(compose_to_input, compose_input_focused, LV_EVENT_FOCUSED, NULL);

    // "Msg:" label and input
    lv_obj_t* lbl_msg = lv_label_create(screen_compose);
    lv_label_set_text(lbl_msg, "Msg:");
    lv_obj_set_pos(lbl_msg, 10, 80);
    lv_obj_set_style_text_color(lbl_msg, lv_color_hex(0xffffff), 0);

    compose_msg_input = lv_textarea_create(screen_compose);
    lv_obj_set_size(compose_msg_input, SCREEN_WIDTH - 20, 50);
    lv_obj_set_pos(compose_msg_input, 10, 95);
    lv_textarea_set_placeholder_text(compose_msg_input, "Your message...");
    lv_obj_add_event_cb(compose_msg_input, compose_input_focused, LV_EVENT_FOCUSED, NULL);

    // Virtual Keyboard (hidden by default - physical keyboard is primary)
    compose_keyboard = lv_keyboard_create(screen_compose);
    lv_obj_set_size(compose_keyboard, SCREEN_WIDTH, 90);
    lv_obj_align(compose_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(compose_keyboard, compose_to_input);
    lv_obj_add_event_cb(compose_keyboard, compose_keyboard_event, LV_EVENT_ALL, NULL);
    lv_obj_add_flag(compose_keyboard, LV_OBJ_FLAG_HIDDEN);  // Hidden by default

    // Toggle keyboard button (bottom right)
    lv_obj_t* btn_kbd = lv_btn_create(screen_compose);
    lv_obj_set_size(btn_kbd, 40, 30);
    lv_obj_align(btn_kbd, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_set_style_bg_color(btn_kbd, lv_color_hex(0x444466), 0);
    lv_obj_add_event_cb(btn_kbd, [](lv_event_t* e) {
        if (lv_obj_has_flag(compose_keyboard, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_clear_flag(compose_keyboard, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(compose_keyboard, LV_OBJ_FLAG_HIDDEN);
        }
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_kbd = lv_label_create(btn_kbd);
    lv_label_set_text(lbl_kbd, LV_SYMBOL_KEYBOARD);
    lv_obj_center(lbl_kbd);

    Serial.println("[LVGL] Compose screen created");
}

// Caps Lock and Symbol Lock state for physical keyboard
static bool capsLockActive = false;
static bool symbolLockActive = false;
static uint32_t lastShiftTime = 0;
static uint32_t lastSymbolTime = 0;
static const uint32_t DOUBLE_TAP_MS = 400;  // Double tap window

// T-Deck keyboard special key codes (adjust if needed)
// These may vary depending on firmware - check serial output
static const char KEY_SHIFT = 0x01;   // Shift key alone
static const char KEY_SYMBOL = 0x02;  // Symbol key alone ($)

// Symbol mapping for T-Deck keyboard (number row -> symbols)
static char getSymbolChar(char key) {
    switch(key) {
        case '1': return '!';
        case '2': return '@';
        case '3': return '#';
        case '4': return '$';
        case '5': return '%';
        case '6': return '^';
        case '7': return '&';
        case '8': return '*';
        case '9': return '(';
        case '0': return ')';
        case 'q': return '#';
        case 'w': return '1';
        case 'e': return '2';
        case 'r': return '3';
        case 't': return '(';
        case 'y': return ')';
        case 'u': return '_';
        case 'i': return '-';
        case 'o': return '+';
        case 'p': return '@';
        case 'a': return '*';
        case 's': return '4';
        case 'd': return '5';
        case 'f': return '6';
        case 'g': return '/';
        case 'h': return ':';
        case 'j': return ';';
        case 'k': return '\'';
        case 'l': return '"';
        case 'z': return '?';
        case 'x': return '7';
        case 'c': return '8';
        case 'v': return '9';
        case 'b': return '!';
        case 'n': return ',';
        case 'm': return '.';
        default: return key;
    }
}

// Handle physical keyboard input for compose screen
void handleComposeKeyboard(char key) {
    if (!compose_screen_active || !current_focused_input) return;

    // Debug: print key code
    Serial.printf("[KB] Key: %d (0x%02X) '%c'\n", key, key, (key >= 32 && key < 127) ? key : '?');

    // Check for Shift key double-tap (Caps Lock toggle)
    if (key == KEY_SHIFT) {
        uint32_t now = millis();
        if (now - lastShiftTime < DOUBLE_TAP_MS) {
            capsLockActive = !capsLockActive;
            symbolLockActive = false;  // Disable symbol lock when enabling caps
            Serial.printf("[KB] Caps Lock: %s\n", capsLockActive ? "ON" : "OFF");
            // Show popup indicator
            static lv_obj_t* lock_popup = nullptr;
            if (lock_popup && lv_obj_is_valid(lock_popup)) {
                lv_obj_del(lock_popup);
            }
            lock_popup = lv_label_create(lv_scr_act());
            lv_label_set_text(lock_popup, capsLockActive ? "MAJ" : "maj");
            lv_obj_set_style_bg_color(lock_popup, capsLockActive ? lv_color_hex(0x00aa00) : lv_color_hex(0x666666), 0);
            lv_obj_set_style_bg_opa(lock_popup, LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(lock_popup, lv_color_hex(0xffffff), 0);
            lv_obj_set_style_pad_all(lock_popup, 8, 0);
            lv_obj_set_style_radius(lock_popup, 5, 0);
            lv_obj_align(lock_popup, LV_ALIGN_TOP_MID, 0, 40);
            // Auto-delete after 1.5 sec
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, lock_popup);
            lv_anim_set_time(&a, 1500);
            lv_anim_set_deleted_cb(&a, [](lv_anim_t* anim) {
                lv_obj_t* obj = (lv_obj_t*)anim->var;
                if (obj && lv_obj_is_valid(obj)) lv_obj_del(obj);
            });
            lv_anim_start(&a);
        }
        lastShiftTime = now;
        return;
    }

    // Check for Symbol key double-tap (Symbol Lock toggle)
    if (key == KEY_SYMBOL) {
        uint32_t now = millis();
        if (now - lastSymbolTime < DOUBLE_TAP_MS) {
            symbolLockActive = !symbolLockActive;
            capsLockActive = false;  // Disable caps lock when enabling symbol
            Serial.printf("[KB] Symbol Lock: %s\n", symbolLockActive ? "ON" : "OFF");
            // Show popup indicator
            static lv_obj_t* sym_popup = nullptr;
            if (sym_popup && lv_obj_is_valid(sym_popup)) {
                lv_obj_del(sym_popup);
            }
            sym_popup = lv_label_create(lv_scr_act());
            lv_label_set_text(sym_popup, symbolLockActive ? "SYM" : "sym");
            lv_obj_set_style_bg_color(sym_popup, symbolLockActive ? lv_color_hex(0x0088ff) : lv_color_hex(0x666666), 0);
            lv_obj_set_style_bg_opa(sym_popup, LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(sym_popup, lv_color_hex(0xffffff), 0);
            lv_obj_set_style_pad_all(sym_popup, 8, 0);
            lv_obj_set_style_radius(sym_popup, 5, 0);
            lv_obj_align(sym_popup, LV_ALIGN_TOP_MID, 0, 40);
            // Auto-delete after 1.5 sec
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, sym_popup);
            lv_anim_set_time(&a, 1500);
            lv_anim_set_deleted_cb(&a, [](lv_anim_t* anim) {
                lv_obj_t* obj = (lv_obj_t*)anim->var;
                if (obj && lv_obj_is_valid(obj)) lv_obj_del(obj);
            });
            lv_anim_start(&a);
        }
        lastSymbolTime = now;
        return;
    }

    if (key == 0x08 || key == 0x7F) {  // Backspace
        lv_textarea_del_char(current_focused_input);
    } else if (key == 0x0D || key == 0x0A) {  // Enter
        // Switch between inputs or send
        if (current_focused_input == compose_to_input) {
            lv_obj_clear_state(compose_to_input, LV_STATE_FOCUSED);
            lv_obj_add_state(compose_msg_input, LV_STATE_FOCUSED);
            current_focused_input = compose_msg_input;
        } else {
            // Send message
            const char* to = lv_textarea_get_text(compose_to_input);
            const char* msg = lv_textarea_get_text(compose_msg_input);
            if (strlen(to) > 0 && strlen(msg) > 0) {
                MSG_Utils::addToOutputBuffer(1, String(to), String(msg));
                lv_textarea_set_text(compose_to_input, "");
                lv_textarea_set_text(compose_msg_input, "");
                compose_screen_active = false;
                lv_scr_load_anim(screen_msg, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
            }
        }
    } else if (key >= 32 && key < 127) {  // Printable chars
        char outputKey = key;

        // Apply Symbol Lock transformation
        if (symbolLockActive && key >= 'a' && key <= 'z') {
            outputKey = getSymbolChar(key);
        }
        // Apply Caps Lock transformation (only for letters)
        else if (capsLockActive && key >= 'a' && key <= 'z') {
            outputKey = key - 32;  // Convert to uppercase
        }

        lv_textarea_add_char(current_focused_input, outputKey);
    }
}

static void btn_compose_clicked(lv_event_t* e) {
    Serial.printf("[LVGL] Compose button clicked, current_msg_type=%d\n", current_msg_type);
    if (current_msg_type == 2) {
        // Contacts tab - open add contact screen
        Serial.println("[LVGL] Opening contact edit screen");
        show_contact_edit_screen(nullptr);
    } else {
        // Messages tab - open compose message screen
        create_compose_screen();
        compose_screen_active = true;
        current_focused_input = compose_to_input;
        lv_scr_load_anim(screen_compose, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
    }
}

// Create the messages screen
static void create_msg_screen() {
    screen_msg = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_msg, lv_color_hex(0x1a1a2e), 0);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen_msg);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, 35);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x00d4ff), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 5, 0);

    // Back button
    lv_obj_t* btn_back = lv_btn_create(title_bar);
    lv_obj_set_size(btn_back, 50, 25);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x16213e), 0);
    lv_obj_add_event_cb(btn_back, btn_back_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT);
    lv_obj_center(lbl_back);

    // Title
    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "Messages");
    lv_obj_set_style_text_color(title, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    // Compose button
    lv_obj_t* btn_compose = lv_btn_create(title_bar);
    lv_obj_set_size(btn_compose, 40, 25);
    lv_obj_align(btn_compose, LV_ALIGN_RIGHT_MID, -50, 0);
    lv_obj_set_style_bg_color(btn_compose, lv_color_hex(0x00aa55), 0);
    lv_obj_add_event_cb(btn_compose, btn_compose_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_compose = lv_label_create(btn_compose);
    lv_label_set_text(lbl_compose, LV_SYMBOL_EDIT);
    lv_obj_center(lbl_compose);

    // Delete button
    lv_obj_t* btn_delete = lv_btn_create(title_bar);
    lv_obj_set_size(btn_delete, 40, 25);
    lv_obj_align(btn_delete, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_bg_color(btn_delete, lv_color_hex(0xff4444), 0);
    lv_obj_add_event_cb(btn_delete, btn_delete_msgs_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_delete = lv_label_create(btn_delete);
    lv_label_set_text(lbl_delete, LV_SYMBOL_TRASH);
    lv_obj_center(lbl_delete);

    // Tabview for APRS / Winlink
    msg_tabview = lv_tabview_create(screen_msg, LV_DIR_TOP, 30);
    lv_obj_set_size(msg_tabview, SCREEN_WIDTH, SCREEN_HEIGHT - 35);
    lv_obj_set_pos(msg_tabview, 0, 35);
    lv_obj_set_style_bg_color(msg_tabview, lv_color_hex(0x0f0f23), 0);
    lv_obj_add_event_cb(msg_tabview, msg_tab_changed, LV_EVENT_VALUE_CHANGED, NULL);

    // APRS Tab
    lv_obj_t* tab_aprs = lv_tabview_add_tab(msg_tabview, "APRS");
    lv_obj_set_style_bg_color(tab_aprs, lv_color_hex(0x0f0f23), 0);
    lv_obj_set_style_pad_all(tab_aprs, 5, 0);

    list_aprs_global = lv_list_create(tab_aprs);
    lv_obj_set_size(list_aprs_global, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(list_aprs_global, lv_color_hex(0x0f0f23), 0);
    lv_obj_set_style_border_width(list_aprs_global, 0, 0);
    populate_msg_list(list_aprs_global, 0);

    // Winlink Tab
    lv_obj_t* tab_wlnk = lv_tabview_add_tab(msg_tabview, "Winlink");
    lv_obj_set_style_bg_color(tab_wlnk, lv_color_hex(0x0f0f23), 0);
    lv_obj_set_style_pad_all(tab_wlnk, 5, 0);

    list_wlnk_global = lv_list_create(tab_wlnk);
    lv_obj_set_size(list_wlnk_global, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(list_wlnk_global, lv_color_hex(0x0f0f23), 0);
    lv_obj_set_style_border_width(list_wlnk_global, 0, 0);
    populate_msg_list(list_wlnk_global, 1);

    // Contacts Tab
    lv_obj_t* tab_contacts = lv_tabview_add_tab(msg_tabview, "Contacts");
    lv_obj_set_style_bg_color(tab_contacts, lv_color_hex(0x0f0f23), 0);
    lv_obj_set_style_pad_all(tab_contacts, 5, 0);

    list_contacts_global = lv_list_create(tab_contacts);
    lv_obj_set_size(list_contacts_global, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(list_contacts_global, lv_color_hex(0x0f0f23), 0);
    lv_obj_set_style_border_width(list_contacts_global, 0, 0);
    populate_contacts_list(list_contacts_global);

    Serial.println("[LVGL] Messages screen created with tabs");
}

namespace LVGL_UI {

    // Splash and init screens shown during boot
    static lv_obj_t* screen_splash = nullptr;
    static lv_obj_t* screen_init = nullptr;
    static lv_obj_t* init_status_label = nullptr;

    void initLvglDisplay() {
        // Turn off backlight during init to avoid garbage display
        #ifdef BOARD_BL_PIN
            pinMode(BOARD_BL_PIN, OUTPUT);
            digitalWrite(BOARD_BL_PIN, LOW);
        #endif

        // Init TFT
        tft.init();
        tft.setRotation(1);
        tft.fillScreen(TFT_BLACK);  // Clear to black before showing anything

        // Now turn on backlight
        #ifdef BOARD_BL_PIN
            digitalWrite(BOARD_BL_PIN, HIGH);
        #endif

        // Initialize LVGL if not already done
        if (!lvgl_display_initialized) {
            lv_init();

            // Allocate display buffers in PSRAM
            #ifdef BOARD_HAS_PSRAM
                buf1 = (lv_color_t*)ps_malloc(LVGL_BUF_SIZE * sizeof(lv_color_t));
                buf2 = (lv_color_t*)ps_malloc(LVGL_BUF_SIZE * sizeof(lv_color_t));
            #else
                buf1 = (lv_color_t*)malloc(LVGL_BUF_SIZE * sizeof(lv_color_t));
                buf2 = nullptr;
            #endif

            if (buf1) {
                lv_disp_draw_buf_init(&draw_buf, buf1, buf2, LVGL_BUF_SIZE);
                lv_disp_drv_init(&disp_drv);
                disp_drv.hor_res = SCREEN_WIDTH;
                disp_drv.ver_res = SCREEN_HEIGHT;
                disp_drv.flush_cb = disp_flush_cb;
                disp_drv.draw_buf = &draw_buf;
                disp_drv.full_refresh = (buf2 != nullptr) ? 1 : 0;
                lv_disp_drv_register(&disp_drv);
            }
            lvgl_display_initialized = true;
        }
    }

    void showSplashScreen(uint8_t loraIndex, const char* version) {
        Serial.println("[LVGL] Showing splash screen");

        initLvglDisplay();

        // Create splash screen
        screen_splash = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen_splash, lv_color_hex(0x1a1a2e), 0);

        // Title: LoRa APRS
        lv_obj_t* title = lv_label_create(screen_splash);
        lv_label_set_text(title, "LoRa APRS");
        lv_obj_set_style_text_color(title, lv_color_hex(0x00ff88), 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

        // Subtitle: (TRACKER)
        lv_obj_t* subtitle = lv_label_create(screen_splash);
        lv_label_set_text(subtitle, "(TRACKER)");
        lv_obj_set_style_text_color(subtitle, lv_color_hex(0x00d4ff), 0);
        lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_18, 0);
        lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 90);

        // LoRa Frequency
        const char* region;
        switch (loraIndex) {
            case 0: region = "EU"; break;
            case 1: region = "PL"; break;
            case 2: region = "UK"; break;
            case 3: region = "US"; break;
            default: region = "??"; break;
        }
        char freqBuf[32];
        snprintf(freqBuf, sizeof(freqBuf), "LoRa Freq [%s]", region);
        lv_obj_t* freq_label = lv_label_create(screen_splash);
        lv_label_set_text(freq_label, freqBuf);
        lv_obj_set_style_text_color(freq_label, lv_color_hex(0xffffff), 0);
        lv_obj_align(freq_label, LV_ALIGN_CENTER, 0, 0);

        // Author and version
        char verBuf[48];
        snprintf(verBuf, sizeof(verBuf), "CA2RXU  %s", version);
        lv_obj_t* ver_label = lv_label_create(screen_splash);
        lv_label_set_text(ver_label, verBuf);
        lv_obj_set_style_text_color(ver_label, lv_color_hex(0xc792ea), 0);
        lv_obj_align(ver_label, LV_ALIGN_BOTTOM_MID, 0, -30);

        // Load and display
        lv_scr_load(screen_splash);
        lv_refr_now(NULL);

        // Brief delay then switch to init screen
        delay(2400);

        Serial.println("[LVGL] Splash done, showing init screen");
    }

    void showInitScreen() {
        // Create initialization status screen
        screen_init = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen_init, lv_color_hex(0x1a1a2e), 0);

        // Title
        lv_obj_t* title = lv_label_create(screen_init);
        lv_label_set_text(title, "Initialisation...");
        lv_obj_set_style_text_color(title, lv_color_hex(0x00d4ff), 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 80);

        // Status label (updated during init)
        init_status_label = lv_label_create(screen_init);
        lv_label_set_text(init_status_label, "...");
        lv_obj_set_style_text_color(init_status_label, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(init_status_label, &lv_font_montserrat_14, 0);
        lv_obj_align(init_status_label, LV_ALIGN_CENTER, 0, 20);

        lv_scr_load(screen_init);
        lv_refr_now(NULL);

        // Delete splash
        if (screen_splash) {
            lv_obj_del(screen_splash);
            screen_splash = nullptr;
        }
    }

    void updateInitStatus(const char* status) {
        if (init_status_label && lv_obj_is_valid(init_status_label)) {
            lv_label_set_text(init_status_label, status);
            lv_refr_now(NULL);
        }
        Serial.printf("[LVGL] Init: %s\n", status);
    }

    void hideInitScreen() {
        if (screen_init) {
            lv_obj_del(screen_init);
            screen_init = nullptr;
            init_status_label = nullptr;
        }
    }

    void setup() {
        Serial.println("[LVGL] Initializing...");

        // Initialize tick counter
        last_tick = millis();

        // Only initialize display if not already done by splash screen
        if (!lvgl_display_initialized) {
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
            lvgl_display_initialized = true;
        } else {
            Serial.println("[LVGL] Display already initialized by splash screen");
        }

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

        // Clean up any remaining init screens
        if (screen_splash) {
            lv_obj_del(screen_splash);
            screen_splash = nullptr;
        }
        if (screen_init) {
            lv_obj_del(screen_init);
            screen_init = nullptr;
            init_status_label = nullptr;
        }

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

            // Update callsign and symbol if changed
            Beacon* currentBeacon = &Config.beacons[myBeaconsIndex];
            if (currentBeacon->callsign != last_callsign) {
                last_callsign = currentBeacon->callsign;
                updateCallsign(last_callsign.c_str());
                // Update APRS symbol when beacon changes
                drawAPRSSymbol(currentBeacon->symbol.c_str());
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

            // Update date/time from GPS
            if (gps.time.isValid() && gps.date.isValid()) {
                updateTime(gps.date.day(), gps.date.month(), gps.date.year(), gps.time.hour(), gps.time.minute(), gps.time.second());
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

            // Update Bluetooth status
            if (label_bluetooth) {
                if (!bluetoothActive) {
                    lv_label_set_text(label_bluetooth, "BT: Disabled");
                    lv_obj_set_style_text_color(label_bluetooth, lv_color_hex(0x666666), 0);
                } else if (bluetoothConnected) {
                    String addr = BLE_Utils::getConnectedDeviceAddress();
                    if (addr.length() > 0) {
                        String btText = "BT: > " + addr;
                        lv_label_set_text(label_bluetooth, btText.c_str());
                    } else {
                        lv_label_set_text(label_bluetooth, "BT: Connected");
                    }
                    lv_obj_set_style_text_color(label_bluetooth, lv_color_hex(0xc792ea), 0);
                } else {
                    lv_label_set_text(label_bluetooth, "BT: Waiting...");
                    lv_obj_set_style_text_color(label_bluetooth, lv_color_hex(0xffa500), 0);
                }
            }

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
                "GPS: %d sat Lat: %.4f Lon: %.4f\nAlt: %.0f m  Spd: %.0f km/h",
                sats, lat, lng, alt, speed);
            lv_label_set_text(label_gps, buf);
        }
    }

    void updateBattery(int percent, float voltage) {
        if (label_battery) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Bat: %.2f V (%d%%)", voltage, percent);
            lv_label_set_text(label_battery, buf);

            // Change color based on level (red/coral base, green when good)
            if (percent > 50) {
                lv_obj_set_style_text_color(label_battery, lv_color_hex(0x00ff88), 0);  // Green
            } else if (percent > 20) {
                lv_obj_set_style_text_color(label_battery, lv_color_hex(0xffa500), 0);  // Orange
            } else {
                lv_obj_set_style_text_color(label_battery, lv_color_hex(0xff6b6b), 0);  // Red/coral
            }
        }
    }

    void updateLoRa(const char* lastRx, int rssi) {
        if (label_lora) {
            char buf[128];
            float freq = Config.loraTypes[loraIndex].frequency / 1000000.0;
            int rate = Config.loraTypes[loraIndex].dataRate;
            snprintf(buf, sizeof(buf), "LoRa: %.3f MHz  %d bps\nLast RX: %s (%ddBm)", freq, rate, lastRx, rssi);
            lv_label_set_text(label_lora, buf);
        }
    }

    void refreshLoRaInfo() {
        if (label_lora) {
            char buf[128];
            float freq = Config.loraTypes[loraIndex].frequency / 1000000.0;
            int rate = Config.loraTypes[loraIndex].dataRate;
            snprintf(buf, sizeof(buf), "LoRa: %.3f MHz  %d bps\nLast RX: ---", freq, rate);
            lv_label_set_text(label_lora, buf);
        }
    }

    void updateWiFi(bool connected, int rssi) {
        if (label_wifi) {
            if (WiFiUserDisabled) {
                lv_label_set_text(label_wifi, "WiFi: Disabled");
                lv_obj_set_style_text_color(label_wifi, lv_color_hex(0xff6b6b), 0);  // Red
            } else if (connected) {
                char buf[48];
                String ip = WiFi.localIP().toString();
                snprintf(buf, sizeof(buf), "WiFi: %s (%d dBm)", ip.c_str(), rssi);
                lv_label_set_text(label_wifi, buf);
                lv_obj_set_style_text_color(label_wifi, lv_color_hex(0x00d4ff), 0);  // Cyan
            } else if (WiFiEcoMode) {
                lv_label_set_text(label_wifi, "WiFi: Eco (sleep)");
                lv_obj_set_style_text_color(label_wifi, lv_color_hex(0xffa500), 0);  // Orange
            } else {
                lv_label_set_text(label_wifi, "WiFi: ---");
                lv_obj_set_style_text_color(label_wifi, lv_color_hex(0x00d4ff), 0);  // Cyan
            }
        }
    }

    // RX Message popup
    static lv_obj_t* rx_msgbox = nullptr;
    static lv_timer_t* rx_popup_timer = nullptr;

    static void hide_rx_popup(lv_timer_t* timer) {
        if (rx_msgbox) {
            lv_msgbox_close(rx_msgbox);
            rx_msgbox = nullptr;
        }
        rx_popup_timer = nullptr;
    }

    void showMessage(const char* from, const char* message) {
        Serial.printf("[LVGL] showMessage from %s: %s\n", from, message);

        // Close existing msgbox if any
        if (rx_msgbox) {
            lv_msgbox_close(rx_msgbox);
            rx_msgbox = nullptr;
        }
        if (rx_popup_timer) {
            lv_timer_del(rx_popup_timer);
            rx_popup_timer = nullptr;
        }

        // Build message content
        char content[256];
        snprintf(content, sizeof(content), "From: %s\n\n%s", from, message);

        // Create message box on active screen (blue color for RX)
        rx_msgbox = lv_msgbox_create(lv_scr_act(), ">>> MSG Rx <<<", content, NULL, false);
        lv_obj_set_size(rx_msgbox, 290, 140);
        lv_obj_set_style_bg_color(rx_msgbox, lv_color_hex(0x000022), 0);
        lv_obj_set_style_bg_opa(rx_msgbox, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(rx_msgbox, lv_color_hex(0x4488ff), 0);
        lv_obj_set_style_border_width(rx_msgbox, 3, 0);
        lv_obj_set_style_text_color(rx_msgbox, lv_color_hex(0x88ccff), 0);
        lv_obj_center(rx_msgbox);

        // Force immediate refresh
        lv_refr_now(NULL);

        // Auto-close after 4 seconds (longer for received messages)
        rx_popup_timer = lv_timer_create(hide_rx_popup, 4000, NULL);
        lv_timer_set_repeat_count(rx_popup_timer, 1);

        Serial.println("[LVGL] RX msgbox created and refreshed");
    }

    void updateCallsign(const char* callsign) {
        if (label_callsign) {
            lv_label_set_text(label_callsign, callsign);
        }
    }

    void updateTime(int day, int month, int year, int hour, int minute, int second) {
        if (label_time) {
            char buf[28];
            snprintf(buf, sizeof(buf), "%02d/%02d/%04d %02d:%02d:%02d UTC", day, month, year, hour, minute, second);
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

    // RX LoRa packet popup (blue)
    static lv_obj_t* rx_lora_msgbox = nullptr;
    static lv_timer_t* rx_lora_timer = nullptr;

    static void hide_rx_lora_popup(lv_timer_t* timer) {
        if (rx_lora_msgbox) {
            lv_msgbox_close(rx_lora_msgbox);
            rx_lora_msgbox = nullptr;
        }
        rx_lora_timer = nullptr;
    }

    void showRxPacket(const char* packet) {
        Serial.printf("[LVGL] showRxPacket called: %s\n", packet);

        // Close existing msgbox if any
        if (rx_lora_msgbox) {
            lv_msgbox_close(rx_lora_msgbox);
            rx_lora_msgbox = nullptr;
        }
        if (rx_lora_timer) {
            lv_timer_del(rx_lora_timer);
            rx_lora_timer = nullptr;
        }

        // Create message box on active screen (blue for RX)
        rx_lora_msgbox = lv_msgbox_create(lv_scr_act(), ">>> RX <<<", packet, NULL, false);
        lv_obj_set_size(rx_lora_msgbox, 280, 120);
        lv_obj_set_style_bg_color(rx_lora_msgbox, lv_color_hex(0x000033), 0);
        lv_obj_set_style_bg_opa(rx_lora_msgbox, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(rx_lora_msgbox, lv_color_hex(0x4488ff), 0);
        lv_obj_set_style_border_width(rx_lora_msgbox, 3, 0);
        lv_obj_set_style_text_color(rx_lora_msgbox, lv_color_hex(0x88bbff), 0);
        lv_obj_center(rx_lora_msgbox);

        // Force immediate refresh
        lv_refr_now(NULL);

        // Auto-close after 3 seconds
        rx_lora_timer = lv_timer_create(hide_rx_lora_popup, 3000, NULL);
        lv_timer_set_repeat_count(rx_lora_timer, 1);

        Serial.println("[LVGL] RX msgbox created and refreshed");
    }

    // WiFi Eco Mode popup
    static lv_obj_t* wifi_eco_msgbox = nullptr;
    static lv_timer_t* wifi_eco_timer = nullptr;

    static void hide_wifi_eco_popup(lv_timer_t* timer) {
        if (wifi_eco_msgbox) {
            lv_msgbox_close(wifi_eco_msgbox);
            wifi_eco_msgbox = nullptr;
        }
        wifi_eco_timer = nullptr;
    }

    void showWiFiEcoMode() {
        Serial.println("[LVGL] showWiFiEcoMode called");

        // Check if LVGL UI is initialized
        if (!screen_main) {
            Serial.println("[LVGL] UI not initialized yet, skipping popup");
            return;
        }

        // Close existing msgbox if any
        if (wifi_eco_msgbox) {
            lv_msgbox_close(wifi_eco_msgbox);
            wifi_eco_msgbox = nullptr;
        }
        if (wifi_eco_timer) {
            lv_timer_del(wifi_eco_timer);
            wifi_eco_timer = nullptr;
        }

        // Create message box on active screen
        wifi_eco_msgbox = lv_msgbox_create(lv_scr_act(), "WiFi Eco Mode", "Connection failed\nRetry in 30 min", NULL, false);
        lv_obj_set_size(wifi_eco_msgbox, 240, 100);
        lv_obj_set_style_bg_color(wifi_eco_msgbox, lv_color_hex(0x332200), 0);
        lv_obj_set_style_bg_opa(wifi_eco_msgbox, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(wifi_eco_msgbox, lv_color_hex(0xffa500), 0);
        lv_obj_set_style_border_width(wifi_eco_msgbox, 3, 0);
        lv_obj_set_style_text_color(wifi_eco_msgbox, lv_color_hex(0xffa500), 0);
        lv_obj_center(wifi_eco_msgbox);

        // Force immediate refresh
        lv_refr_now(NULL);

        // Auto-close after 2 seconds
        wifi_eco_timer = lv_timer_create(hide_wifi_eco_popup, 2000, NULL);
        lv_timer_set_repeat_count(wifi_eco_timer, 1);

        Serial.println("[LVGL] WiFi Eco msgbox created");
    }

    // Caps Lock popup
    static lv_obj_t* capslock_msgbox = nullptr;
    static lv_timer_t* capslock_timer = nullptr;

    static void hide_capslock_popup(lv_timer_t* timer) {
        if (capslock_msgbox && lv_obj_is_valid(capslock_msgbox)) {
            lv_msgbox_close(capslock_msgbox);
        }
        capslock_msgbox = nullptr;
        capslock_timer = nullptr;
    }

    void showCapsLockPopup(bool active) {
        Serial.printf("[LVGL] showCapsLockPopup called: %s\n", active ? "ON" : "OFF");

        // Check if LVGL UI is initialized
        if (!screen_main) {
            Serial.println("[LVGL] UI not initialized yet, skipping popup");
            return;
        }

        // Close existing msgbox if any
        if (capslock_msgbox && lv_obj_is_valid(capslock_msgbox)) {
            lv_msgbox_close(capslock_msgbox);
            capslock_msgbox = nullptr;
        }
        if (capslock_timer) {
            lv_timer_del(capslock_timer);
            capslock_timer = nullptr;
        }

        // Create message box on active screen
        const char* title = active ? "MAJ" : "maj";
        const char* msg = active ? "Caps Lock ON" : "Caps Lock OFF";
        capslock_msgbox = lv_msgbox_create(lv_scr_act(), title, msg, NULL, false);
        lv_obj_set_size(capslock_msgbox, 150, 70);
        if (active) {
            lv_obj_set_style_bg_color(capslock_msgbox, lv_color_hex(0x003300), 0);
            lv_obj_set_style_border_color(capslock_msgbox, lv_color_hex(0x00ff00), 0);
            lv_obj_set_style_text_color(capslock_msgbox, lv_color_hex(0x00ff00), 0);
        } else {
            lv_obj_set_style_bg_color(capslock_msgbox, lv_color_hex(0x333333), 0);
            lv_obj_set_style_border_color(capslock_msgbox, lv_color_hex(0x888888), 0);
            lv_obj_set_style_text_color(capslock_msgbox, lv_color_hex(0xaaaaaa), 0);
        }
        lv_obj_set_style_bg_opa(capslock_msgbox, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(capslock_msgbox, 2, 0);
        lv_obj_center(capslock_msgbox);

        // Force immediate refresh
        lv_refr_now(NULL);

        // Auto-close after 1.5 seconds
        capslock_timer = lv_timer_create(hide_capslock_popup, 1500, NULL);
        lv_timer_set_repeat_count(capslock_timer, 1);

        Serial.println("[LVGL] Caps Lock popup created");
    }

    void handleComposeKeyboard(char key) {
        ::handleComposeKeyboard(key);
    }

    void showBootWebConfig() {
        Serial.println("[LVGL] First boot web-conf mode (NOCALL detected)");

        // Create web-conf screen
        screen_webconf = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen_webconf, lv_color_hex(0x1a1a2e), 0);

        // Title bar (orange for web-conf)
        lv_obj_t* title_bar = lv_obj_create(screen_webconf);
        lv_obj_set_size(title_bar, SCREEN_WIDTH, 35);
        lv_obj_set_pos(title_bar, 0, 0);
        lv_obj_set_style_bg_color(title_bar, lv_color_hex(0xff6b35), 0);
        lv_obj_set_style_border_width(title_bar, 0, 0);
        lv_obj_set_style_radius(title_bar, 0, 0);

        lv_obj_t* title = lv_label_create(title_bar);
        lv_label_set_text(title, "First Time Setup");
        lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
        lv_obj_center(title);

        // Content area
        lv_obj_t* content = lv_obj_create(screen_webconf);
        lv_obj_set_size(content, SCREEN_WIDTH - 20, SCREEN_HEIGHT - 55);
        lv_obj_set_pos(content, 10, 45);
        lv_obj_set_style_bg_color(content, lv_color_hex(0x0f0f23), 0);
        lv_obj_set_style_border_color(content, lv_color_hex(0xff6b35), 0);
        lv_obj_set_style_radius(content, 8, 0);
        lv_obj_set_style_pad_all(content, 15, 0);
        lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        // Starting message
        lv_obj_t* msg1 = lv_label_create(content);
        lv_label_set_text(msg1, "Starting WiFi Access Point...");
        lv_obj_set_style_text_color(msg1, lv_color_hex(0xffa500), 0);
        lv_obj_set_style_text_font(msg1, &lv_font_montserrat_14, 0);

        // Load screen and refresh
        lv_scr_load(screen_webconf);
        lv_timer_handler();

        // Start AP mode
        bool success = WIFI_Utils::startAPModeNonBlocking();

        // Update screen with result
        lv_obj_clean(content);

        if (success) {
            String apName = WIFI_Utils::getAPName();

            lv_obj_t* lbl_status = lv_label_create(content);
            lv_label_set_text(lbl_status, "WiFi AP Active");
            lv_obj_set_style_text_color(lbl_status, lv_color_hex(0x00ff88), 0);
            lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_18, 0);

            lv_obj_t* lbl_ssid = lv_label_create(content);
            String ssidText = "SSID: " + apName;
            lv_label_set_text(lbl_ssid, ssidText.c_str());
            lv_obj_set_style_text_color(lbl_ssid, lv_color_hex(0xffffff), 0);
            lv_obj_set_style_text_font(lbl_ssid, &lv_font_montserrat_14, 0);

            lv_obj_t* lbl_pass = lv_label_create(content);
            lv_label_set_text(lbl_pass, "Pass: 1234567890");
            lv_obj_set_style_text_color(lbl_pass, lv_color_hex(0xffaa00), 0);
            lv_obj_set_style_text_font(lbl_pass, &lv_font_montserrat_14, 0);

            lv_obj_t* lbl_ip = lv_label_create(content);
            lv_label_set_text(lbl_ip, "IP: 192.168.4.1");
            lv_obj_set_style_text_color(lbl_ip, lv_color_hex(0x00d4ff), 0);
            lv_obj_set_style_text_font(lbl_ip, &lv_font_montserrat_18, 0);

            lv_obj_t* lbl_info = lv_label_create(content);
            lv_label_set_text(lbl_info, "Connect to WiFi AP\nthen open http://192.168.4.1\nto configure your callsign");
            lv_obj_set_style_text_color(lbl_info, lv_color_hex(0xaaaaaa), 0);
            lv_obj_set_style_text_font(lbl_info, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_align(lbl_info, LV_TEXT_ALIGN_CENTER, 0);

            // Reboot button
            lv_obj_t* btn_reboot = lv_btn_create(content);
            lv_obj_set_size(btn_reboot, 120, 40);
            lv_obj_set_style_bg_color(btn_reboot, lv_color_hex(0xff6b6b), 0);
            lv_obj_add_event_cb(btn_reboot, webconf_reboot_cb, LV_EVENT_CLICKED, NULL);
            lv_obj_t* lbl_btn = lv_label_create(btn_reboot);
            lv_label_set_text(lbl_btn, "Reboot");
            lv_obj_center(lbl_btn);

            // Force complete screen refresh
            lv_refr_now(NULL);
            lv_timer_handler();

            // *** BLOCKING LOOP - Only LVGL and touch handled ***
            Serial.println("[LVGL] Entering First Boot Web-Conf blocking loop");
            webconf_reboot_requested = false;

            while (!webconf_reboot_requested) {
                uint32_t now = millis();
                lv_tick_inc(now - last_tick);
                last_tick = now;
                lv_timer_handler();
                yield();
                delay(10);
            }

            Serial.println("[LVGL] Rebooting from First Boot Web-Conf");
            ESP.restart();

        } else {
            lv_obj_t* lbl_error = lv_label_create(content);
            lv_label_set_text(lbl_error, "Failed to start AP!\nPlease reboot");
            lv_obj_set_style_text_color(lbl_error, lv_color_hex(0xff6b6b), 0);
            lv_obj_set_style_text_font(lbl_error, &lv_font_montserrat_18, 0);
            lv_obj_set_style_text_align(lbl_error, LV_TEXT_ALIGN_CENTER, 0);

            lv_timer_handler();

            // Wait and reboot
            delay(5000);
            ESP.restart();
        }
    }

}

#endif // USE_LVGL_UI
