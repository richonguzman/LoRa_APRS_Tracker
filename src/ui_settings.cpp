/* LVGL UI Settings Module
 * Setup menu, frequency, speed, callsign, display, sound, WiFi, Bluetooth screens
 *
 * Extracted from lvgl_ui.cpp for modularization
 */

#ifdef USE_LVGL_UI

#include "ui_settings.h"
#include "ui_common.h"
#include "ui_popups.h"
#include "ui_dashboard.h"
#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_heap_caps.h>
#include <math.h>

// External dependencies from other modules
#include "ble_utils.h"
#include "board_pinout.h"
#include "configuration.h"
#include "lora_utils.h"
#include "notification_utils.h"
#include "station_utils.h"
#include "wifi_utils.h"

// External variables from main code
extern Configuration Config;
extern bool sendUpdate;
extern bool WiFiConnected;
extern bool WiFiUserDisabled;
extern bool WiFiEcoMode;
extern uint8_t screenBrightness;
extern bool displayEcoMode;
extern bool screenDimmed;
extern uint32_t lastActivityTime;
extern bool bluetoothActive;
extern bool bluetoothConnected;
extern int loraIndex;
extern int loraIndexSize;
extern int myBeaconsIndex;
extern int myBeaconsSize;
extern uint32_t last_tick;

// External namespace for map screen
namespace UIMapManager {
    extern lv_obj_t *screen_map;
}

// =============================================================================
// Module State - Screen Pointers
// =============================================================================

static lv_obj_t *screen_setup = nullptr;
static lv_obj_t *screen_freq = nullptr;
static lv_obj_t *screen_speed = nullptr;
static lv_obj_t *screen_callsign = nullptr;
static lv_obj_t *screen_display = nullptr;
static lv_obj_t *screen_sound = nullptr;
static lv_obj_t *screen_wifi = nullptr;
static lv_obj_t *screen_bluetooth = nullptr;
static lv_obj_t *screen_webconf = nullptr;

// WiFi screen widgets
static lv_obj_t *wifi_switch = nullptr;
static lv_obj_t *wifi_status_label = nullptr;
static lv_obj_t *wifi_ip_row = nullptr;
static lv_obj_t *wifi_ip_label = nullptr;
static lv_obj_t *wifi_rssi_row = nullptr;
static lv_obj_t *wifi_rssi_label = nullptr;
static lv_timer_t *wifi_update_timer = nullptr;

// Bluetooth screen widgets
static lv_obj_t *bluetooth_switch = nullptr;
static lv_obj_t *bluetooth_status_label = nullptr;
static lv_obj_t *bluetooth_device_label = nullptr;
static lv_obj_t *bluetooth_device_row = nullptr;
static lv_timer_t *bluetooth_update_timer = nullptr;

// Selection tracking (for highlight updates)
static lv_obj_t *current_freq_btn = nullptr;
static lv_obj_t *current_speed_btn = nullptr;
static lv_obj_t *current_callsign_btn = nullptr;

// Web-conf state
static bool webconf_reboot_requested = false;

// =============================================================================
// Forward Declarations
// =============================================================================

static void btn_back_clicked(lv_event_t *e);
static void btn_back_to_setup_clicked(lv_event_t *e);
static void btn_wifi_back_clicked(lv_event_t *e);
static void btn_bluetooth_back_clicked(lv_event_t *e);

static void setup_item_callsign(lv_event_t *e);
static void setup_item_frequency(lv_event_t *e);
static void setup_item_speed(lv_event_t *e);
static void setup_item_display(lv_event_t *e);
static void setup_item_sound(lv_event_t *e);
static void setup_item_wifi(lv_event_t *e);
static void setup_item_bluetooth(lv_event_t *e);
static void setup_item_webconf(lv_event_t *e);
static void setup_item_reboot(lv_event_t *e);

static void freq_item_clicked(lv_event_t *e);
static void speed_item_clicked(lv_event_t *e);
static void callsign_item_clicked(lv_event_t *e);

static void wifi_switch_changed(lv_event_t *e);
static void wifi_screen_timer_cb(lv_timer_t *timer);
static void update_wifi_screen_status();

static void bluetooth_switch_changed(lv_event_t *e);
static void bluetooth_screen_timer_cb(lv_timer_t *timer);
static void update_bluetooth_screen_status();

static void nav_to_setup_timer_cb(lv_timer_t *timer);

// =============================================================================
// Navigation Callbacks
// =============================================================================

