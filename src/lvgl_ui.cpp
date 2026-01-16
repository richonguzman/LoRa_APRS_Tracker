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
#include "lora_utils.h"
#include "station_utils.h"
#include "msg_utils.h"
#include "notification_utils.h"
#include "custom_characters.h"

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

// Current selection tracking (for highlight updates)
static lv_obj_t* current_freq_btn = nullptr;
static lv_obj_t* current_speed_btn = nullptr;
static lv_obj_t* current_callsign_btn = nullptr;

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

// Forward declarations
static void create_setup_screen();
static void create_freq_screen();
static void create_callsign_screen();
static void create_display_screen();
static void create_msg_screen();
static void create_speed_screen();
static void create_sound_screen();

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

    // Battery info (with extra spacing from WiFi)
    label_battery = lv_label_create(content);
    lv_label_set_text(label_battery, "Bat: --.-- V (--%)");
    lv_obj_set_style_text_color(label_battery, lv_color_hex(0xff6b6b), 0);  // Red/coral color
    lv_obj_set_style_text_font(label_battery, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(label_battery, 0, 115);

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

static void setup_item_reboot(lv_event_t* e) {
    Serial.println("[LVGL] Setup: Reboot selected");
    ESP.restart();
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
    lv_obj_set_size(btn_back, 60, 25);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x16213e), 0);
    lv_obj_add_event_cb(btn_back, btn_back_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "< BACK");
    lv_obj_center(lbl_back);

    // Title
    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "Messages");
    lv_obj_set_style_text_color(title, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 20, 0);

    // Content area
    lv_obj_t* content = lv_obj_create(screen_msg);
    lv_obj_set_size(content, SCREEN_WIDTH - 10, SCREEN_HEIGHT - 45);
    lv_obj_set_pos(content, 5, 40);
    lv_obj_set_style_bg_color(content, lv_color_hex(0x0f0f23), 0);
    lv_obj_set_style_border_color(content, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_radius(content, 8, 0);
    lv_obj_set_style_pad_all(content, 15, 0);

    // APRS Messages info
    char aprs_buf[64];
    int numAprs = MSG_Utils::getNumAPRSMessages();
    snprintf(aprs_buf, sizeof(aprs_buf), "APRS Messages: %d", numAprs);

    lv_obj_t* aprs_label = lv_label_create(content);
    lv_label_set_text(aprs_label, aprs_buf);
    lv_obj_set_style_text_color(aprs_label, lv_color_hex(0x00d4ff), 0);
    lv_obj_set_style_text_font(aprs_label, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(aprs_label, 0, 10);

    // Winlink Messages info
    char wlnk_buf[64];
    int numWlnk = MSG_Utils::getNumWLNKMails();
    snprintf(wlnk_buf, sizeof(wlnk_buf), "Winlink Mails: %d", numWlnk);

    lv_obj_t* wlnk_label = lv_label_create(content);
    lv_label_set_text(wlnk_label, wlnk_buf);
    lv_obj_set_style_text_color(wlnk_label, lv_color_hex(0xc792ea), 0);
    lv_obj_set_style_text_font(wlnk_label, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(wlnk_label, 0, 50);

    // Last heard tracker
    String lastHeard = MSG_Utils::getLastHeardTracker();
    char heard_buf[96];
    snprintf(heard_buf, sizeof(heard_buf), "Last Heard: %s", lastHeard.length() > 0 ? lastHeard.c_str() : "---");

    lv_obj_t* heard_label = lv_label_create(content);
    lv_label_set_text(heard_label, heard_buf);
    lv_obj_set_style_text_color(heard_label, lv_color_hex(0xff6b6b), 0);
    lv_obj_set_style_text_font(heard_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(heard_label, 0, 100);

    // Info text
    lv_obj_t* info_label = lv_label_create(content);
    lv_label_set_text(info_label, "Use keyboard to write messages");
    lv_obj_set_style_text_color(info_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(info_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(info_label, 0, 140);

    Serial.println("[LVGL] Messages screen created");
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
            if (connected) {
                char buf[48];
                String ip = WiFi.localIP().toString();
                snprintf(buf, sizeof(buf), "WiFi: %s (%d dBm)", ip.c_str(), rssi);
                lv_label_set_text(label_wifi, buf);
            } else {
                lv_label_set_text(label_wifi, "WiFi: ---");
            }
            // Always cyan like GPS
            lv_obj_set_style_text_color(label_wifi, lv_color_hex(0x00d4ff), 0);
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

}

#endif // USE_LVGL_UI
