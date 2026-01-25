/* LVGL UI Settings Module
 * Setup menu, frequency, speed, callsign, display, sound, WiFi, Bluetooth screens
 *
 * Extracted from lvgl_ui.cpp for modularization
 */

#ifdef USE_LVGL_UI

#include "ui_settings.h"
#include "ui_common.h"
#include "ui_popups.h"
#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>

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
extern int loraIndex;
extern int loraIndexSize;
extern int myBeaconsIndex;
extern int myBeaconsSize;
extern lv_obj_t *label_callsign;

// Forward declaration from lvgl_ui.cpp
void drawAPRSSymbol(const char *symbolStr);

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
    if (label_callsign) {
        lv_label_set_text(label_callsign, Config.beacons[myBeaconsIndex].callsign.c_str());
    }

    // Update APRS symbol for new beacon
    String fullSymbol = Config.beacons[myBeaconsIndex].overlay +
                        Config.beacons[myBeaconsIndex].symbol;
    drawAPRSSymbol(fullSymbol.c_str());

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
// Display Screen (placeholder)
// =============================================================================

void UISettings::createDisplayScreen() {
    // TODO: Extract from lvgl_ui.cpp
    Serial.println("[UISettings] Display screen - not yet implemented");
}

// =============================================================================
// Sound Screen (placeholder)
// =============================================================================

void UISettings::createSoundScreen() {
    // TODO: Extract from lvgl_ui.cpp
    Serial.println("[UISettings] Sound screen - not yet implemented");
}

// =============================================================================
// WiFi Screen (placeholder)
// =============================================================================

void UISettings::createWifiScreen() {
    // TODO: Extract from lvgl_ui.cpp
    Serial.println("[UISettings] WiFi screen - not yet implemented");
}

// =============================================================================
// Bluetooth Screen (placeholder)
// =============================================================================

void UISettings::createBluetoothScreen() {
    // TODO: Extract from lvgl_ui.cpp
    Serial.println("[UISettings] Bluetooth screen - not yet implemented");
}

// =============================================================================
// Web-Conf callbacks
// =============================================================================

static void webconf_reboot_cb(lv_event_t *e) {
    webconf_reboot_requested = true;
}

static void setup_item_webconf(lv_event_t *e) {
    Serial.println("[UISettings] Web-Conf Mode selected");
    UISettings::openWebConf();
}

void UISettings::openWebConf() {
    // TODO: Extract blocking web-conf mode from lvgl_ui.cpp
    Serial.println("[UISettings] Web-Conf mode - not yet implemented");
}

void UISettings::showBootWebConfig() {
    // TODO: Extract from lvgl_ui.cpp - blocking mode
    Serial.println("[UISettings] Boot Web-Conf - not yet implemented");
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