static void btn_back_clicked(lv_event_t *e) {
    Serial.println("[UISettings] BACK to dashboard");
    UIPopups::closeAll();
    lv_scr_load_anim(UIScreens::getMainScreen(), LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
}

static void btn_back_to_setup_clicked(lv_event_t *e) {
    Serial.println("[UISettings] BACK to setup");
    lv_scr_load_anim(screen_setup, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
}

static void btn_wifi_back_clicked(lv_event_t *e) {
    Serial.println("[UISettings] WiFi BACK");
    if (wifi_update_timer) {
        lv_timer_del(wifi_update_timer);
        wifi_update_timer = nullptr;
    }
    lv_scr_load_anim(screen_setup, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
}

static void btn_bluetooth_back_clicked(lv_event_t *e) {
    Serial.println("[UISettings] Bluetooth BACK");
    if (bluetooth_update_timer) {
        lv_timer_del(bluetooth_update_timer);
        bluetooth_update_timer = nullptr;
    }
    lv_scr_load_anim(screen_setup, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
}

static void nav_to_setup_timer_cb(lv_timer_t *timer) {
    lv_scr_load_anim(screen_setup, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
    lv_timer_del(timer);
}

// =============================================================================
// Setup Menu Item Callbacks
// =============================================================================

static void setup_item_callsign(lv_event_t *e) {
    Serial.println("[UISettings] Callsign selected");
    if (screen_callsign) {
        lv_obj_del(screen_callsign);
        screen_callsign = nullptr;
        current_callsign_btn = nullptr;
    }
    UISettings::createCallsignScreen();
    lv_scr_load_anim(screen_callsign, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
}

static void setup_item_frequency(lv_event_t *e) {
    Serial.println("[UISettings] Frequency selected");
    if (screen_freq) {
        lv_obj_del(screen_freq);
        screen_freq = nullptr;
        current_freq_btn = nullptr;
    }
    UISettings::createFreqScreen();
    lv_scr_load_anim(screen_freq, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
}

static void setup_item_speed(lv_event_t *e) {
    Serial.println("[UISettings] Speed selected");
    if (screen_speed) {
        lv_obj_del(screen_speed);
        screen_speed = nullptr;
        current_speed_btn = nullptr;
    }
    UISettings::createSpeedScreen();
    lv_scr_load_anim(screen_speed, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
}

static void setup_item_display(lv_event_t *e) {
    Serial.println("[UISettings] Display selected");
    if (!screen_display) {
        UISettings::createDisplayScreen();
    }
    lv_scr_load_anim(screen_display, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
}

static void setup_item_sound(lv_event_t *e) {
    Serial.println("[UISettings] Sound selected");
    if (!screen_sound) {
        UISettings::createSoundScreen();
    }
    lv_scr_load_anim(screen_sound, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
}

static void setup_item_wifi(lv_event_t *e) {
    Serial.println("[UISettings] WiFi selected");
    if (screen_wifi) {
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
    UISettings::createWifiScreen();
    lv_scr_load_anim(screen_wifi, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
}

static void setup_item_bluetooth(lv_event_t *e) {
    Serial.println("[UISettings] Bluetooth selected");
    if (BLE_Utils::isSleeping()) {
        BLE_Utils::wake();
    }
    if (screen_bluetooth) {
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
    UISettings::createBluetoothScreen();
    lv_scr_load_anim(screen_bluetooth, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
}

static void setup_item_reboot(lv_event_t *e) {
    Serial.println("[UISettings] Reboot selected");
    ESP.restart();
}

// =============================================================================
// Setup Screen Creation
// =============================================================================

void UISettings::createSetupScreen() {
    screen_setup = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_setup, lv_color_hex(UIColors::BG_DARK), 0);

    // Title bar
    lv_obj_t *title_bar = lv_obj_create(screen_setup);
    lv_obj_set_size(title_bar, UI_SCREEN_WIDTH, 35);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(UIColors::TEXT_PURPLE), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 5, 0);

    // Back button
    lv_obj_t *btn_back = lv_btn_create(title_bar);
    lv_obj_set_size(btn_back, 60, 25);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(UIColors::BG_HEADER), 0);
    lv_obj_add_event_cb(btn_back, btn_back_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "< BACK");
    lv_obj_center(lbl_back);

    // Title
    lv_obj_t *title = lv_label_create(title_bar);
    lv_label_set_text(title, "SETUP");
    lv_obj_set_style_text_color(title, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 20, 0);

    // Menu list
    lv_obj_t *list = lv_list_create(screen_setup);
    lv_obj_set_size(list, UI_SCREEN_WIDTH - 10, UI_SCREEN_HEIGHT - 45);
    lv_obj_set_pos(list, 5, 40);
    lv_obj_set_style_bg_color(list, lv_color_hex(UIColors::BG_DARKER), 0);
    lv_obj_set_style_border_color(list, lv_color_hex(UIColors::BG_HEADER), 0);
    lv_obj_set_style_radius(list, 8, 0);

    // Menu items
    lv_obj_t *btn;

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

    Serial.println("[UISettings] Setup screen created");
}

// =============================================================================
// Frequency Screen
// =============================================================================

static void freq_item_clicked(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_current_target(e);
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    Serial.printf("[UISettings] Frequency %d selected\n", index);
    LoRa_Utils::requestFrequencyChange(index);

    if (current_freq_btn && current_freq_btn != btn) {
        lv_obj_set_style_bg_color(current_freq_btn, lv_color_hex(UIColors::BG_DARKER), 0);
        lv_obj_set_style_text_color(current_freq_btn, lv_color_hex(UIColors::TEXT_WHITE), 0);
    }

    lv_obj_set_style_bg_color(btn, lv_color_hex(UIColors::TEXT_GREEN), 0);
    lv_obj_set_style_text_color(btn, lv_color_hex(0x000000), 0);
    current_freq_btn = btn;

    lv_timer_create(nav_to_setup_timer_cb, 600, NULL);
}

void UISettings::createFreqScreen() {
    screen_freq = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_freq, lv_color_hex(UIColors::BG_DARK), 0);

    // Title bar
    lv_obj_t *title_bar = lv_obj_create(screen_freq);
    lv_obj_set_size(title_bar, UI_SCREEN_WIDTH, 35);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(UIColors::BTN_BLUE), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 5, 0);

    // Back button
    lv_obj_t *btn_back = lv_btn_create(title_bar);
    lv_obj_set_size(btn_back, 60, 25);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(UIColors::BG_HEADER), 0);
    lv_obj_add_event_cb(btn_back, btn_back_to_setup_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "< BACK");
    lv_obj_center(lbl_back);

    // Title
    lv_obj_t *title = lv_label_create(title_bar);
    lv_label_set_text(title, "LoRa Frequency");
    lv_obj_set_style_text_color(title, lv_color_hex(UIColors::TEXT_WHITE), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 20, 0);

    // Frequency list
    lv_obj_t *list = lv_list_create(screen_freq);
    lv_obj_set_size(list, UI_SCREEN_WIDTH - 10, UI_SCREEN_HEIGHT - 45);
    lv_obj_set_pos(list, 5, 40);
    lv_obj_set_style_bg_color(list, lv_color_hex(UIColors::BG_DARKER), 0);
    lv_obj_set_style_border_color(list, lv_color_hex(UIColors::BG_HEADER), 0);
    lv_obj_set_style_radius(list, 8, 0);

    // Add frequency options from Config (skip US - index 3)
    for (int i = 0; i < loraIndexSize && i < (int)Config.loraTypes.size(); i++) {
        if (i == 3) continue; // Skip US frequency

        char buf[64];
        float freq = Config.loraTypes[i].frequency / 1000000.0;

        const char *region;
        switch (i) {
            case 0: region = "EU/WORLD"; break;
            case 1: region = "POLAND"; break;
            case 2: region = "UK"; break;
            default: region = "CUSTOM"; break;
        }
        snprintf(buf, sizeof(buf), "%s - %.3f MHz", region, freq);

        lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_WIFI, buf);
        lv_obj_add_event_cb(btn, freq_item_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        if (i == loraIndex) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(UIColors::TEXT_GREEN), 0);
            lv_obj_set_style_text_color(btn, lv_color_hex(0x000000), 0);
            current_freq_btn = btn;
        } else {
            lv_obj_set_style_bg_color(btn, lv_color_hex(UIColors::BG_DARKER), 0);
            lv_obj_set_style_text_color(btn, lv_color_hex(UIColors::TEXT_WHITE), 0);
        }
    }

    Serial.println("[UISettings] Frequency screen created");
}

// =============================================================================
// Speed Screen
// =============================================================================

static void speed_item_clicked(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_current_target(e);
    int dataRate = (int)(intptr_t)lv_event_get_user_data(e);
    Serial.printf("[UISettings] Speed %d bps selected\n", dataRate);
    LoRa_Utils::requestDataRateChange(dataRate);

    if (current_speed_btn && current_speed_btn != btn) {
        lv_obj_set_style_bg_color(current_speed_btn, lv_color_hex(UIColors::BG_DARKER), 0);
        lv_obj_set_style_text_color(current_speed_btn, lv_color_hex(UIColors::TEXT_WHITE), 0);
    }

    lv_obj_set_style_bg_color(btn, lv_color_hex(UIColors::TEXT_GREEN), 0);
    lv_obj_set_style_text_color(btn, lv_color_hex(0x000000), 0);
    current_speed_btn = btn;

    lv_timer_create(nav_to_setup_timer_cb, 600, NULL);
}

void UISettings::createSpeedScreen() {
    screen_speed = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_speed, lv_color_hex(UIColors::BG_DARK), 0);

    // Title bar
    lv_obj_t *title_bar = lv_obj_create(screen_speed);
    lv_obj_set_size(title_bar, UI_SCREEN_WIDTH, 35);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(UIColors::BTN_BLUE), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 5, 0);

    // Back button
    lv_obj_t *btn_back = lv_btn_create(title_bar);
    lv_obj_set_size(btn_back, 60, 25);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(UIColors::BG_HEADER), 0);
    lv_obj_add_event_cb(btn_back, btn_back_to_setup_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "< BACK");
    lv_obj_center(lbl_back);

    // Title
    lv_obj_t *title = lv_label_create(title_bar);
    lv_label_set_text(title, "LoRa Speed");
    lv_obj_set_style_text_color(title, lv_color_hex(UIColors::TEXT_WHITE), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 20, 0);

    // Speed list
    lv_obj_t *list = lv_list_create(screen_speed);
    lv_obj_set_size(list, UI_SCREEN_WIDTH - 10, UI_SCREEN_HEIGHT - 45);
    lv_obj_set_pos(list, 5, 40);
    lv_obj_set_style_bg_color(list, lv_color_hex(UIColors::BG_DARKER), 0);
    lv_obj_set_style_border_color(list, lv_color_hex(UIColors::BG_HEADER), 0);
    lv_obj_set_style_radius(list, 8, 0);

    // Speed options with data rates
    struct SpeedOption { int dataRate; const char *desc; };
    const SpeedOption speeds[] = {
        {1200, "1200 bps (SF9, Fast)"},
        {610, "610 bps (SF10)"},
        {300, "300 bps (SF12, Long range)"},
        {244, "244 bps (SF12)"},
        {209, "209 bps (SF12)"},
        {183, "183 bps (SF12, Longest)"}
    };

    int currentDataRate = Config.loraTypes[loraIndex].dataRate;

    for (int i = 0; i < 6; i++) {
        lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_SHUFFLE, speeds[i].desc);
        lv_obj_add_event_cb(btn, speed_item_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)speeds[i].dataRate);

        if (speeds[i].dataRate == currentDataRate) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(UIColors::TEXT_GREEN), 0);
            lv_obj_set_style_text_color(btn, lv_color_hex(0x000000), 0);
            current_speed_btn = btn;
        } else {
            lv_obj_set_style_bg_color(btn, lv_color_hex(UIColors::BG_DARKER), 0);
            lv_obj_set_style_text_color(btn, lv_color_hex(UIColors::TEXT_WHITE), 0);
        }
    }

    Serial.println("[UISettings] Speed screen created");
}

