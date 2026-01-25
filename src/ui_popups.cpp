/* LVGL UI Popups Module
 * TX/RX notifications, beacon pending, WiFi eco, caps lock, add contact
 *
 * Extracted from lvgl_ui.cpp for modularization
 */

#ifdef USE_LVGL_UI

#include "ui_popups.h"
#include "ui_common.h"
#include "storage_utils.h"
#include <Arduino.h>
#include <lvgl.h>

namespace UIPopups {

// =============================================================================
// Module State
// =============================================================================

static bool initialized = false;

// =============================================================================
// RX Message Popup (APRS messages addressed to user)
// =============================================================================

static lv_obj_t *rx_msgbox = nullptr;
static lv_timer_t *rx_popup_timer = nullptr;

static void hide_rx_popup(lv_timer_t *timer) {
    if (rx_msgbox) {
        lv_msgbox_close(rx_msgbox);
        rx_msgbox = nullptr;
    }
    rx_popup_timer = nullptr;
}

void showMessage(const char *from, const char *message) {
    Serial.printf("[UIPopups] showMessage from %s: %s\n", from, message);

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

    // Create message box on top layer (visible on all screens)
    rx_msgbox = lv_msgbox_create(lv_layer_top(), ">>> MSG Rx <<<", content, NULL, false);
    lv_obj_set_size(rx_msgbox, 290, 140);
    lv_obj_set_style_bg_color(rx_msgbox, lv_color_hex(0x000022), 0);
    lv_obj_set_style_bg_opa(rx_msgbox, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(rx_msgbox, lv_color_hex(0x4488ff), 0);
    lv_obj_set_style_border_width(rx_msgbox, 3, 0);
    lv_obj_set_style_text_color(rx_msgbox, lv_color_hex(0x88ccff), 0);
    lv_obj_center(rx_msgbox);

    lv_refr_now(NULL);

    // Auto-close after 4 seconds
    rx_popup_timer = lv_timer_create(hide_rx_popup, 4000, NULL);
    lv_timer_set_repeat_count(rx_popup_timer, 1);

    Serial.println("[UIPopups] RX msgbox created");
}

// =============================================================================
// TX Packet Popup (Green - beacon/message sent)
// =============================================================================

static lv_obj_t *tx_msgbox = nullptr;
static lv_timer_t *tx_popup_timer = nullptr;

static void hide_tx_popup(lv_timer_t *timer) {
    if (tx_msgbox && lv_obj_is_valid(tx_msgbox)) {
        lv_obj_del(tx_msgbox);
        tx_msgbox = nullptr;
    }
    tx_popup_timer = nullptr;
}

void showTxPacket(const char *packet) {
    Serial.printf("[UIPopups] showTxPacket: %s\n", packet);

    // Always close beacon pending popup when TX happens
    hideBeaconPending();

    // Only show popup on dashboard
    if (lv_scr_act() != UIScreens::getMainScreen()) {
        Serial.println("[UIPopups] TX popup skipped (not on dashboard)");
        return;
    }

    // Close existing msgbox if any
    if (tx_msgbox && lv_obj_is_valid(tx_msgbox)) {
        lv_obj_del(tx_msgbox);
        tx_msgbox = nullptr;
    }
    if (tx_popup_timer) {
        lv_timer_del(tx_popup_timer);
        tx_popup_timer = nullptr;
    }

    // Create message box on top layer
    tx_msgbox = lv_msgbox_create(lv_layer_top(), "<<< TX >>>", packet, NULL, false);
    lv_obj_set_size(tx_msgbox, 280, 120);
    lv_obj_set_style_bg_color(tx_msgbox, lv_color_hex(0x002200), 0);
    lv_obj_set_style_bg_opa(tx_msgbox, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(tx_msgbox, lv_color_hex(0x006600), 0);
    lv_obj_set_style_border_width(tx_msgbox, 3, 0);
    lv_obj_set_style_text_color(tx_msgbox, lv_color_hex(0x006600), 0);
    lv_obj_center(tx_msgbox);

    lv_refr_now(NULL);

    // Auto-close after 3 seconds
    tx_popup_timer = lv_timer_create(hide_tx_popup, 3000, NULL);
    lv_timer_set_repeat_count(tx_popup_timer, 1);

    Serial.println("[UIPopups] TX msgbox created");
}

// =============================================================================
// RX LoRa Packet Popup (Blue - frame received)
// =============================================================================

static lv_obj_t *rx_lora_msgbox = nullptr;
static lv_timer_t *rx_lora_timer = nullptr;

static void hide_rx_lora_popup(lv_timer_t *timer) {
    if (rx_lora_msgbox && lv_obj_is_valid(rx_lora_msgbox)) {
        lv_obj_del(rx_lora_msgbox);
        rx_lora_msgbox = nullptr;
    }
    rx_lora_timer = nullptr;
}

void showRxPacket(const char *packet) {
    Serial.printf("[UIPopups] showRxPacket: %s\n", packet);

    // Only show popup on dashboard
    if (lv_scr_act() != UIScreens::getMainScreen()) {
        Serial.println("[UIPopups] RX popup skipped (not on dashboard)");
        return;
    }

    // Close existing msgbox if any
    if (rx_lora_msgbox && lv_obj_is_valid(rx_lora_msgbox)) {
        lv_obj_del(rx_lora_msgbox);
        rx_lora_msgbox = nullptr;
    }
    if (rx_lora_timer) {
        lv_timer_del(rx_lora_timer);
        rx_lora_timer = nullptr;
    }

    // Create message box on top layer
    rx_lora_msgbox = lv_msgbox_create(lv_layer_top(), ">>> RX <<<", packet, NULL, false);
    lv_obj_set_size(rx_lora_msgbox, 280, 120);
    lv_obj_set_style_bg_color(rx_lora_msgbox, lv_color_hex(0x000033), 0);
    lv_obj_set_style_bg_opa(rx_lora_msgbox, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(rx_lora_msgbox, lv_color_hex(0x4488ff), 0);
    lv_obj_set_style_border_width(rx_lora_msgbox, 3, 0);
    lv_obj_set_style_text_color(rx_lora_msgbox, lv_color_hex(0x88bbff), 0);
    lv_obj_center(rx_lora_msgbox);

    lv_refr_now(NULL);

    // Auto-close after 3 seconds
    rx_lora_timer = lv_timer_create(hide_rx_lora_popup, 3000, NULL);
    lv_timer_set_repeat_count(rx_lora_timer, 1);

    Serial.println("[UIPopups] RX LoRa msgbox created");
}

// =============================================================================
// Beacon Pending Popup (Orange - waiting for GPS)
// =============================================================================

static lv_obj_t *beacon_pending_msgbox = nullptr;
static lv_timer_t *beacon_pending_timer = nullptr;

static void hide_beacon_pending_popup(lv_timer_t *timer) {
    if (beacon_pending_msgbox && lv_obj_is_valid(beacon_pending_msgbox)) {
        lv_obj_del(beacon_pending_msgbox);
        beacon_pending_msgbox = nullptr;
    }
    beacon_pending_timer = nullptr;
}

void showBeaconPending() {
    Serial.println("[UIPopups] showBeaconPending");

    if (beacon_pending_msgbox && lv_obj_is_valid(beacon_pending_msgbox)) {
        lv_obj_del(beacon_pending_msgbox);
        beacon_pending_msgbox = nullptr;
    }
    if (beacon_pending_timer) {
        lv_timer_del(beacon_pending_timer);
        beacon_pending_timer = nullptr;
    }

    beacon_pending_msgbox = lv_msgbox_create(lv_layer_top(), "BEACON",
                                             "Waiting for GPS...", NULL, false);
    lv_obj_set_size(beacon_pending_msgbox, 200, 80);
    lv_obj_set_style_bg_color(beacon_pending_msgbox, lv_color_hex(0x332200), 0);
    lv_obj_set_style_bg_opa(beacon_pending_msgbox, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(beacon_pending_msgbox, lv_color_hex(0xaa6600), 0);
    lv_obj_set_style_border_width(beacon_pending_msgbox, 3, 0);
    lv_obj_set_style_text_color(beacon_pending_msgbox, lv_color_hex(0xffaa00), 0);
    lv_obj_center(beacon_pending_msgbox);

    lv_refr_now(NULL);

    // Auto-close after 5 seconds
    beacon_pending_timer = lv_timer_create(hide_beacon_pending_popup, 5000, NULL);
    lv_timer_set_repeat_count(beacon_pending_timer, 1);

    Serial.println("[UIPopups] Beacon pending msgbox created");
}

void hideBeaconPending() {
    if (beacon_pending_msgbox && lv_obj_is_valid(beacon_pending_msgbox)) {
        lv_obj_del(beacon_pending_msgbox);
        beacon_pending_msgbox = nullptr;
    }
    if (beacon_pending_timer) {
        lv_timer_del(beacon_pending_timer);
        beacon_pending_timer = nullptr;
    }
}

// =============================================================================
// WiFi Eco Mode Popup
// =============================================================================

static lv_obj_t *wifi_eco_msgbox = nullptr;
static lv_timer_t *wifi_eco_timer = nullptr;

static void hide_wifi_eco_popup(lv_timer_t *timer) {
    if (wifi_eco_msgbox && lv_obj_is_valid(wifi_eco_msgbox)) {
        lv_obj_del(wifi_eco_msgbox);
        wifi_eco_msgbox = nullptr;
    }
    wifi_eco_timer = nullptr;
}

void showWiFiEcoMode() {
    Serial.println("[UIPopups] showWiFiEcoMode");

    if (!UIScreens::isInitialized()) {
        Serial.println("[UIPopups] UI not initialized, skipping popup");
        return;
    }

    if (wifi_eco_msgbox && lv_obj_is_valid(wifi_eco_msgbox)) {
        lv_obj_del(wifi_eco_msgbox);
        wifi_eco_msgbox = nullptr;
    }
    if (wifi_eco_timer) {
        lv_timer_del(wifi_eco_timer);
        wifi_eco_timer = nullptr;
    }

    wifi_eco_msgbox = lv_msgbox_create(lv_layer_top(), "WiFi Eco Mode",
                                       "Connection failed\nRetry in 30 min", NULL, false);
    lv_obj_set_size(wifi_eco_msgbox, 240, 100);
    lv_obj_set_style_bg_color(wifi_eco_msgbox, lv_color_hex(0x332200), 0);
    lv_obj_set_style_bg_opa(wifi_eco_msgbox, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(wifi_eco_msgbox, lv_color_hex(0xffa500), 0);
    lv_obj_set_style_border_width(wifi_eco_msgbox, 3, 0);
    lv_obj_set_style_text_color(wifi_eco_msgbox, lv_color_hex(0xffa500), 0);
    lv_obj_center(wifi_eco_msgbox);

    lv_refr_now(NULL);

    // Auto-close after 2 seconds
    wifi_eco_timer = lv_timer_create(hide_wifi_eco_popup, 2000, NULL);
    lv_timer_set_repeat_count(wifi_eco_timer, 1);

    Serial.println("[UIPopups] WiFi Eco msgbox created");
}

// =============================================================================
// Caps Lock Popup
// =============================================================================

static lv_obj_t *capslock_msgbox = nullptr;
static lv_timer_t *capslock_timer = nullptr;

static void hide_capslock_popup(lv_timer_t *timer) {
    if (capslock_msgbox && lv_obj_is_valid(capslock_msgbox)) {
        lv_msgbox_close(capslock_msgbox);
    }
    capslock_msgbox = nullptr;
    capslock_timer = nullptr;
}

void showCapsLockPopup(bool active) {
    Serial.printf("[UIPopups] showCapsLockPopup: %s\n", active ? "ON" : "OFF");

    if (!UIScreens::isInitialized()) {
        Serial.println("[UIPopups] UI not initialized, skipping popup");
        return;
    }

    if (capslock_msgbox && lv_obj_is_valid(capslock_msgbox)) {
        lv_msgbox_close(capslock_msgbox);
        capslock_msgbox = nullptr;
    }
    if (capslock_timer) {
        lv_timer_del(capslock_timer);
        capslock_timer = nullptr;
    }

    const char *title = active ? "MAJ" : "maj";
    const char *msg = active ? "Caps Lock ON" : "Caps Lock OFF";
    capslock_msgbox = lv_msgbox_create(lv_layer_top(), title, msg, NULL, false);
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

    lv_refr_now(NULL);

    // Auto-close after 1.5 seconds
    capslock_timer = lv_timer_create(hide_capslock_popup, 1500, NULL);
    lv_timer_set_repeat_count(capslock_timer, 1);

    Serial.println("[UIPopups] Caps Lock popup created");
}

// =============================================================================
// Add Contact Prompt Popup
// =============================================================================

static lv_obj_t *add_contact_msgbox = nullptr;
static String pending_contact_callsign;

static void add_contact_btn_callback(lv_event_t *e) {
    (void)e;
    const char *btn_text = lv_msgbox_get_active_btn_text(add_contact_msgbox);

    bool accepted = (btn_text && strcmp(btn_text, "Yes") == 0);

    if (accepted) {
        Contact newContact;
        newContact.callsign = pending_contact_callsign;
        newContact.name = "";
        newContact.comment = "Auto-added";

        if (STORAGE_Utils::addContact(newContact)) {
            Serial.printf("[UIPopups] Contact %s added\n", pending_contact_callsign.c_str());
        } else {
            Serial.printf("[UIPopups] Failed to add contact %s\n", pending_contact_callsign.c_str());
        }
    } else {
        Serial.printf("[UIPopups] User declined contact %s\n", pending_contact_callsign.c_str());
    }

    if (add_contact_msgbox && lv_obj_is_valid(add_contact_msgbox)) {
        lv_obj_del(add_contact_msgbox);
        add_contact_msgbox = nullptr;
    }
    pending_contact_callsign = "";

    // Navigate to Contacts tab if accepted
    if (accepted) {
        lv_obj_t* msgScreen = UIScreens::getMsgScreen();
        lv_obj_t* tabview = UIScreens::getMsgTabview();
        if (msgScreen && tabview) {
            lv_scr_load_anim(msgScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
            lv_tabview_set_act(tabview, 2, LV_ANIM_ON); // 2 = Contacts tab
            UIScreens::populateContactsList();
            Serial.println("[UIPopups] Navigated to Contacts tab");
        }
    }
}

void showAddContactPrompt(const char *callsign) {
    Serial.printf("[UIPopups] showAddContactPrompt: %s\n", callsign);

    if (!UIScreens::isInitialized()) {
        Serial.println("[UIPopups] UI not initialized, skipping popup");
        return;
    }

    if (add_contact_msgbox && lv_obj_is_valid(add_contact_msgbox)) {
        lv_obj_del(add_contact_msgbox);
        add_contact_msgbox = nullptr;
    }

    pending_contact_callsign = String(callsign);

    char msg[64];
    snprintf(msg, sizeof(msg), "New contact:\n%s\n\nAdd to contacts?", callsign);

    static const char *btns[] = {"Yes", "No", ""};
    add_contact_msgbox = lv_msgbox_create(lv_layer_top(), "New Contact", msg, btns, false);
    lv_obj_set_size(add_contact_msgbox, 220, 140);
    lv_obj_set_style_bg_color(add_contact_msgbox, lv_color_hex(UIColors::BG_DARK), 0);
    lv_obj_set_style_bg_opa(add_contact_msgbox, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(add_contact_msgbox, lv_color_hex(UIColors::TEXT_GREEN), 0);
    lv_obj_set_style_border_width(add_contact_msgbox, 3, 0);
    lv_obj_set_style_text_color(add_contact_msgbox, lv_color_hex(UIColors::TEXT_WHITE), 0);
    lv_obj_center(add_contact_msgbox);

    lv_obj_t *btns_obj = lv_msgbox_get_btns(add_contact_msgbox);
    lv_obj_set_style_bg_color(btns_obj, lv_color_hex(UIColors::TEXT_GREEN), LV_PART_ITEMS);
    lv_obj_set_style_text_color(btns_obj, lv_color_hex(UIColors::TEXT_WHITE), LV_PART_ITEMS);

    lv_obj_add_event_cb(add_contact_msgbox, add_contact_btn_callback,
                        LV_EVENT_VALUE_CHANGED, NULL);

    lv_refr_now(NULL);

    Serial.println("[UIPopups] Add contact popup created");
}

// =============================================================================
// Close All Popups
// =============================================================================

void closeAll() {
    hideBeaconPending();

    if (tx_msgbox && lv_obj_is_valid(tx_msgbox)) {
        lv_obj_del(tx_msgbox);
        tx_msgbox = nullptr;
    }
    if (tx_popup_timer) {
        lv_timer_del(tx_popup_timer);
        tx_popup_timer = nullptr;
    }

    if (rx_msgbox && lv_obj_is_valid(rx_msgbox)) {
        lv_obj_del(rx_msgbox);
        rx_msgbox = nullptr;
    }
    if (rx_popup_timer) {
        lv_timer_del(rx_popup_timer);
        rx_popup_timer = nullptr;
    }

    if (rx_lora_msgbox && lv_obj_is_valid(rx_lora_msgbox)) {
        lv_obj_del(rx_lora_msgbox);
        rx_lora_msgbox = nullptr;
    }
    if (rx_lora_timer) {
        lv_timer_del(rx_lora_timer);
        rx_lora_timer = nullptr;
    }

    if (wifi_eco_msgbox && lv_obj_is_valid(wifi_eco_msgbox)) {
        lv_obj_del(wifi_eco_msgbox);
        wifi_eco_msgbox = nullptr;
    }
    if (wifi_eco_timer) {
        lv_timer_del(wifi_eco_timer);
        wifi_eco_timer = nullptr;
    }

    if (capslock_msgbox && lv_obj_is_valid(capslock_msgbox)) {
        lv_msgbox_close(capslock_msgbox);
        capslock_msgbox = nullptr;
    }
    if (capslock_timer) {
        lv_timer_del(capslock_timer);
        capslock_timer = nullptr;
    }

    if (add_contact_msgbox && lv_obj_is_valid(add_contact_msgbox)) {
        lv_obj_del(add_contact_msgbox);
        add_contact_msgbox = nullptr;
    }
}

// =============================================================================
// Initialize
// =============================================================================

void init() {
    initialized = true;
    Serial.println("[UIPopups] Module initialized");
}

} // namespace UIPopups

#endif // USE_LVGL_UI