// =============================================================================
// Callsign Screen (placeholder - will be completed)
// =============================================================================

static void callsign_item_clicked(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_current_target(e);
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    Serial.printf("[UISettings] Callsign %d selected\n", index);

    // Update beacon index and save
    myBeaconsIndex = index;
    STATION_Utils::saveIndex(0, myBeaconsIndex);

    // Update the callsign label on main screen
    UIDashboard::updateCallsign(Config.beacons[myBeaconsIndex].callsign.c_str());

    // Update APRS symbol for new beacon
    String fullSymbol = Config.beacons[myBeaconsIndex].overlay +
                        Config.beacons[myBeaconsIndex].symbol;
    UIDashboard::drawAPRSSymbol(fullSymbol.c_str());

    if (current_callsign_btn && current_callsign_btn != btn) {
        lv_obj_set_style_bg_color(current_callsign_btn, lv_color_hex(UIColors::BG_DARKER), 0);
        lv_obj_set_style_text_color(current_callsign_btn, lv_color_hex(UIColors::TEXT_WHITE), 0);
    }

    lv_obj_set_style_bg_color(btn, lv_color_hex(UIColors::TEXT_GREEN), 0);
    lv_obj_set_style_text_color(btn, lv_color_hex(0x000000), 0);
    current_callsign_btn = btn;

    lv_timer_create(nav_to_setup_timer_cb, 600, NULL);
}

void UISettings::createCallsignScreen() {
    screen_callsign = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_callsign, lv_color_hex(UIColors::BG_DARK), 0);

    // Title bar (green for callsign)
    lv_obj_t *title_bar = lv_obj_create(screen_callsign);
    lv_obj_set_size(title_bar, UI_SCREEN_WIDTH, 35);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(UIColors::TEXT_GREEN), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 5, 0);

    // Back button
    lv_obj_t *btn_back = lv_btn_create(title_bar);
    lv_obj_set_size(btn_back, 60, 25);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(UIColors::BG_HEADER), 0);
    lv_obj_add_event_cb(btn_back, btn_back_to_setup_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "< BACK");
    lv_obj_center(lbl_back);

    // Title
    lv_obj_t *title = lv_label_create(title_bar);
    lv_label_set_text(title, "Callsign");
    lv_obj_set_style_text_color(title, lv_color_hex(UIColors::TEXT_WHITE), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 20, 0);

    // Callsign list
    lv_obj_t *list = lv_list_create(screen_callsign);
    lv_obj_set_size(list, UI_SCREEN_WIDTH - 10, UI_SCREEN_HEIGHT - 45);
    lv_obj_set_pos(list, 5, 40);
    lv_obj_set_style_bg_color(list, lv_color_hex(UIColors::BG_DARKER), 0);
    lv_obj_set_style_border_color(list, lv_color_hex(UIColors::BG_HEADER), 0);
    lv_obj_set_style_radius(list, 8, 0);

    // Add callsign options from Config
    for (int i = 0; i < myBeaconsSize && i < (int)Config.beacons.size(); i++) {
        String label = Config.beacons[i].callsign;
        if (Config.beacons[i].profileLabel.length() > 0) {
            label += " (" + Config.beacons[i].profileLabel + ")";
        }

        lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_CALL, label.c_str());
        lv_obj_add_event_cb(btn, callsign_item_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        if (i == myBeaconsIndex) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(UIColors::TEXT_GREEN), 0);
            lv_obj_set_style_text_color(btn, lv_color_hex(0x000000), 0);
            current_callsign_btn = btn;
        } else {
            lv_obj_set_style_bg_color(btn, lv_color_hex(UIColors::BG_DARKER), 0);
            lv_obj_set_style_text_color(btn, lv_color_hex(UIColors::TEXT_WHITE), 0);
        }
    }

    Serial.println("[UISettings] Callsign screen created");
}

// =============================================================================
// Display Screen
// =============================================================================

// Display screen widgets
static lv_obj_t *eco_switch = nullptr;
static lv_obj_t *brightness_slider = nullptr;
static lv_obj_t *brightness_label = nullptr;

// Brightness range (PWM values)
static const uint8_t BRIGHT_MIN = 50;
static const uint8_t BRIGHT_MAX = 255;
static const float GAMMA = 2.2;

// Convert percentage (5-100) to PWM with gamma correction
static uint8_t percentToPWM(int percent) {
    if (percent < 5) percent = 5;
    if (percent > 100) percent = 100;
    float normalized = (percent - 5) / 95.0;
    float corrected = pow(normalized, GAMMA);
    return BRIGHT_MIN + (uint8_t)(corrected * (BRIGHT_MAX - BRIGHT_MIN));
}

// Convert PWM to percentage (5-100) with inverse gamma
static int pwmToPercent(uint8_t pwm) {
    if (pwm < BRIGHT_MIN) pwm = BRIGHT_MIN;
    if (pwm > BRIGHT_MAX) pwm = BRIGHT_MAX;
    float normalized = (pwm - BRIGHT_MIN) / (float)(BRIGHT_MAX - BRIGHT_MIN);
    float corrected = pow(normalized, 1.0 / GAMMA);
    return 5 + (int)(corrected * 95);
}

static void eco_switch_changed(lv_event_t *e) {
    lv_obj_t *sw = lv_event_get_target(e);
    displayEcoMode = lv_obj_has_state(sw, LV_STATE_CHECKED);
    Serial.printf("[UISettings] ECO Mode: %s\n", displayEcoMode ? "ON" : "OFF");

    STATION_Utils::saveIndex(3, displayEcoMode ? 1 : 0);

    if (displayEcoMode) {
        lastActivityTime = millis();
    } else {
        if (screenDimmed) {
            screenDimmed = false;
#ifdef BOARD_BL_PIN
            analogWrite(BOARD_BL_PIN, screenBrightness);
#endif
            if (lv_scr_act() == UIMapManager::screen_map) {
                setCpuFrequencyMhz(240);
                Serial.printf("[UISettings] Eco mode disabled, CPU boosted to %d MHz (map)\n",
                              getCpuFrequencyMhz());
            }
        }
    }
}

static void brightness_slider_changed(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    int percent = (int)lv_slider_get_value(slider);

    screenBrightness = percentToPWM(percent);

#ifdef BOARD_BL_PIN
    analogWrite(BOARD_BL_PIN, screenBrightness);
#endif

    if (brightness_label) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", percent);
        lv_label_set_text(brightness_label, buf);
    }
}

static void brightness_slider_released(lv_event_t *e) {
    STATION_Utils::saveIndex(2, screenBrightness);
    Serial.printf("[UISettings] Brightness saved: %d\n", screenBrightness);
}

void UISettings::createDisplayScreen() {
    screen_display = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_display, lv_color_hex(UIColors::BG_DARK), 0);

    // Title bar
    lv_obj_t *title_bar = lv_obj_create(screen_display);
    lv_obj_set_size(title_bar, UI_SCREEN_WIDTH, 35);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0xffd700), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 5, 0);

    // Back button
    lv_obj_t *btn_back = lv_btn_create(title_bar);
    lv_obj_set_size(btn_back, 60, 25);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(UIColors::BG_HEADER), 0);
    lv_obj_add_event_cb(btn_back, btn_back_to_setup_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "< BACK");
    lv_obj_center(lbl_back);

    // Title
    lv_obj_t *title = lv_label_create(title_bar);
    lv_label_set_text(title, "Display");
    lv_obj_set_style_text_color(title, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 20, 0);

    // Content area
    lv_obj_t *content = lv_obj_create(screen_display);
    lv_obj_set_size(content, UI_SCREEN_WIDTH - 10, UI_SCREEN_HEIGHT - 45);
    lv_obj_set_pos(content, 5, 40);
    lv_obj_set_style_bg_color(content, lv_color_hex(UIColors::BG_DARKER), 0);
    lv_obj_set_style_border_color(content, lv_color_hex(UIColors::BG_HEADER), 0);
    lv_obj_set_style_radius(content, 8, 0);
    lv_obj_set_style_pad_all(content, 15, 0);

    // ECO Mode row
    lv_obj_t *eco_row = lv_obj_create(content);
    lv_obj_set_size(eco_row, lv_pct(100), 50);
    lv_obj_set_pos(eco_row, 0, 0);
    lv_obj_set_style_bg_opa(eco_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(eco_row, 0, 0);
    lv_obj_set_style_pad_all(eco_row, 0, 0);

    lv_obj_t *eco_label = lv_label_create(eco_row);
    lv_label_set_text(eco_label, "ECO Mode");
    lv_obj_set_style_text_color(eco_label, lv_color_hex(UIColors::TEXT_WHITE), 0);
    lv_obj_set_style_text_font(eco_label, &lv_font_montserrat_18, 0);
    lv_obj_align(eco_label, LV_ALIGN_LEFT_MID, 0, 0);

    eco_switch = lv_switch_create(eco_row);
    lv_obj_set_size(eco_switch, 60, 30);
    lv_obj_align(eco_switch, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(eco_switch, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(eco_switch, lv_color_hex(UIColors::TEXT_GREEN),
                              LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(eco_switch, lv_color_hex(UIColors::TEXT_WHITE), LV_PART_KNOB);
    if (displayEcoMode) {
        lv_obj_add_state(eco_switch, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(eco_switch, eco_switch_changed, LV_EVENT_VALUE_CHANGED, NULL);

    // Brightness row
    lv_obj_t *bright_row = lv_obj_create(content);
    lv_obj_set_size(bright_row, lv_pct(100), 70);
    lv_obj_set_pos(bright_row, 0, 55);
    lv_obj_set_style_bg_opa(bright_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bright_row, 0, 0);
    lv_obj_set_style_pad_all(bright_row, 0, 0);

    lv_obj_t *bright_title = lv_label_create(bright_row);
    lv_label_set_text(bright_title, "Brightness");
    lv_obj_set_style_text_color(bright_title, lv_color_hex(UIColors::TEXT_WHITE), 0);
    lv_obj_set_style_text_font(bright_title, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(bright_title, 0, 0);

    brightness_label = lv_label_create(bright_row);
    int pct = pwmToPercent(screenBrightness);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    lv_label_set_text(brightness_label, buf);
    lv_obj_set_style_text_color(brightness_label, lv_color_hex(0xffd700), 0);
    lv_obj_set_style_text_font(brightness_label, &lv_font_montserrat_14, 0);
    lv_obj_align(brightness_label, LV_ALIGN_TOP_RIGHT, 0, 0);

    brightness_slider = lv_slider_create(bright_row);
    lv_obj_set_size(brightness_slider, lv_pct(80), 20);
    lv_obj_align(brightness_slider, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_pad_all(brightness_slider, 5, LV_PART_KNOB);
    lv_slider_set_range(brightness_slider, 5, 100);
    lv_slider_set_value(brightness_slider, pct, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0x444466), LV_PART_MAIN);
    lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(UIColors::BTN_BLUE), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(UIColors::TEXT_WHITE), LV_PART_KNOB);
    lv_obj_add_event_cb(brightness_slider, brightness_slider_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(brightness_slider, brightness_slider_released, LV_EVENT_RELEASED, NULL);

    Serial.println("[UISettings] Display settings screen created");
}

// =============================================================================
// Sound Screen
// =============================================================================

static lv_obj_t *sound_switch = nullptr;
static lv_obj_t *volume_slider = nullptr;
static lv_obj_t *volume_label = nullptr;

static void sound_switch_changed(lv_event_t *e) {
    lv_obj_t *sw = lv_event_get_target(e);
    Config.notification.buzzerActive = lv_obj_has_state(sw, LV_STATE_CHECKED);
    Serial.printf("[UISettings] Sound: %s\n", Config.notification.buzzerActive ? "ON" : "OFF");
}

static void volume_slider_changed(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    Config.notification.volume = lv_slider_get_value(slider);

    if (volume_label) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", Config.notification.volume);
        lv_label_set_text(volume_label, buf);
    }
}

static void volume_slider_released(lv_event_t *e) {
    if (Config.notification.buzzerActive) {
        NOTIFICATION_Utils::playTone(1000, 100);
    }
}

static void tx_beep_changed(lv_event_t *e) {
    lv_obj_t *sw = lv_event_get_target(e);
    Config.notification.txBeep = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

static void rx_beep_changed(lv_event_t *e) {
    lv_obj_t *sw = lv_event_get_target(e);
    Config.notification.messageRxBeep = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

static void station_beep_changed(lv_event_t *e) {
    lv_obj_t *sw = lv_event_get_target(e);
    Config.notification.stationBeep = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

void UISettings::createSoundScreen() {
    screen_sound = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_sound, lv_color_hex(UIColors::BG_DARK), 0);

    // Title bar
    lv_obj_t *title_bar = lv_obj_create(screen_sound);
    lv_obj_set_size(title_bar, UI_SCREEN_WIDTH, 35);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0xff6b6b), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 5, 0);

    // Back button
    lv_obj_t *btn_back = lv_btn_create(title_bar);
    lv_obj_set_size(btn_back, 60, 25);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(UIColors::BG_HEADER), 0);
    lv_obj_add_event_cb(btn_back, btn_back_to_setup_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "< BACK");
    lv_obj_center(lbl_back);

    // Title
    lv_obj_t *title = lv_label_create(title_bar);
    lv_label_set_text(title, "Sound");
    lv_obj_set_style_text_color(title, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 20, 0);

    // Content area (scrollable)
    lv_obj_t *content = lv_obj_create(screen_sound);
    lv_obj_set_size(content, UI_SCREEN_WIDTH - 10, UI_SCREEN_HEIGHT - 45);
    lv_obj_set_pos(content, 5, 40);
    lv_obj_set_style_bg_color(content, lv_color_hex(UIColors::BG_DARKER), 0);
    lv_obj_set_style_border_color(content, lv_color_hex(UIColors::BG_HEADER), 0);
    lv_obj_set_style_radius(content, 8, 0);
    lv_obj_set_style_pad_all(content, 10, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Sound ON/OFF row
    lv_obj_t *sound_row = lv_obj_create(content);
    lv_obj_set_size(sound_row, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(sound_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sound_row, 0, 0);
    lv_obj_set_style_pad_all(sound_row, 0, 0);

    lv_obj_t *sound_label = lv_label_create(sound_row);
    lv_label_set_text(sound_label, "Sound");
    lv_obj_set_style_text_color(sound_label, lv_color_hex(UIColors::TEXT_WHITE), 0);
    lv_obj_set_style_text_font(sound_label, &lv_font_montserrat_14, 0);
    lv_obj_align(sound_label, LV_ALIGN_LEFT_MID, 0, 0);

    sound_switch = lv_switch_create(sound_row);
    lv_obj_set_size(sound_switch, 50, 25);
    lv_obj_align(sound_switch, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(sound_switch, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sound_switch, lv_color_hex(UIColors::TEXT_GREEN),
                              LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sound_switch, lv_color_hex(UIColors::TEXT_WHITE), LV_PART_KNOB);
    if (Config.notification.buzzerActive) {
        lv_obj_add_state(sound_switch, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(sound_switch, sound_switch_changed, LV_EVENT_VALUE_CHANGED, NULL);

    // Volume row
    lv_obj_t *vol_row = lv_obj_create(content);
    lv_obj_set_size(vol_row, lv_pct(100), 50);
    lv_obj_set_style_bg_opa(vol_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(vol_row, 0, 0);
    lv_obj_set_style_pad_all(vol_row, 0, 0);

    lv_obj_t *vol_title = lv_label_create(vol_row);
    lv_label_set_text(vol_title, "Volume");
    lv_obj_set_style_text_color(vol_title, lv_color_hex(UIColors::TEXT_WHITE), 0);
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
    lv_obj_set_style_bg_color(volume_slider, lv_color_hex(0x444466), LV_PART_MAIN);
    lv_obj_set_style_bg_color(volume_slider, lv_color_hex(0xff6b6b), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(volume_slider, lv_color_hex(UIColors::TEXT_WHITE), LV_PART_KNOB);
    lv_obj_add_event_cb(volume_slider, volume_slider_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(volume_slider, volume_slider_released, LV_EVENT_RELEASED, NULL);

    // TX Beep row
    lv_obj_t *tx_row = lv_obj_create(content);
    lv_obj_set_size(tx_row, lv_pct(100), 35);
    lv_obj_set_style_bg_opa(tx_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tx_row, 0, 0);
    lv_obj_set_style_pad_all(tx_row, 0, 0);

    lv_obj_t *tx_label = lv_label_create(tx_row);
    lv_label_set_text(tx_label, "TX Beep");
    lv_obj_set_style_text_color(tx_label, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(tx_label, &lv_font_montserrat_14, 0);
    lv_obj_align(tx_label, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *tx_sw = lv_switch_create(tx_row);
    lv_obj_set_size(tx_sw, 45, 22);
    lv_obj_align(tx_sw, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(tx_sw, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(tx_sw, lv_color_hex(UIColors::TEXT_GREEN),
                              LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (Config.notification.txBeep)
        lv_obj_add_state(tx_sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(tx_sw, tx_beep_changed, LV_EVENT_VALUE_CHANGED, NULL);

    // RX Message Beep row
    lv_obj_t *rx_row = lv_obj_create(content);
    lv_obj_set_size(rx_row, lv_pct(100), 35);
    lv_obj_set_style_bg_opa(rx_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rx_row, 0, 0);
    lv_obj_set_style_pad_all(rx_row, 0, 0);

    lv_obj_t *rx_label = lv_label_create(rx_row);
    lv_label_set_text(rx_label, "Message Beep");
    lv_obj_set_style_text_color(rx_label, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(rx_label, &lv_font_montserrat_14, 0);
    lv_obj_align(rx_label, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *rx_sw = lv_switch_create(rx_row);
    lv_obj_set_size(rx_sw, 45, 22);
    lv_obj_align(rx_sw, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(rx_sw, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(rx_sw, lv_color_hex(UIColors::TEXT_GREEN),
                              LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (Config.notification.messageRxBeep)
        lv_obj_add_state(rx_sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(rx_sw, rx_beep_changed, LV_EVENT_VALUE_CHANGED, NULL);

    // Station Beep row
    lv_obj_t *sta_row = lv_obj_create(content);
    lv_obj_set_size(sta_row, lv_pct(100), 35);
    lv_obj_set_style_bg_opa(sta_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sta_row, 0, 0);
    lv_obj_set_style_pad_all(sta_row, 0, 0);

    lv_obj_t *sta_label = lv_label_create(sta_row);
    lv_label_set_text(sta_label, "Station Beep");
    lv_obj_set_style_text_color(sta_label, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(sta_label, &lv_font_montserrat_14, 0);
    lv_obj_align(sta_label, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *sta_sw = lv_switch_create(sta_row);
    lv_obj_set_size(sta_sw, 45, 22);
    lv_obj_align(sta_sw, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(sta_sw, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sta_sw, lv_color_hex(UIColors::TEXT_GREEN),
                              LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (Config.notification.stationBeep)
        lv_obj_add_state(sta_sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sta_sw, station_beep_changed, LV_EVENT_VALUE_CHANGED, NULL);

    Serial.println("[UISettings] Sound settings screen created");
}

// =============================================================================
// WiFi Screen
// =============================================================================

static void wifi_switch_changed(lv_event_t *e) {
    lv_obj_t *sw = lv_event_get_target(e);
    bool is_on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    extern uint32_t lastWiFiRetry;

    if (is_on) {
        Serial.println("[UISettings] WiFi: User enabled");
        WiFiUserDisabled = false;
        WiFiEcoMode = true;
        lastWiFiRetry = 0;

        if (wifi_status_label) {
            lv_label_set_text(wifi_status_label, "Reconnecting...");
            lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xffa500), 0);
        }
    } else {
        Serial.println("[UISettings] WiFi: User disabled");
        WiFiUserDisabled = true;
        WiFiConnected = false;
        WiFiEcoMode = false;

        esp_wifi_disconnect();
        esp_wifi_stop();

        wifi_mode_t mode;
        if (esp_wifi_get_mode(&mode) == ESP_OK) {
            Serial.printf("[UISettings] WiFi hardware mode: %d (0=OFF)\n", mode);
        }

        if (wifi_status_label) {
            lv_label_set_text(wifi_status_label, "OFF (disabled)");
            lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xff6b6b), 0);
        }

        lv_obj_t *wifi_lbl = UIDashboard::getLabelWifi();
        if (wifi_lbl) {
            lv_label_set_text(wifi_lbl, "WiFi: OFF");
            lv_obj_set_style_text_color(wifi_lbl, lv_color_hex(0xff6b6b), 0);
        }
    }

    Config.wifiEnabled = is_on;
    Config.writeFile();
    Serial.printf("[UISettings] WiFi setting saved: %s\n", is_on ? "enabled" : "disabled");
}

static void update_wifi_screen_status() {
    if (!screen_wifi) return;

    if (wifi_switch) {
        if (WiFiUserDisabled) {
            lv_obj_clear_state(wifi_switch, LV_STATE_CHECKED);
        } else {
            lv_obj_add_state(wifi_switch, LV_STATE_CHECKED);
        }
    }

    if (wifi_status_label) {
        if (WiFiUserDisabled) {
            lv_label_set_text(wifi_status_label, "Disabled");
            lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xff6b6b), 0);
        } else if (WiFiConnected) {
            lv_label_set_text(wifi_status_label, "Connected");
            lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(UIColors::TEXT_GREEN), 0);
        } else if (WiFiEcoMode) {
            lv_label_set_text(wifi_status_label, "Eco (retry)");
            lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xffa500), 0);
        } else {
            lv_label_set_text(wifi_status_label, "Connecting...");
            lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xffa500), 0);
        }
    }

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

static void wifi_screen_timer_cb(lv_timer_t *timer) {
    update_wifi_screen_status();
}

void UISettings::createWifiScreen() {
    screen_wifi = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_wifi, lv_color_hex(UIColors::BG_DARK), 0);

    // Title bar (cyan)
    lv_obj_t *title_bar = lv_obj_create(screen_wifi);
    lv_obj_set_size(title_bar, UI_SCREEN_WIDTH, 35);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(UIColors::BTN_BLUE), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 5, 0);

    // Back button
    lv_obj_t *btn_back = lv_btn_create(title_bar);
    lv_obj_set_size(btn_back, 60, 25);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(UIColors::BG_HEADER), 0);
    lv_obj_add_event_cb(btn_back, btn_wifi_back_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "< BACK");
    lv_obj_center(lbl_back);

    // Title
    lv_obj_t *title = lv_label_create(title_bar);
    lv_label_set_text(title, "WiFi");
    lv_obj_set_style_text_color(title, lv_color_hex(UIColors::TEXT_WHITE), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 20, 0);

    // Content area
    lv_obj_t *content = lv_obj_create(screen_wifi);
    lv_obj_set_size(content, UI_SCREEN_WIDTH - 10, UI_SCREEN_HEIGHT - 45);
    lv_obj_set_pos(content, 5, 40);
    lv_obj_set_style_bg_color(content, lv_color_hex(UIColors::BG_DARKER), 0);
    lv_obj_set_style_border_color(content, lv_color_hex(UIColors::BG_HEADER), 0);
    lv_obj_set_style_radius(content, 8, 0);
    lv_obj_set_style_pad_all(content, 10, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // WiFi ON/OFF row
    lv_obj_t *wifi_row = lv_obj_create(content);
    lv_obj_set_size(wifi_row, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(wifi_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wifi_row, 0, 0);
    lv_obj_set_style_pad_all(wifi_row, 0, 0);

    lv_obj_t *wifi_label = lv_label_create(wifi_row);
    lv_label_set_text(wifi_label, "WiFi");
    lv_obj_set_style_text_color(wifi_label, lv_color_hex(UIColors::TEXT_WHITE), 0);
    lv_obj_set_style_text_font(wifi_label, &lv_font_montserrat_14, 0);
    lv_obj_align(wifi_label, LV_ALIGN_LEFT_MID, 0, 0);

    wifi_switch = lv_switch_create(wifi_row);
    lv_obj_set_size(wifi_switch, 50, 25);
    lv_obj_align(wifi_switch, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(wifi_switch, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(wifi_switch, lv_color_hex(UIColors::TEXT_GREEN),
                              LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(wifi_switch, lv_color_hex(UIColors::TEXT_WHITE), LV_PART_KNOB);
    if (!WiFiUserDisabled) {
        lv_obj_add_state(wifi_switch, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(wifi_switch, wifi_switch_changed, LV_EVENT_VALUE_CHANGED, NULL);

    // Status row
    lv_obj_t *status_row = lv_obj_create(content);
    lv_obj_set_size(status_row, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(status_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(status_row, 0, 0);
    lv_obj_set_style_pad_all(status_row, 0, 0);

    lv_obj_t *status_title = lv_label_create(status_row);
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
        lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(UIColors::TEXT_GREEN), 0);
    } else if (WiFiEcoMode) {
        lv_label_set_text(wifi_status_label, "Eco mode (retry)");
        lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xffa500), 0);
    } else {
        lv_label_set_text(wifi_status_label, "Connecting...");
        lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xffa500), 0);
    }
    lv_obj_set_style_text_font(wifi_status_label, &lv_font_montserrat_14, 0);
    lv_obj_align(wifi_status_label, LV_ALIGN_RIGHT_MID, 0, 0);

    // IP Address row
    wifi_ip_row = lv_obj_create(content);
    lv_obj_set_size(wifi_ip_row, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(wifi_ip_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wifi_ip_row, 0, 0);
    lv_obj_set_style_pad_all(wifi_ip_row, 0, 0);

    lv_obj_t *ip_title = lv_label_create(wifi_ip_row);
    lv_label_set_text(ip_title, "IP:");
    lv_obj_set_style_text_color(ip_title, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(ip_title, &lv_font_montserrat_14, 0);
    lv_obj_align(ip_title, LV_ALIGN_LEFT_MID, 0, 0);

    wifi_ip_label = lv_label_create(wifi_ip_row);
    lv_label_set_text(wifi_ip_label, "---");
    lv_obj_set_style_text_color(wifi_ip_label, lv_color_hex(UIColors::BTN_BLUE), 0);
    lv_obj_set_style_text_font(wifi_ip_label, &lv_font_montserrat_14, 0);
    lv_obj_align(wifi_ip_label, LV_ALIGN_RIGHT_MID, 0, 0);

    // RSSI row
    wifi_rssi_row = lv_obj_create(content);
    lv_obj_set_size(wifi_rssi_row, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(wifi_rssi_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wifi_rssi_row, 0, 0);
    lv_obj_set_style_pad_all(wifi_rssi_row, 0, 0);

    lv_obj_t *rssi_title = lv_label_create(wifi_rssi_row);
    lv_label_set_text(rssi_title, "Signal:");
    lv_obj_set_style_text_color(rssi_title, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(rssi_title, &lv_font_montserrat_14, 0);
    lv_obj_align(rssi_title, LV_ALIGN_LEFT_MID, 0, 0);

    wifi_rssi_label = lv_label_create(wifi_rssi_row);
    lv_label_set_text(wifi_rssi_label, "---");
    lv_obj_set_style_text_color(wifi_rssi_label, lv_color_hex(UIColors::BTN_BLUE), 0);
    lv_obj_set_style_text_font(wifi_rssi_label, &lv_font_montserrat_14, 0);
    lv_obj_align(wifi_rssi_label, LV_ALIGN_RIGHT_MID, 0, 0);

    // Initial update
    update_wifi_screen_status();

    // Start update timer
    wifi_update_timer = lv_timer_create(wifi_screen_timer_cb, 1000, NULL);

    Serial.println("[UISettings] WiFi settings screen created");
}

// =============================================================================
// Bluetooth Screen
// =============================================================================

static void update_bluetooth_screen_status() {
    if (!screen_bluetooth) return;

    if (bluetooth_switch) {
        if (bluetoothActive) {
            lv_obj_add_state(bluetooth_switch, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(bluetooth_switch, LV_STATE_CHECKED);
        }
    }

    if (bluetooth_status_label) {
        if (!bluetoothActive) {
            lv_label_set_text(bluetooth_status_label, "OFF");
            lv_obj_set_style_text_color(bluetooth_status_label, lv_color_hex(0xff6b6b), 0);
        } else if (bluetoothConnected) {
            lv_label_set_text(bluetooth_status_label, "Connected");
            lv_obj_set_style_text_color(bluetooth_status_label, lv_color_hex(UIColors::TEXT_GREEN), 0);
        } else {
            lv_label_set_text(bluetooth_status_label, "Waiting...");
            lv_obj_set_style_text_color(bluetooth_status_label, lv_color_hex(0xffa500), 0);
        }
    }

    if (bluetooth_device_row) {
        if (bluetoothActive && bluetoothConnected) {
            lv_obj_clear_flag(bluetooth_device_row, LV_OBJ_FLAG_HIDDEN);
            if (bluetooth_device_label) {
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

static void bluetooth_screen_timer_cb(lv_timer_t *timer) {
    update_bluetooth_screen_status();
}

// Timer callbacks for deferred BLE operations
static void ble_setup_timer_cb(lv_timer_t *timer) {
    uint32_t currentFreeHeap = ESP.getFreeHeap();
    uint32_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    Serial.printf("[UISettings] BLE setup timer: Free heap: %u bytes, Largest block: %u bytes\n",
                  currentFreeHeap, largestBlock);

    const uint32_t MIN_CONTIGUOUS_HEAP_FOR_BLE = 40 * 1024;

    if (Config.bluetooth.useBLE) {
        if (largestBlock < MIN_CONTIGUOUS_HEAP_FOR_BLE) {
            Serial.printf("[UISettings] WARNING: Insufficient contiguous memory (%u bytes) for BLE init. Aborting.\n",
                          largestBlock);
            if (bluetooth_status_label) {
                lv_label_set_text(bluetooth_status_label, "Failed (Mem)");
                lv_obj_set_style_text_color(bluetooth_status_label, lv_color_hex(0xff6b6b), 0);
            }
            if (bluetooth_switch) {
                lv_obj_clear_state(bluetooth_switch, LV_STATE_CHECKED);
            }
            bluetoothActive = false;
            Config.bluetooth.useBLE = false;
            Config.writeFile();
        } else {
            BLE_Utils::setup();
            Serial.printf("[UISettings] BLE setup done (deferred). Free heap: %u bytes\n", ESP.getFreeHeap());
        }
    }
    lv_timer_del(timer);
}

static void ble_stop_timer_cb(lv_timer_t *timer) {
    Serial.printf("[UISettings] BLE stop timer: Free heap before stop: %u bytes\n", ESP.getFreeHeap());
    if (Config.bluetooth.useBLE) {
        BLE_Utils::stop();
        Serial.printf("[UISettings] BLE stop done (deferred). Free heap: %u bytes\n", ESP.getFreeHeap());
    }
    lv_timer_del(timer);
}

static void bluetooth_switch_changed(lv_event_t *e) {
    lv_obj_t *sw = lv_event_get_target(e);
    bool is_on = lv_obj_has_state(sw, LV_STATE_CHECKED);

    if (is_on) {
        Serial.println("[UISettings] Bluetooth: Scheduling ON");
        bluetoothActive = true;
        lv_timer_create(ble_setup_timer_cb, 50, NULL);

        if (bluetooth_status_label) {
            lv_label_set_text(bluetooth_status_label, "Starting...");
            lv_obj_set_style_text_color(bluetooth_status_label, lv_color_hex(0xffa500), 0);
        }
    } else {
        Serial.println("[UISettings] Bluetooth: Scheduling OFF");
        bluetoothActive = false;
        lv_timer_create(ble_stop_timer_cb, 50, NULL);

        if (bluetooth_status_label) {
            lv_label_set_text(bluetooth_status_label, "Stopping...");
            lv_obj_set_style_text_color(bluetooth_status_label, lv_color_hex(0xffa500), 0);
        }
    }
}

void UISettings::createBluetoothScreen() {
    screen_bluetooth = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_bluetooth, lv_color_hex(UIColors::BG_DARK), 0);

    // Title bar (purple)
    lv_obj_t *title_bar = lv_obj_create(screen_bluetooth);
    lv_obj_set_size(title_bar, UI_SCREEN_WIDTH, 35);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(UIColors::TEXT_PURPLE), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 5, 0);

    // Back button
    lv_obj_t *btn_back = lv_btn_create(title_bar);
    lv_obj_set_size(btn_back, 60, 25);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(UIColors::BG_HEADER), 0);
    lv_obj_add_event_cb(btn_back, btn_bluetooth_back_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "< BACK");
    lv_obj_center(lbl_back);

    // Title
    lv_obj_t *title = lv_label_create(title_bar);
    lv_label_set_text(title, "Bluetooth");
    lv_obj_set_style_text_color(title, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 20, 0);

    // Content area
    lv_obj_t *content = lv_obj_create(screen_bluetooth);
    lv_obj_set_size(content, UI_SCREEN_WIDTH - 10, UI_SCREEN_HEIGHT - 45);
    lv_obj_set_pos(content, 5, 40);
    lv_obj_set_style_bg_color(content, lv_color_hex(UIColors::BG_DARKER), 0);
    lv_obj_set_style_border_color(content, lv_color_hex(UIColors::BG_HEADER), 0);
    lv_obj_set_style_radius(content, 8, 0);
    lv_obj_set_style_pad_all(content, 10, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Bluetooth ON/OFF row
    lv_obj_t *bt_row = lv_obj_create(content);
    lv_obj_set_size(bt_row, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(bt_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bt_row, 0, 0);
    lv_obj_set_style_pad_all(bt_row, 0, 0);

    lv_obj_t *bt_label = lv_label_create(bt_row);
    lv_label_set_text(bt_label, "Bluetooth");
    lv_obj_set_style_text_color(bt_label, lv_color_hex(UIColors::TEXT_WHITE), 0);
    lv_obj_set_style_text_font(bt_label, &lv_font_montserrat_14, 0);
    lv_obj_align(bt_label, LV_ALIGN_LEFT_MID, 0, 0);

    bluetooth_switch = lv_switch_create(bt_row);
    lv_obj_set_size(bluetooth_switch, 50, 25);
    lv_obj_align(bluetooth_switch, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(bluetooth_switch, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bluetooth_switch, lv_color_hex(UIColors::TEXT_PURPLE),
                              LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(bluetooth_switch, lv_color_hex(UIColors::TEXT_WHITE), LV_PART_KNOB);
    if (bluetoothActive) {
        lv_obj_add_state(bluetooth_switch, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(bluetooth_switch, bluetooth_switch_changed, LV_EVENT_VALUE_CHANGED, NULL);

    // Status row
    lv_obj_t *status_row = lv_obj_create(content);
    lv_obj_set_size(status_row, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(status_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(status_row, 0, 0);
    lv_obj_set_style_pad_all(status_row, 0, 0);

    lv_obj_t *status_title = lv_label_create(status_row);
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
        lv_obj_set_style_text_color(bluetooth_status_label, lv_color_hex(UIColors::TEXT_GREEN), 0);
    } else {
        lv_label_set_text(bluetooth_status_label, "ON (waiting)");
        lv_obj_set_style_text_color(bluetooth_status_label, lv_color_hex(0xffa500), 0);
    }
    lv_obj_set_style_text_font(bluetooth_status_label, &lv_font_montserrat_14, 0);
    lv_obj_align(bluetooth_status_label, LV_ALIGN_RIGHT_MID, 0, 0);

    // Type row
    lv_obj_t *type_row = lv_obj_create(content);
    lv_obj_set_size(type_row, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(type_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(type_row, 0, 0);
    lv_obj_set_style_pad_all(type_row, 0, 0);

    lv_obj_t *type_title = lv_label_create(type_row);
    lv_label_set_text(type_title, "Type:");
    lv_obj_set_style_text_color(type_title, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(type_title, &lv_font_montserrat_14, 0);
    lv_obj_align(type_title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *type_label = lv_label_create(type_row);
    lv_label_set_text(type_label, Config.bluetooth.useBLE ? "BLE" : "Classic");
    lv_obj_set_style_text_color(type_label, lv_color_hex(UIColors::TEXT_PURPLE), 0);
    lv_obj_set_style_text_font(type_label, &lv_font_montserrat_14, 0);
    lv_obj_align(type_label, LV_ALIGN_RIGHT_MID, 0, 0);

    // Device row
    bluetooth_device_row = lv_obj_create(content);
    lv_obj_set_size(bluetooth_device_row, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(bluetooth_device_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bluetooth_device_row, 0, 0);
    lv_obj_set_style_pad_all(bluetooth_device_row, 0, 0);

    lv_obj_t *device_title = lv_label_create(bluetooth_device_row);
    lv_label_set_text(device_title, "Device:");
    lv_obj_set_style_text_color(device_title, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(device_title, &lv_font_montserrat_14, 0);
    lv_obj_align(device_title, LV_ALIGN_LEFT_MID, 0, 0);

    bluetooth_device_label = lv_label_create(bluetooth_device_row);
    if (bluetoothConnected) {
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
    lv_obj_set_style_text_color(bluetooth_device_label, lv_color_hex(UIColors::TEXT_PURPLE), 0);
    lv_obj_set_style_text_font(bluetooth_device_label, &lv_font_montserrat_14, 0);
    lv_obj_align(bluetooth_device_label, LV_ALIGN_RIGHT_MID, 0, 0);

    // Hide device row if not connected
    if (!bluetoothActive || !bluetoothConnected) {
        lv_obj_add_flag(bluetooth_device_row, LV_OBJ_FLAG_HIDDEN);
    }

    // Start update timer
    bluetooth_update_timer = lv_timer_create(bluetooth_screen_timer_cb, 1000, NULL);

    Serial.println("[UISettings] Bluetooth settings screen created");
}

// =============================================================================
// Web-Conf Screen (blocking mode)
// =============================================================================

static void webconf_reboot_cb(lv_event_t *e) {
    webconf_reboot_requested = true;
}

static void setup_item_webconf(lv_event_t *e) {
    Serial.println("[UISettings] Web-Conf Mode selected");
    UISettings::openWebConf();
}

void UISettings::openWebConf() {
    Serial.println("[UISettings] Web-Conf Mode - entering blocking mode");

    // Create web-conf screen
    screen_webconf = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_webconf, lv_color_hex(UIColors::BG_DARK), 0);

    // Title bar (orange)
    lv_obj_t *title_bar = lv_obj_create(screen_webconf);
    lv_obj_set_size(title_bar, UI_SCREEN_WIDTH, 35);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0xff6b35), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);

    lv_obj_t *title = lv_label_create(title_bar);
    lv_label_set_text(title, "Web Configuration");
    lv_obj_set_style_text_color(title, lv_color_hex(UIColors::TEXT_WHITE), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_center(title);

    // Content area
    lv_obj_t *content = lv_obj_create(screen_webconf);
    lv_obj_set_size(content, UI_SCREEN_WIDTH - 20, UI_SCREEN_HEIGHT - 55);
    lv_obj_set_pos(content, 10, 45);
    lv_obj_set_style_bg_color(content, lv_color_hex(UIColors::BG_DARKER), 0);
    lv_obj_set_style_border_color(content, lv_color_hex(0xff6b35), 0);
    lv_obj_set_style_radius(content, 8, 0);
    lv_obj_set_style_pad_all(content, 15, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Starting message
    lv_obj_t *msg1 = lv_label_create(content);
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

        lv_obj_t *lbl_status = lv_label_create(content);
        lv_label_set_text(lbl_status, "WiFi AP Active");
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(UIColors::TEXT_GREEN), 0);
        lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_18, 0);

        lv_obj_t *lbl_ssid = lv_label_create(content);
        String ssidText = "SSID: " + apName;
        lv_label_set_text(lbl_ssid, ssidText.c_str());
        lv_obj_set_style_text_color(lbl_ssid, lv_color_hex(UIColors::TEXT_WHITE), 0);
        lv_obj_set_style_text_font(lbl_ssid, &lv_font_montserrat_14, 0);

        lv_obj_t *lbl_ip = lv_label_create(content);
        lv_label_set_text(lbl_ip, "IP: 192.168.4.1");
        lv_obj_set_style_text_color(lbl_ip, lv_color_hex(UIColors::BTN_BLUE), 0);
        lv_obj_set_style_text_font(lbl_ip, &lv_font_montserrat_18, 0);

        lv_obj_t *lbl_info = lv_label_create(content);
        lv_label_set_text(lbl_info, "Connect to WiFi AP\nOpen http://192.168.4.1");
        lv_obj_set_style_text_color(lbl_info, lv_color_hex(0xaaaaaa), 0);
        lv_obj_set_style_text_font(lbl_info, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_align(lbl_info, LV_TEXT_ALIGN_CENTER, 0);

        // Reboot button
        lv_obj_t *btn_reboot = lv_btn_create(content);
        lv_obj_set_size(btn_reboot, 120, 40);
        lv_obj_set_style_bg_color(btn_reboot, lv_color_hex(0xff6b6b), 0);
        lv_obj_add_event_cb(btn_reboot, webconf_reboot_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *lbl_btn = lv_label_create(btn_reboot);
        lv_label_set_text(lbl_btn, "Reboot");
        lv_obj_center(lbl_btn);

        // Force refresh
        lv_refr_now(NULL);
        lv_timer_handler();
        delay(100);
        lv_refr_now(NULL);

        // *** BLOCKING LOOP ***
        Serial.println("[UISettings] Entering Web-Conf blocking loop");
        webconf_reboot_requested = false;

        while (!webconf_reboot_requested) {
            uint32_t now = millis();
            lv_tick_inc(now - last_tick);
            last_tick = now;
            lv_timer_handler();
            yield();
            delay(10);
        }

        Serial.println("[UISettings] Rebooting from Web-Conf mode");
        ESP.restart();

    } else {
        lv_obj_t *lbl_error = lv_label_create(content);
        lv_label_set_text(lbl_error, "Failed to start AP!");
        lv_obj_set_style_text_color(lbl_error, lv_color_hex(0xff6b6b), 0);
        lv_obj_set_style_text_font(lbl_error, &lv_font_montserrat_18, 0);

        lv_obj_t *lbl_info = lv_label_create(content);
        lv_label_set_text(lbl_info, "Touch to go back");
        lv_obj_set_style_text_color(lbl_info, lv_color_hex(0xaaaaaa), 0);

        lv_timer_handler();
        delay(3000);

        lv_scr_load_anim(screen_setup, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
    }
}

void UISettings::showBootWebConfig() {
    Serial.println("[UISettings] First boot web-conf mode (NOCALL detected)");

    // Create web-conf screen
    screen_webconf = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_webconf, lv_color_hex(UIColors::BG_DARK), 0);

    // Title bar (orange)
    lv_obj_t *title_bar = lv_obj_create(screen_webconf);
    lv_obj_set_size(title_bar, UI_SCREEN_WIDTH, 35);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0xff6b35), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);

    lv_obj_t *title = lv_label_create(title_bar);
    lv_label_set_text(title, "First Time Setup");
    lv_obj_set_style_text_color(title, lv_color_hex(UIColors::TEXT_WHITE), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_center(title);

    // Content area
    lv_obj_t *content = lv_obj_create(screen_webconf);
    lv_obj_set_size(content, UI_SCREEN_WIDTH - 20, UI_SCREEN_HEIGHT - 55);
    lv_obj_set_pos(content, 10, 45);
    lv_obj_set_style_bg_color(content, lv_color_hex(UIColors::BG_DARKER), 0);
    lv_obj_set_style_border_color(content, lv_color_hex(0xff6b35), 0);
    lv_obj_set_style_radius(content, 8, 0);
    lv_obj_set_style_pad_all(content, 15, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Starting message
    lv_obj_t *msg1 = lv_label_create(content);
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

        lv_obj_t *lbl_status = lv_label_create(content);
        lv_label_set_text(lbl_status, "WiFi AP Active");
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(UIColors::TEXT_GREEN), 0);
        lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_18, 0);

        lv_obj_t *lbl_ssid = lv_label_create(content);
        String ssidText = "SSID: " + apName;
        lv_label_set_text(lbl_ssid, ssidText.c_str());
        lv_obj_set_style_text_color(lbl_ssid, lv_color_hex(UIColors::TEXT_WHITE), 0);
        lv_obj_set_style_text_font(lbl_ssid, &lv_font_montserrat_14, 0);

        lv_obj_t *lbl_pass = lv_label_create(content);
        lv_label_set_text(lbl_pass, "Pass: 1234567890");
        lv_obj_set_style_text_color(lbl_pass, lv_color_hex(0xffaa00), 0);
        lv_obj_set_style_text_font(lbl_pass, &lv_font_montserrat_14, 0);

        lv_obj_t *lbl_ip = lv_label_create(content);
        lv_label_set_text(lbl_ip, "IP: 192.168.4.1");
        lv_obj_set_style_text_color(lbl_ip, lv_color_hex(UIColors::BTN_BLUE), 0);
        lv_obj_set_style_text_font(lbl_ip, &lv_font_montserrat_18, 0);

        lv_obj_t *lbl_info = lv_label_create(content);
        lv_label_set_text(lbl_info, "Connect to WiFi AP\nthen open http://192.168.4.1\nto configure callsign & WiFi");
        lv_obj_set_style_text_color(lbl_info, lv_color_hex(0xaaaaaa), 0);
        lv_obj_set_style_text_font(lbl_info, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_align(lbl_info, LV_TEXT_ALIGN_CENTER, 0);

        // Reboot button
        lv_obj_t *btn_reboot = lv_btn_create(content);
        lv_obj_set_size(btn_reboot, 120, 40);
        lv_obj_set_style_bg_color(btn_reboot, lv_color_hex(0xff6b6b), 0);
        lv_obj_add_event_cb(btn_reboot, webconf_reboot_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *lbl_btn = lv_label_create(btn_reboot);
        lv_label_set_text(lbl_btn, "Reboot");
        lv_obj_center(lbl_btn);

        // Force refresh
        lv_refr_now(NULL);
        lv_timer_handler();

        // *** BLOCKING LOOP ***
        Serial.println("[UISettings] Entering First Boot Web-Conf blocking loop");
        webconf_reboot_requested = false;

        while (!webconf_reboot_requested) {
            uint32_t now = millis();
            lv_tick_inc(now - last_tick);
            last_tick = now;
            lv_timer_handler();
            yield();
            delay(10);
        }

        Serial.println("[UISettings] Rebooting from First Boot Web-Conf");
        ESP.restart();

    } else {
        lv_obj_t *lbl_error = lv_label_create(content);
        lv_label_set_text(lbl_error, "Failed to start AP!\nPlease reboot");
        lv_obj_set_style_text_color(lbl_error, lv_color_hex(0xff6b6b), 0);
        lv_obj_set_style_text_font(lbl_error, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_align(lbl_error, LV_TEXT_ALIGN_CENTER, 0);

        lv_timer_handler();
    }
}

// =============================================================================
// Public Navigation Functions
// =============================================================================

void UISettings::openSetup() {
    if (!screen_setup) {
        createSetupScreen();
    }
    lv_scr_load_anim(screen_setup, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
}

void UISettings::backToDashboard() {
    UIPopups::closeAll();
    lv_scr_load_anim(UIScreens::getMainScreen(), LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
}

void UISettings::backToSetup() {
    lv_scr_load_anim(screen_setup, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
}

// =============================================================================
// Screen Getters
// =============================================================================

lv_obj_t* UISettings::getSetupScreen() { return screen_setup; }
lv_obj_t* UISettings::getWifiScreen() { return screen_wifi; }
lv_obj_t* UISettings::getBluetoothScreen() { return screen_bluetooth; }

// =============================================================================
// Timer Cleanup
// =============================================================================

void UISettings::stopWifiTimer() {
    if (wifi_update_timer) {
        lv_timer_del(wifi_update_timer);
        wifi_update_timer = nullptr;
    }
}

void UISettings::stopBluetoothTimer() {
    if (bluetooth_update_timer) {
        lv_timer_del(bluetooth_update_timer);
        bluetooth_update_timer = nullptr;
    }
}

// =============================================================================
// Initialize
// =============================================================================

void UISettings::init() {
    Serial.println("[UISettings] Module initialized");
}

#endif // USE_LVGL_UI
