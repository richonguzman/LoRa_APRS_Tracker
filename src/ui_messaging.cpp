/* LVGL UI Messaging Module
 * Messages, conversations, contacts, compose, frames, stats screens
 *
 * Extracted from lvgl_ui.cpp for modularization
 */

#ifdef USE_LVGL_UI

#include "ui_messaging.h"
#include "ui_common.h"
#include "ui_popups.h"
#include "ui_dashboard.h"
#include "lvgl_ui.h"

#include <Arduino.h>
#include <lvgl.h>
#include <vector>
#include <algorithm>
#include <TimeLib.h>

#include "configuration.h"
#include "msg_utils.h"
#include "storage_utils.h"

// External variables
extern Configuration Config;
extern SemaphoreHandle_t spiMutex;
extern uint32_t lastActivityTime;
extern bool screenDimmed;
extern uint8_t screenBrightness;

// Screen dimensions
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

namespace UIMessaging {

// =============================================================================
// Module State - Screen Pointers
// =============================================================================

static lv_obj_t *screen_msg = nullptr;
static lv_obj_t *msg_tabview = nullptr;
static lv_obj_t *list_aprs_global = nullptr;
static lv_obj_t *list_wlnk_global = nullptr;
static lv_obj_t *list_contacts_global = nullptr;
static lv_obj_t *list_frames_global = nullptr;
static lv_obj_t *cont_stats_global = nullptr;
static int current_msg_type = 0; // 0 = APRS, 1 = Winlink, 2 = Contacts, 3 = Frames, 4 = Stats

// Compose screen variables
static lv_obj_t *screen_compose = nullptr;
static lv_obj_t *compose_to_input = nullptr;
static lv_obj_t *compose_msg_input = nullptr;
static lv_obj_t *compose_keyboard = nullptr;
static lv_obj_t *current_focused_input = nullptr;
static bool compose_screen_active = false;
static lv_obj_t *compose_return_screen = nullptr;

// Conversation screen variables
static lv_obj_t *screen_conversation = nullptr;
static lv_obj_t *conversation_list = nullptr;
static String current_conversation_callsign = "";
static int pending_conversation_msg_delete = -1;
static bool conversation_msg_longpress_handled = false;
static lv_obj_t *conversation_confirm_msgbox = nullptr;
static lv_obj_t *msgbox_to_delete = nullptr;
static bool need_conversation_refresh = false;

// Contact edit screen variables
static lv_obj_t *screen_contact_edit = nullptr;
static lv_obj_t *contact_callsign_input = nullptr;
static lv_obj_t *contact_name_input = nullptr;
static lv_obj_t *contact_comment_input = nullptr;
static lv_obj_t *contact_edit_keyboard = nullptr;
static lv_obj_t *contact_current_input = nullptr;
static String editing_contact_callsign = "";

// Message detail popup
static lv_obj_t *detail_msgbox = nullptr;

// Delete confirmation
static lv_obj_t *confirm_msgbox = nullptr;
static lv_obj_t *confirm_msgbox_to_delete = nullptr;
static int pending_delete_msg_index = -1;
static bool msg_longpress_handled = false;
static bool need_aprs_list_refresh = false;

// Stats tab persistent widgets
static lv_obj_t *stats_title_lbl = nullptr;
static lv_obj_t *stats_table = nullptr;

// Frame item pool for object recycling (avoids alloc/dealloc churn)
static const int MAX_FRAME_ITEMS = 20;
struct FrameItemPool {
    lv_obj_t *container;
    lv_obj_t *summary_label;
    lv_obj_t *full_label;
};
static FrameItemPool frame_pool[MAX_FRAME_ITEMS];
static bool frame_pool_initialized = false;
static lv_style_t style_header_text; // Déclaration du style pour les en-têtes du tableau de statistiques
static bool style_header_text_initialized = false; // Flag pour l'initialisation du style

// Contact long-press flag
static bool contact_longpress_handled = false;

// Caps Lock and Symbol Lock state for physical keyboard
static bool capsLockActive = false;
static bool symbolLockActive = false;
static uint32_t lastShiftTime = 0;
static uint32_t lastSymbolTime = 0;
static const uint32_t DOUBLE_TAP_MS = 400;

// T-Deck keyboard special key codes
static const char KEY_SHIFT = 0x01;
static const char KEY_SYMBOL = 0x02;

// =============================================================================
// Forward Declarations
// =============================================================================

static void populate_msg_list(lv_obj_t *list, int type);
static void populate_contacts_list(lv_obj_t *list);
static void populate_frames_list(lv_obj_t *list);
static void populate_stats(lv_obj_t *cont);
static void create_conversation_screen(const String &callsign);
static void refresh_conversation_messages();
static void show_contact_edit_screen(const Contact *contact);
static void show_message_detail(const char *msg);
static void show_delete_confirmation(const char *message, int msg_index);
static void frame_item_clicked(lv_event_t *e);

// =============================================================================
// Module Initialization
// =============================================================================

void init() {
    // Nothing to initialize yet
}

// =============================================================================
// Screen Getters
// =============================================================================

lv_obj_t* getMsgScreen() { return screen_msg; }
lv_obj_t* getMsgTabview() { return msg_tabview; }
lv_obj_t* getContactsList() { return list_contacts_global; }
bool isComposeScreenActive() { return compose_screen_active; }
bool isCapsLockActive() { return capsLockActive; }

// =============================================================================
// Message Detail Popup
// =============================================================================

static void detail_msgbox_deleted_cb(lv_event_t *e) { detail_msgbox = nullptr; }

static void show_message_detail(const char *msg) {
    if (detail_msgbox && lv_obj_is_valid(detail_msgbox)) {
        lv_msgbox_close(detail_msgbox);
    }
    detail_msgbox = nullptr;

    detail_msgbox = lv_msgbox_create(NULL, "Message", msg, NULL, true);
    lv_obj_set_style_bg_color(detail_msgbox, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_text_color(detail_msgbox, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_width(detail_msgbox, SCREEN_WIDTH - 40);
    lv_obj_center(detail_msgbox);
    lv_obj_add_event_cb(detail_msgbox, detail_msgbox_deleted_cb, LV_EVENT_DELETE, NULL);
}

// =============================================================================
// Delete Confirmation System
// =============================================================================

static void delete_confirm_msgbox_timer_cb(lv_timer_t *timer) {
    Serial.println("[UIMessaging] delete_confirm_msgbox_timer_cb called");
    if (confirm_msgbox_to_delete && lv_obj_is_valid(confirm_msgbox_to_delete)) {
        lv_obj_del(confirm_msgbox_to_delete);
        Serial.println("[UIMessaging] Msgbox deleted");
    }
    confirm_msgbox_to_delete = nullptr;

    lv_obj_invalidate(lv_layer_top());
    lv_refr_now(NULL);

    if (need_aprs_list_refresh) {
        if (list_aprs_global) {
            Serial.println("[UIMessaging] Refreshing APRS list after delete");
            populate_msg_list(list_aprs_global, 0);
        }
        if (list_wlnk_global) {
            populate_msg_list(list_wlnk_global, 1);
        }
        need_aprs_list_refresh = false;
    }

    lv_timer_del(timer);
}

static void confirm_delete_cb(lv_event_t *e) {
    (void)e;
    if (!confirm_msgbox)
        return;

    const char *btn_text = lv_msgbox_get_active_btn_text(confirm_msgbox);
    Serial.printf("[UIMessaging] confirm_delete_cb: btn=%s, pending_index=%d\n",
                  btn_text ? btn_text : "NULL", pending_delete_msg_index);

    need_aprs_list_refresh = false;
    if (btn_text && strcmp(btn_text, "Yes") == 0) {
        if (pending_delete_msg_index == -1) {
            Serial.printf("[UIMessaging] Deleting ALL messages type %d\n", current_msg_type);
            MSG_Utils::deleteFile(current_msg_type);
        } else {
            MSG_Utils::deleteMessageByIndex(current_msg_type, pending_delete_msg_index);
        }
        need_aprs_list_refresh = true;
        Serial.println("[UIMessaging] need_aprs_list_refresh set to true");
    }

    confirm_msgbox_to_delete = confirm_msgbox;
    confirm_msgbox = nullptr;
    pending_delete_msg_index = -1;
    lv_timer_create(delete_confirm_msgbox_timer_cb, 10, NULL);
}

static void show_delete_confirmation(const char *message, int msg_index) {
    if (confirm_msgbox != nullptr)
        return;

    pending_delete_msg_index = msg_index;

    static const char *btns[] = {"Yes", "No", ""};
    confirm_msgbox = lv_msgbox_create(lv_layer_top(), "Confirmation", message, btns, false);
    lv_obj_set_style_bg_color(confirm_msgbox, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_opa(confirm_msgbox, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(confirm_msgbox, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_width(confirm_msgbox, 220);
    lv_obj_center(confirm_msgbox);
    lv_obj_add_event_cb(confirm_msgbox, confirm_delete_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

// =============================================================================
// Message List Callbacks
// =============================================================================

static void msg_item_clicked(lv_event_t *e) {
    if (msg_longpress_handled) {
        msg_longpress_handled = false;
        return;
    }

    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    if (label) {
        const char *text = lv_label_get_text(label);
        show_message_detail(text);
    }
}

static void msg_item_longpress(lv_event_t *e) {
    int msg_index = (int)(intptr_t)lv_event_get_user_data(e);
    Serial.printf("[UIMessaging] Message long-press: index %d\n", msg_index);
    msg_longpress_handled = true;
    show_delete_confirmation("Delete this message?", msg_index);
}

static void conversation_item_clicked(lv_event_t *e) {
    if (msg_longpress_handled) {
        msg_longpress_handled = false;
        return;
    }

    const char *callsign = (const char *)lv_event_get_user_data(e);
    if (!callsign)
        return;

    Serial.printf("[UIMessaging] Conversation clicked: %s\n", callsign);
    create_conversation_screen(String(callsign));
}

// =============================================================================
// Populate Message List
// =============================================================================

static void populate_msg_list(lv_obj_t *list, int type) {
    lv_obj_clean(list);

    if (type == 0) {
        // APRS messages - show conversations
        std::vector<String> conversations = MSG_Utils::getConversationsList();

        static std::vector<String> callsign_storage;
        callsign_storage.clear();

        if (conversations.size() == 0) {
            lv_obj_t *empty = lv_label_create(list);
            lv_label_set_text(empty, "No conversations");
            lv_obj_set_style_text_color(empty, lv_color_hex(0x888888), 0);
        } else {
            callsign_storage.reserve(conversations.size());
            for (size_t i = 0; i < conversations.size(); i++) {
                callsign_storage.push_back(conversations[i]);
            }

            // Display in order (already sorted by most recent first)
            // Static buffer for preview text (reused, no heap alloc per iteration)
            static char previewBuf[80];

            for (size_t i = 0; i < conversations.size(); i++) {
                std::vector<String> messages = MSG_Utils::getMessagesForContact(conversations[i]);
                const char* callsign = conversations[i].c_str();
                size_t callLen = strlen(callsign);
                if (callLen > 15) callLen = 15;

                memcpy(previewBuf, callsign, callLen);
                size_t pos = callLen;

                if (messages.size() > 0) {
                    const String& lastMsg = messages[messages.size() - 1];
                    const char* msgPtr = lastMsg.c_str();
                    // Find second comma to get message content
                    const char* comma1 = strchr(msgPtr, ',');
                    if (comma1) {
                        const char* comma2 = strchr(comma1 + 1, ',');
                        if (comma2) {
                            const char* content = comma2 + 1;
                            size_t contentLen = strlen(content);
                            previewBuf[pos++] = '\n';
                            if (contentLen > 30) {
                                memcpy(previewBuf + pos, content, 27);
                                pos += 27;
                                memcpy(previewBuf + pos, "...", 3);
                                pos += 3;
                            } else {
                                memcpy(previewBuf + pos, content, contentLen);
                                pos += contentLen;
                            }
                        }
                    }
                }
                previewBuf[pos] = '\0';

                lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_ENVELOPE, previewBuf);
                lv_obj_add_event_cb(btn, conversation_item_clicked, LV_EVENT_CLICKED,
                                    (void *)callsign_storage[i].c_str());
            }
        }
    } else {
        // Winlink messages
        MSG_Utils::loadMessagesFromMemory(1);
        std::vector<String> &messages = MSG_Utils::getLoadedWLNKMails();

        if (messages.size() == 0) {
            lv_obj_t *empty = lv_label_create(list);
            lv_label_set_text(empty, "No Winlink mails");
            lv_obj_set_style_text_color(empty, lv_color_hex(0x888888), 0);
        } else {
            for (int i = messages.size() - 1; i >= 0; i--) {
                lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_ENVELOPE, messages[i].c_str());
                lv_obj_add_event_cb(btn, msg_item_clicked, LV_EVENT_CLICKED, NULL);
                lv_obj_add_event_cb(btn, msg_item_longpress, LV_EVENT_LONG_PRESSED,
                                    (void *)(intptr_t)i);
            }
        }
    }
}

// =============================================================================
// Conversation Screen
// =============================================================================

static void delete_msgbox_timer_cb(lv_timer_t *timer) {
    if (msgbox_to_delete && lv_obj_is_valid(msgbox_to_delete)) {
        lv_obj_del(msgbox_to_delete);
    }
    msgbox_to_delete = nullptr;

    lv_obj_invalidate(lv_layer_top());
    lv_refr_now(NULL);

    if (need_conversation_refresh) {
        refresh_conversation_messages();
        need_conversation_refresh = false;
    }

    lv_timer_del(timer);
}

static void confirm_conversation_delete_cb(lv_event_t *e) {
    (void)e;
    if (!conversation_confirm_msgbox)
        return;

    const char *btn_text = lv_msgbox_get_active_btn_text(conversation_confirm_msgbox);

    need_conversation_refresh = false;
    if (btn_text && strcmp(btn_text, "Yes") == 0) {
        MSG_Utils::deleteMessageFromConversation(current_conversation_callsign,
                                                 pending_conversation_msg_delete);
        need_conversation_refresh = true;
    }

    msgbox_to_delete = conversation_confirm_msgbox;
    conversation_confirm_msgbox = nullptr;
    pending_conversation_msg_delete = -1;
    lv_timer_create(delete_msgbox_timer_cb, 10, NULL);
}

static void show_conversation_delete_confirmation(int msg_index) {
    if (conversation_confirm_msgbox != nullptr)
        return;

    pending_conversation_msg_delete = msg_index;

    static const char *btns[] = {"Yes", "No", ""};
    conversation_confirm_msgbox = lv_msgbox_create(
        lv_layer_top(), "Delete message?", "Delete this message?", btns, false);
    lv_obj_set_style_bg_color(conversation_confirm_msgbox, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_opa(conversation_confirm_msgbox, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(conversation_confirm_msgbox, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_width(conversation_confirm_msgbox, 240);
    lv_obj_center(conversation_confirm_msgbox);
    lv_obj_add_event_cb(conversation_confirm_msgbox, confirm_conversation_delete_cb,
                        LV_EVENT_VALUE_CHANGED, NULL);
}

static void conversation_msg_longpress(lv_event_t *e) {
    int msg_index = (int)(intptr_t)lv_event_get_user_data(e);
    Serial.printf("[UIMessaging] Conversation message long-press: index %d\n", msg_index);
    conversation_msg_longpress_handled = true;
    show_conversation_delete_confirmation(msg_index);
}

static void conversation_msg_clicked(lv_event_t *e) {
    if (conversation_msg_longpress_handled) {
        conversation_msg_longpress_handled = false;
        return;
    }
}

static void refresh_conversation_messages() {
    if (!conversation_list)
        return;

    lv_obj_clean(conversation_list);

    std::vector<String> messages = MSG_Utils::getMessagesForContact(current_conversation_callsign);

    if (messages.size() == 0) {
        lv_obj_t *empty = lv_label_create(conversation_list);
        lv_label_set_text(empty, "No messages");
        lv_obj_set_style_text_color(empty, lv_color_hex(0x888888), 0);
    } else {
        for (int i = messages.size() - 1; i >= 0; i--) {
            const char* msgPtr = messages[i].c_str();
            // Parse: "timestamp,direction,content" using C-style
            const char* comma1 = strchr(msgPtr, ',');
            if (!comma1) continue;
            const char* comma2 = strchr(comma1 + 1, ',');
            if (!comma2) continue;

            // Check direction (between comma1 and comma2)
            bool isOutgoing = (strncmp(comma1 + 1, "OUT", 3) == 0);
            const char* content = comma2 + 1;

            lv_obj_t *bubble_container = lv_obj_create(conversation_list);
            lv_obj_set_width(bubble_container, lv_pct(100));
            lv_obj_set_height(bubble_container, LV_SIZE_CONTENT);
            lv_obj_set_style_bg_opa(bubble_container, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(bubble_container, 0, 0);
            lv_obj_set_style_pad_all(bubble_container, 2, 0);
            lv_obj_clear_flag(bubble_container, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t *bubble = lv_obj_create(bubble_container);
            lv_obj_set_width(bubble, lv_pct(75));
            lv_obj_set_height(bubble, LV_SIZE_CONTENT);
            lv_obj_set_style_pad_all(bubble, 8, 0);
            lv_obj_set_style_radius(bubble, 10, 0);
            lv_obj_set_style_border_width(bubble, 0, 0);

            lv_obj_add_flag(bubble, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(bubble, conversation_msg_clicked, LV_EVENT_CLICKED,
                                (void *)(intptr_t)i);
            lv_obj_add_event_cb(bubble, conversation_msg_longpress, LV_EVENT_LONG_PRESSED,
                                (void *)(intptr_t)i);

            if (isOutgoing) {
                lv_obj_align(bubble, LV_ALIGN_RIGHT_MID, 0, 0);
                lv_obj_set_style_bg_color(bubble, lv_color_hex(0x82aaff), 0);
            } else {
                lv_obj_align(bubble, LV_ALIGN_LEFT_MID, 0, 0);
                lv_obj_set_style_bg_color(bubble, lv_color_hex(0x2a2a3e), 0);
            }

            lv_obj_t *msg_label = lv_label_create(bubble);
            lv_label_set_text(msg_label, content);  // Direct pointer, no String copy
            lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(msg_label, lv_pct(100));
            lv_obj_set_style_text_color(msg_label, lv_color_hex(0xffffff), 0);
        }
    }

    lv_obj_scroll_to_y(conversation_list, 0, LV_ANIM_OFF);
    Serial.printf("[UIMessaging] Conversation messages refreshed: %d messages\n", messages.size());
}

static void btn_conversation_back_clicked(lv_event_t *e) {
    if (screen_msg) {
        if (list_aprs_global) {
            populate_msg_list(list_aprs_global, 0);
        }
        lv_scr_load_anim(screen_msg, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
    }
}

static void btn_conversation_reply_clicked(lv_event_t *e);

static void create_conversation_screen(const String &callsign) {
    current_conversation_callsign = callsign;

    bool isRefresh = (screen_conversation != nullptr && lv_scr_act() == screen_conversation);
    lv_obj_t *old_screen = screen_conversation;

    screen_conversation = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_conversation, lv_color_hex(0x1a1a2e), 0);

    // Title bar
    lv_obj_t *title_bar = lv_obj_create(screen_conversation);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, 35);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x0f0f23), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    // Back button
    lv_obj_t *btn_back = lv_btn_create(title_bar);
    lv_obj_set_size(btn_back, 50, 28);
    lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x82aaff), 0);
    lv_obj_add_event_cb(btn_back, btn_conversation_back_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT);
    lv_obj_center(lbl_back);

    // Title
    lv_obj_t *title = lv_label_create(title_bar);
    lv_label_set_text(title, callsign.c_str());
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    // Reply button
    lv_obj_t *btn_reply = lv_btn_create(title_bar);
    lv_obj_set_size(btn_reply, 50, 28);
    lv_obj_align(btn_reply, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_bg_color(btn_reply, lv_color_hex(0x89ddff), 0);
    lv_obj_add_event_cb(btn_reply, btn_conversation_reply_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_reply = lv_label_create(btn_reply);
    lv_label_set_text(lbl_reply, LV_SYMBOL_EDIT);
    lv_obj_center(lbl_reply);

    // Chat container
    conversation_list = lv_obj_create(screen_conversation);
    lv_obj_set_size(conversation_list, SCREEN_WIDTH - 10, SCREEN_HEIGHT - 45);
    lv_obj_set_pos(conversation_list, 5, 38);
    lv_obj_set_style_bg_color(conversation_list, lv_color_hex(0x0f0f23), 0);
    lv_obj_set_style_border_width(conversation_list, 0, 0);
    lv_obj_set_style_pad_all(conversation_list, 5, 0);
    lv_obj_set_flex_flow(conversation_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(conversation_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Load and display messages
    std::vector<String> messages = MSG_Utils::getMessagesForContact(callsign);

    if (messages.size() == 0) {
        lv_obj_t *empty = lv_label_create(conversation_list);
        lv_label_set_text(empty, "No messages");
        lv_obj_set_style_text_color(empty, lv_color_hex(0x888888), 0);
    } else {
        for (int i = messages.size() - 1; i >= 0; i--) {
            const char* msgPtr = messages[i].c_str();
            // Parse: "timestamp,direction,content" using C-style
            const char* comma1 = strchr(msgPtr, ',');
            if (!comma1) continue;
            const char* comma2 = strchr(comma1 + 1, ',');
            if (!comma2) continue;

            // Check direction (between comma1 and comma2)
            bool isOutgoing = (strncmp(comma1 + 1, "OUT", 3) == 0);
            const char* content = comma2 + 1;

            lv_obj_t *bubble_container = lv_obj_create(conversation_list);
            lv_obj_set_width(bubble_container, lv_pct(100));
            lv_obj_set_height(bubble_container, LV_SIZE_CONTENT);
            lv_obj_set_style_bg_opa(bubble_container, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(bubble_container, 0, 0);
            lv_obj_set_style_pad_all(bubble_container, 2, 0);
            lv_obj_clear_flag(bubble_container, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t *bubble = lv_obj_create(bubble_container);
            lv_obj_set_width(bubble, lv_pct(75));
            lv_obj_set_height(bubble, LV_SIZE_CONTENT);
            lv_obj_set_style_pad_all(bubble, 8, 0);
            lv_obj_set_style_radius(bubble, 10, 0);
            lv_obj_set_style_border_width(bubble, 0, 0);

            lv_obj_add_flag(bubble, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(bubble, conversation_msg_clicked, LV_EVENT_CLICKED,
                                (void *)(intptr_t)i);
            lv_obj_add_event_cb(bubble, conversation_msg_longpress, LV_EVENT_LONG_PRESSED,
                                (void *)(intptr_t)i);

            if (isOutgoing) {
                lv_obj_align(bubble, LV_ALIGN_RIGHT_MID, 0, 0);
                lv_obj_set_style_bg_color(bubble, lv_color_hex(0x82aaff), 0);
            } else {
                lv_obj_align(bubble, LV_ALIGN_LEFT_MID, 0, 0);
                lv_obj_set_style_bg_color(bubble, lv_color_hex(0x2a2a3e), 0);
            }

            lv_obj_t *msg_label = lv_label_create(bubble);
            lv_label_set_text(msg_label, content);  // Direct pointer, no String copy
            lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(msg_label, lv_pct(100));
            lv_obj_set_style_text_color(msg_label, lv_color_hex(0xffffff), 0);
        }
    }

    lv_obj_scroll_to_y(conversation_list, 0, LV_ANIM_OFF);

    if (isRefresh) {
        lv_disp_load_scr(screen_conversation);
        if (old_screen) {
            lv_obj_del(old_screen);
        }
    } else {
        if (old_screen) {
            lv_obj_del(old_screen);
        }
        lv_scr_load_anim(screen_conversation, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
    }
    Serial.printf("[UIMessaging] Conversation screen created for %s with %d messages\n",
                  callsign.c_str(), messages.size());
}

// =============================================================================
// Contacts Screen
// =============================================================================

static void contact_item_clicked(lv_event_t *e);
static void contact_item_longpress(lv_event_t *e);

static void populate_contacts_list(lv_obj_t *list) {
    lv_obj_clean(list);

    std::vector<Contact> contacts = STORAGE_Utils::loadContacts();

    if (contacts.size() == 0) {
        lv_obj_t *empty = lv_label_create(list);
        lv_label_set_text(empty, "No contacts\nTap + to add");
        lv_obj_set_style_text_color(empty, lv_color_hex(0x888888), 0);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
    } else {
        // Static buffer for display text (reused, no heap alloc per iteration)
        static char displayBuf[100];

        for (size_t i = 0; i < contacts.size(); i++) {
            Contact *c = STORAGE_Utils::findContact(contacts[i].callsign);
            const char* callsign = contacts[i].callsign.c_str();
            const char* name = contacts[i].name.c_str();
            const char* comment = contacts[i].comment.c_str();

            size_t pos = 0;
            size_t callLen = strlen(callsign);
            if (callLen > 15) callLen = 15;
            memcpy(displayBuf, callsign, callLen);
            pos = callLen;

            if (name[0] != '\0') {
                memcpy(displayBuf + pos, " - ", 3);
                pos += 3;
                size_t nameLen = strlen(name);
                if (nameLen > 30) nameLen = 30;
                memcpy(displayBuf + pos, name, nameLen);
                pos += nameLen;
            }

            if (comment[0] != '\0') {
                displayBuf[pos++] = '\n';
                size_t commentLen = strlen(comment);
                if (commentLen > 40) commentLen = 40;
                memcpy(displayBuf + pos, comment, commentLen);
                pos += commentLen;
            }

            displayBuf[pos] = '\0';

            lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_CALL, displayBuf);
            lv_obj_add_event_cb(btn, contact_item_clicked, LV_EVENT_CLICKED, c);
            lv_obj_add_event_cb(btn, contact_item_longpress, LV_EVENT_LONG_PRESSED, c);
        }
    }
}

static void contact_item_clicked(lv_event_t *e) {
    if (contact_longpress_handled) {
        contact_longpress_handled = false;
        return;
    }

    Contact *contact = (Contact *)lv_event_get_user_data(e);
    if (!contact)
        return;

    Serial.printf("[UIMessaging] Contact clicked: %s - opening compose\n", contact->callsign.c_str());
    openComposeWithCallsign(contact->callsign);
}

static void contact_item_longpress(lv_event_t *e) {
    Contact *contact = (Contact *)lv_event_get_user_data(e);
    if (!contact)
        return;
    Serial.printf("[UIMessaging] Contact long-press: %s\n", contact->callsign.c_str());
    contact_longpress_handled = true;
    show_contact_edit_screen(contact);
}

// =============================================================================
// Contact Edit Screen
// =============================================================================

static void contact_edit_input_focused(lv_event_t *e) {
    lv_obj_t *ta = lv_event_get_target(e);
    contact_current_input = ta;
    lv_keyboard_set_textarea(contact_edit_keyboard, ta);
}

static void contact_edit_keyboard_event(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_obj_add_flag(contact_edit_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

static void btn_contact_save_clicked(lv_event_t *e) {
    Contact contact;
    contact.callsign = String(lv_textarea_get_text(contact_callsign_input));
    contact.name = String(lv_textarea_get_text(contact_name_input));
    contact.comment = String(lv_textarea_get_text(contact_comment_input));

    contact.callsign.trim();
    contact.callsign.toUpperCase();

    if (contact.callsign.length() == 0) {
        show_message_detail("Callsign required");
        return;
    }

    bool success;
    if (editing_contact_callsign.length() > 0) {
        success = STORAGE_Utils::updateContact(editing_contact_callsign, contact);
    } else {
        success = STORAGE_Utils::addContact(contact);
    }

    if (success) {
        Serial.printf("[UIMessaging] Contact saved: %s\n", contact.callsign.c_str());
        lv_scr_load(screen_msg);
        if (list_contacts_global) {
            populate_contacts_list(list_contacts_global);
        }
    } else {
        show_message_detail("Failed to save contact\n(duplicate callsign?)");
    }
}

static void btn_contact_delete_clicked(lv_event_t *e) {
    if (editing_contact_callsign.length() > 0) {
        bool success = STORAGE_Utils::removeContact(editing_contact_callsign);
        if (success) {
            Serial.printf("[UIMessaging] Contact deleted: %s\n", editing_contact_callsign.c_str());
            lv_scr_load(screen_msg);
            if (list_contacts_global) {
                populate_contacts_list(list_contacts_global);
            }
        }
    }
}

static void btn_contact_cancel_clicked(lv_event_t *e) {
    lv_scr_load(screen_msg);
}

static void btn_contact_kb_toggle_clicked(lv_event_t *e) {
    if (lv_obj_has_flag(contact_edit_keyboard, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(contact_edit_keyboard, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(contact_edit_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

// =============================================================================
// Contact Edit Screen
// =============================================================================

static void show_contact_edit_screen(const Contact *contact) {
    // Determine if we are editing an existing contact or adding a new one
    if (contact) {
        editing_contact_callsign = contact->callsign;
    } else {
        editing_contact_callsign = "";
    }

    // Create the screen and UI objects if they don't exist yet
    if (!screen_contact_edit) {
        screen_contact_edit = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen_contact_edit, lv_color_hex(0x0f0f23), 0);

        // --- Title Bar ---
        lv_obj_t *title_bar = lv_obj_create(screen_contact_edit);
        lv_obj_set_size(title_bar, SCREEN_WIDTH, 35);
        lv_obj_set_pos(title_bar, 0, 0);
        lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x1a1a2e), 0);
        lv_obj_set_style_border_width(title_bar, 0, 0);
        lv_obj_set_style_pad_all(title_bar, 5, 0);

        // Back button
        lv_obj_t *btn_back = lv_btn_create(title_bar);
        lv_obj_set_size(btn_back, 30, 25);
        lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x444444), 0);
        lv_obj_add_event_cb(btn_back, btn_contact_cancel_clicked, LV_EVENT_CLICKED, NULL);
        lv_obj_t *lbl_back = lv_label_create(btn_back);
        lv_label_set_text(lbl_back, LV_SYMBOL_LEFT);
        lv_obj_center(lbl_back);

        // Title
        lv_obj_t *title = lv_label_create(title_bar);
        lv_label_set_text(title, "Contact");
        lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

        // Save button
        lv_obj_t *btn_save = lv_btn_create(title_bar);
        lv_obj_set_size(btn_save, 40, 25);
        lv_obj_align(btn_save, LV_ALIGN_RIGHT_MID, -50, 0);
        lv_obj_set_style_bg_color(btn_save, lv_color_hex(0x00aa55), 0);
        lv_obj_add_event_cb(btn_save, btn_contact_save_clicked, LV_EVENT_CLICKED, NULL);
        lv_obj_t *lbl_save = lv_label_create(btn_save);
        lv_label_set_text(lbl_save, LV_SYMBOL_OK);
        lv_obj_center(lbl_save);

        // Delete button
        lv_obj_t *btn_del = lv_btn_create(title_bar);
        lv_obj_set_size(btn_del, 40, 25);
        lv_obj_align(btn_del, LV_ALIGN_RIGHT_MID, -5, 0);
        lv_obj_set_style_bg_color(btn_del, lv_color_hex(0xff4444), 0);
        lv_obj_add_event_cb(btn_del, btn_contact_delete_clicked, LV_EVENT_CLICKED, NULL);
        lv_obj_t *lbl_del = lv_label_create(btn_del);
        lv_label_set_text(lbl_del, LV_SYMBOL_TRASH);
        lv_obj_center(lbl_del);

        // --- Form Container ---
        lv_obj_t *form = lv_obj_create(screen_contact_edit);
        lv_obj_set_size(form, SCREEN_WIDTH - 10, 160);
        lv_obj_set_pos(form, 5, 40);
        lv_obj_set_scrollbar_mode(form, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_set_style_bg_color(form, lv_color_hex(0x1a1a2e), 0);
        lv_obj_set_style_border_width(form, 0, 0);
        lv_obj_set_style_pad_all(form, 5, 0);
        lv_obj_set_flex_flow(form, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(form, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

        // Callsign input
        lv_obj_t *lbl_call = lv_label_create(form);
        lv_label_set_text(lbl_call, "Callsign:");
        lv_obj_set_style_text_color(lbl_call, lv_color_hex(0x82aaff), 0);

        contact_callsign_input = lv_textarea_create(form);
        lv_obj_set_size(contact_callsign_input, lv_pct(100), 32);
        lv_textarea_set_one_line(contact_callsign_input, true);
        lv_textarea_set_placeholder_text(contact_callsign_input, "e.g. F4ABC-9");
        lv_obj_add_event_cb(contact_callsign_input, contact_edit_input_focused, LV_EVENT_FOCUSED, NULL);

        // Name input
        lv_obj_t *lbl_name = lv_label_create(form);
        lv_label_set_text(lbl_name, "Name:");
        lv_obj_set_style_text_color(lbl_name, lv_color_hex(0x82aaff), 0);

        contact_name_input = lv_textarea_create(form);
        lv_obj_set_size(contact_name_input, lv_pct(100), 32);
        lv_textarea_set_one_line(contact_name_input, true);
        lv_textarea_set_placeholder_text(contact_name_input, "e.g. Jean");
        lv_obj_add_event_cb(contact_name_input, contact_edit_input_focused, LV_EVENT_FOCUSED, NULL);

        // Comment/Note input
        lv_obj_t *lbl_comment = lv_label_create(form);
        lv_label_set_text(lbl_comment, "Note:");
        lv_obj_set_style_text_color(lbl_comment, lv_color_hex(0x82aaff), 0);

        contact_comment_input = lv_textarea_create(form);
        lv_obj_set_size(contact_comment_input, lv_pct(100), 32);
        lv_textarea_set_one_line(contact_comment_input, true);
        lv_textarea_set_placeholder_text(contact_comment_input, "e.g. Paris");
        lv_obj_add_event_cb(contact_comment_input, contact_edit_input_focused, LV_EVENT_FOCUSED, NULL);

        // --- Virtual Keyboard Button ---
        lv_obj_t *btn_kb = lv_btn_create(screen_contact_edit);
        lv_obj_set_size(btn_kb, 40, 30);
        lv_obj_set_pos(btn_kb, SCREEN_WIDTH - 50, 165);
        lv_obj_set_style_bg_color(btn_kb, lv_color_hex(0x555555), 0);
        // Link the toggle callback (Fixes "unused function" warning)
        lv_obj_add_event_cb(btn_kb, btn_contact_kb_toggle_clicked, LV_EVENT_CLICKED, NULL);
        lv_obj_t *lbl_kb = lv_label_create(btn_kb);
        lv_label_set_text(lbl_kb, LV_SYMBOL_KEYBOARD);
        lv_obj_center(lbl_kb);

        // --- Virtual Keyboard ---
        contact_edit_keyboard = lv_keyboard_create(screen_contact_edit);
        lv_obj_set_size(contact_edit_keyboard, SCREEN_WIDTH, 100);
        lv_obj_align(contact_edit_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_add_flag(contact_edit_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_event_cb(contact_edit_keyboard, contact_edit_keyboard_event, LV_EVENT_ALL, NULL);
    }

    // --- Data Filling (Fixes the empty fields issue) ---
    if (contact) {
        // Edit mode: populate with existing contact data
        lv_textarea_set_text(contact_callsign_input, contact->callsign.c_str());
        lv_textarea_set_text(contact_name_input, contact->name.c_str());
        lv_textarea_set_text(contact_comment_input, contact->comment.c_str());
    } else {
        // Add mode: clear all fields
        lv_textarea_set_text(contact_callsign_input, "");
        lv_textarea_set_text(contact_name_input, "");
        lv_textarea_set_text(contact_comment_input, "");
    }

    // Load the screen
    lv_scr_load(screen_contact_edit);
    
    // --- Initial Setup for Physical Keyboard and Focus ---
    // 1. Set the initial target for physical keyboard typing
    contact_current_input = contact_callsign_input;
    
    // 2. Link virtual keyboard to the first field
    if (contact_edit_keyboard) {
        lv_keyboard_set_textarea(contact_edit_keyboard, contact_callsign_input);
    }

    // 3. Visual Focus (Orange glow on the Callsign field)
    lv_obj_clear_state(contact_name_input, LV_STATE_FOCUSED);
    lv_obj_clear_state(contact_comment_input, LV_STATE_FOCUSED);
    lv_obj_add_state(contact_callsign_input, LV_STATE_FOCUSED);
}

// =============================================================================
// Frames List (Optimized with Object Pooling)
// =============================================================================

static void frame_item_clicked(lv_event_t *e) {
    lv_obj_t *cont = lv_event_get_target(e);
    // Hidden label is the 2nd child (index 1)
    lv_obj_t *hidden_label = lv_obj_get_child(cont, 1);
    if (hidden_label) {
        const char *full_text = lv_label_get_text(hidden_label);
        show_message_detail(full_text);
    }
}

// Initialize the frame item pool (called once on first use)
static void init_frame_pool(lv_obj_t *list) {
    if (frame_pool_initialized) return;

    for (int i = 0; i < MAX_FRAME_ITEMS; i++) {
        lv_obj_t *cont = lv_obj_create(list);
        lv_obj_set_width(cont, lv_pct(100));
        lv_obj_set_height(cont, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(cont, lv_color_hex(0x0a0a14), 0);
        lv_obj_set_style_pad_all(cont, 6, 0);
        lv_obj_set_style_border_width(cont, 1, LV_PART_MAIN);
        lv_obj_set_style_border_side(cont, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
        lv_obj_set_style_border_color(cont, lv_color_hex(0x333344), 0);
        lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(cont, frame_item_clicked, LV_EVENT_CLICKED, NULL);

        lv_obj_t *label_summary = lv_label_create(cont);
        lv_obj_set_width(label_summary, lv_pct(100));
        lv_label_set_long_mode(label_summary, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_font(label_summary, &lv_font_mono_14, 0);

        lv_obj_t *label_full = lv_label_create(cont);
        lv_obj_add_flag(label_full, LV_OBJ_FLAG_HIDDEN);

        // Hide all items by default
        lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN);

        frame_pool[i].container = cont;
        frame_pool[i].summary_label = label_summary;
        frame_pool[i].full_label = label_full;
    }

    frame_pool_initialized = true;
    Serial.println("[UIMessaging] Frame pool initialized (20 items)");
}

// Update a single frame item in the pool (no String alloc - uses static buffer)
static void update_frame_item(int index, const String& rawLine) {
    if (index < 0 || index >= MAX_FRAME_ITEMS) return;

    FrameItemPool& item = frame_pool[index];
    const char* raw = rawLine.c_str();
    size_t rawLen = rawLine.length();

    // Détermine si c'est une trame directe (pour la couleur)
    bool isDirect = (rawLen >= 3 && raw[0] == '[' && raw[1] == 'D' && raw[2] == ']');

    // Buffer statique pour le résumé (réutilisé, pas d'allocation de heap)
    static char summary[80];
    summary[0] = '\0'; // Initialise le buffer

    // Extraire la partie HH:MM:SS (commence à l'index 14 de raw, 8 caractères de long)
    // Ex: "[D]YYYY-MM-DD HH:MM:SS GMT: ..." -> HH:MM:SS est à raw[14]
    char timeStr[9]; // HH:MM:SS\0
    if (rawLen >= 22) { // S'assurer que la ligne est assez longue pour contenir l'heure
        strncpy(timeStr, raw + 14, 8);
        timeStr[8] = '\0';
    } else {
        strcpy(timeStr, "        "); // Fallback si la trame est trop courte
    }

    // Déterminer le début du contenu réel de la trame APRS (après "[X]YYYY-MM-JJ HH:MM:SS GMT: ")
    // Le contenu APRS commence à l'index 27 (3+10+1+8+5 = 27 caractères à sauter)
    const char* aprsFrameContent = raw;
    if (rawLen >= 27) { // Longueur suffisante pour le préfixe complet
        aprsFrameContent = raw + 27;
    } else {
        aprsFrameContent = raw; // Si trop court, traiter la ligne entière comme contenu
    }

    const char* arrowPos = strchr(aprsFrameContent, '>');
    if (arrowPos) {
        // Extrait l'émetteur
        size_t senderLen = arrowPos - aprsFrameContent;
        // Extrait la destination/chemin
        const char* pathStart = arrowPos + 1;
        const char* pathEnd = strchr(pathStart, ':'); // Trouve la fin du chemin/début du texte du message
        if (!pathEnd) pathEnd = strchr(pathStart, ','); // S'il n'y a pas de texte de message, la virgule sépare les éléments du chemin
        if (!pathEnd) pathEnd = pathStart + strlen(pathStart); // S'il n'y a pas d'éléments de chemin, le chemin est tout ce qui reste
        size_t pathDisplayLen = pathEnd - pathStart;

        // Construire le résumé: "HH:MM:SS EMETTEUR > CHEMIN"
        snprintf(summary, sizeof(summary), "%s %.*s > %.*s",
                 timeStr,
                 (int)senderLen, aprsFrameContent,
                 (int)pathDisplayLen, pathStart);
    } else {
        // Pas de '>' trouvé, juste l'heure et le contenu de la trame
        snprintf(summary, sizeof(summary), "%s %s", timeStr, aprsFrameContent);
    }
    
    // Troncation finale si le résumé est toujours trop long
    if (strlen(summary) >= sizeof(summary) - 4) { // Réserve de l'espace pour "..."
        summary[sizeof(summary) - 4] = '\0';
        strcat(summary, "...");
    }

    // Met à jour le texte (LVGL fait une copie interne)
    lv_label_set_text(item.summary_label, summary);
    lv_label_set_text(item.full_label, raw); // Ligne raw complète pour la popup de détail

    // Met à jour la couleur en fonction de direct/digipeated
    lv_obj_set_style_text_color(item.summary_label,
        isDirect ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_ORANGE), 0);

    // Affiche l'élément
    lv_obj_clear_flag(item.container, LV_OBJ_FLAG_HIDDEN);
}

static void populate_frames_list(lv_obj_t *list) {
    // Initialize pool on first call
    if (!frame_pool_initialized) {
        init_frame_pool(list);
    }

    const std::vector<String> &frames = STORAGE_Utils::getLastFrames(20);

    // Update existing pool items with new data
    size_t frameCount = std::min(frames.size(), (size_t)MAX_FRAME_ITEMS);
    for (size_t i = 0; i < frameCount; i++) {
        update_frame_item(i, frames[i]);
    }

    // Hide unused items
    for (size_t i = frameCount; i < MAX_FRAME_ITEMS; i++) {
        lv_obj_add_flag(frame_pool[i].container, LV_OBJ_FLAG_HIDDEN);
    }
}

// =============================================================================
// Stats Tab
// =============================================================================

static void populate_stats(lv_obj_t *cont) {
    const std::vector<StationStats> &stations = STORAGE_Utils::getStationStats();

    uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < 8000) {
        return;
    }

    char buf[32];

    // Create title and table if not exists
    if (!stats_title_lbl) {
        stats_title_lbl = lv_label_create(cont);
        if (stats_title_lbl) {
            lv_label_set_text(stats_title_lbl, "Stations Heard");
            lv_obj_set_style_text_color(stats_title_lbl, lv_color_hex(0x4CAF50), 0);
            lv_obj_set_style_text_font(stats_title_lbl, &lv_font_montserrat_14, 0);
        }

        stats_table = lv_table_create(cont);
        if (stats_table) {
            lv_table_set_col_cnt(stats_table, 4); // Suppression de la colonne "Time"
            lv_obj_set_width(stats_table, lv_pct(100));
            lv_obj_set_style_bg_color(stats_table, lv_color_hex(0x0f0f23), 0);
            lv_obj_set_style_border_color(stats_table, lv_color_hex(0x333344), 0);
            lv_obj_set_style_pad_all(stats_table, 2, LV_PART_ITEMS);
            // Couleur de texte par défaut pour les éléments du tableau (lignes de données)
            lv_obj_set_style_text_color(stats_table, lv_color_hex(0x759a9e), LV_PART_ITEMS);

            // Initialiser le style des en-têtes une seule fois
            if (!style_header_text_initialized) { // Vérifie si le style a déjà été initialisé
                lv_style_init(&style_header_text);
                lv_style_set_text_color(&style_header_text, lv_color_hex(0xFF8C00)); // Orange
                lv_style_set_text_font(&style_header_text, &lv_font_montserrat_14); // Spécifier la police pour les en-têtes
                style_header_text_initialized = true; // Marque le style comme initialisé
            }
            // Appliquer le style pour les en-têtes (utilisant l'état personnalisé)
            lv_obj_add_style(stats_table, &style_header_text, LV_PART_ITEMS | LV_STATE_USER_1);

            // Largeurs de colonne ajustées (total ~290px pour la zone de contenu)
            // Ancien: Time (0, 65), Station (1, 80), Pkts (2, 38), RSSI (3, 45), SNR (4, 45)
            // Largeurs de colonne ajustées pour utiliser la largeur complète (~310px) et décaler Pkts
            // Largeurs de colonne ajustées pour utiliser la largeur complète (~310px)
            // Largeurs de colonne ajustées pour utiliser la largeur complète de 310px
            // Largeurs de colonne ajustées pour un espacement équilibré et pour remplir la largeur de 310px
            // Largeurs de colonne ajustées pour un espacement équilibré et pour remplir la largeur de 280px
            // Largeurs de colonne ajustées pour un espacement équilibré et pour remplir la largeur de 280px
            // Nouveau: Station (0, 100), Pkts (1, 40), RSSI (2, 70), SNR (3, 70) -> 100+40+70+70 = 280
            lv_table_set_col_width(stats_table, 0, 100);  // Station
            lv_table_set_col_width(stats_table, 1, 40);   // Pkts
            lv_table_set_col_width(stats_table, 2, 70);   // RSSI
            lv_table_set_col_width(stats_table, 3, 70);   // SNR

            // Centrer le texte dans toutes les colonnes du tableau
            lv_obj_set_style_text_align(stats_table, LV_TEXT_ALIGN_CENTER, LV_PART_ITEMS);
        }
    }

    // Update table
    if (stats_table) {
        // Ligne d'en-tête
        lv_table_set_cell_value(stats_table, 0, 0, "Station");
        lv_table_set_cell_value(stats_table, 0, 1, "Pkts");
        lv_table_set_cell_value(stats_table, 0, 2, "RSSI");
        lv_table_set_cell_value(stats_table, 0, 3, "SNR");
        // Appliquer l'état personnalisé aux cellules d'en-tête pour le style
        lv_table_add_cell_ctrl(stats_table, 0, 0, LV_TABLE_CELL_CTRL_CUSTOM_1);
        lv_table_add_cell_ctrl(stats_table, 0, 1, LV_TABLE_CELL_CTRL_CUSTOM_1);
        lv_table_add_cell_ctrl(stats_table, 0, 2, LV_TABLE_CELL_CTRL_CUSTOM_1);
        lv_table_add_cell_ctrl(stats_table, 0, 3, LV_TABLE_CELL_CTRL_CUSTOM_1);

        if (stations.size() == 0) {
            lv_table_set_row_cnt(stats_table, 2);
            lv_table_set_cell_value(stats_table, 1, 0, "No stations");
            lv_table_set_cell_value(stats_table, 1, 1, "heard yet");
            lv_table_set_cell_value(stats_table, 1, 2, "");
            lv_table_set_cell_value(stats_table, 1, 3, "");
        } else {
            // Trie les stations par 'count' (nombre de paquets) décroissant
            std::vector<size_t> indices(stations.size());
            for (size_t i = 0; i < stations.size(); i++) indices[i] = i;
            std::sort(indices.begin(), indices.end(), [&stations](size_t a, size_t b) {
                return stations[a].count > stations[b].count; // Tri par nombre de paquets
            });

            size_t rowCount = (indices.size() < 10) ? indices.size() : 10;
            lv_table_set_row_cnt(stats_table, rowCount + 1); // +1 pour l'en-tête

            for (size_t i = 0; i < rowCount; i++) {
                const StationStats &s = stations[indices[i]];
                int row = i + 1; // Saute la ligne d'en-tête

                // Station
                lv_table_set_cell_value(stats_table, row, 0, s.callsign.c_str());

                // Packets
                snprintf(buf, sizeof(buf), "%d", s.count);
                lv_table_set_cell_value(stats_table, row, 1, buf);

                // RSSI
                snprintf(buf, sizeof(buf), "%d", s.lastRssi);
                lv_table_set_cell_value(stats_table, row, 2, buf);

                // SNR
                snprintf(buf, sizeof(buf), "%.1f", s.lastSnr);
                lv_table_set_cell_value(stats_table, row, 3, buf);
            }
        }
    }
}

// =============================================================================
// Tab Changed Callback
// =============================================================================

static void msg_tab_changed(lv_event_t *e) {
    lv_obj_t *tabview = lv_event_get_target(e);

    if (!tabview || tabview != msg_tabview) {
        return;
    }

    uint16_t tab_idx = lv_tabview_get_tab_act(tabview);

    if (tab_idx > 4 || tab_idx == 0xFFFF) {
        return;
    }

    if (current_msg_type != (int)tab_idx) {
        current_msg_type = (int)tab_idx;
        Serial.printf("[UIMessaging] Messages tab changed to %d\n", current_msg_type);

        if (current_msg_type == 1 && list_wlnk_global) {
            populate_msg_list(list_wlnk_global, 1);
        }
        if (current_msg_type == 2 && list_contacts_global) {
            populate_contacts_list(list_contacts_global);
        }
        if (current_msg_type == 3 && list_frames_global) {
            populate_frames_list(list_frames_global);
            STORAGE_Utils::clearFramesDirty();  // Data is now up-to-date
        }
        if (current_msg_type == 4 && cont_stats_global) {
            populate_stats(cont_stats_global);
            STORAGE_Utils::clearStatsDirty();  // Data is now up-to-date
        }
    }
}

// =============================================================================
// Delete All Messages
// =============================================================================

static void btn_delete_msgs_clicked(lv_event_t *e) {
    Serial.printf("[UIMessaging] Delete all button pressed, type %d\n", current_msg_type);

    if (current_msg_type >= 2) {
        return;
    }

    const char *msg = (current_msg_type == 0) ? "Delete all APRS messages?"
                                              : "Delete all Winlink mails?";
    show_delete_confirmation(msg, -1);
}

// =============================================================================
// Compose Screen
// =============================================================================

static void compose_keyboard_event(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_obj_add_flag(compose_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

static void compose_input_focused(lv_event_t *e) {
    lv_obj_t *ta = lv_event_get_target(e);
    current_focused_input = ta;
    lv_keyboard_set_textarea(compose_keyboard, ta);
}

static void btn_send_msg_clicked(lv_event_t *e) {
    const char *to = lv_textarea_get_text(compose_to_input);
    const char *msg = lv_textarea_get_text(compose_msg_input);

    if (strlen(to) > 0 && strlen(msg) > 0) {
        Serial.printf("[UIMessaging] Sending message to %s: %s\n", to, msg);
        MSG_Utils::addToOutputBuffer(1, String(to), String(msg)); // String copies still occur within addToOutputBuffer

        // Save sent message
        if (xSemaphoreTakeRecursive(spiMutex, portMAX_DELAY) == pdTRUE) {
            File msgFile = STORAGE_Utils::openFile("/aprsMessages.txt", FILE_APPEND);
            if (msgFile) {
                char sentMsgBuf[256]; // Utilisation d'un buffer statique pour éviter les allocations String
                snprintf(sentMsgBuf, sizeof(sentMsgBuf), "%s,>%s", to, msg);
                msgFile.println(sentMsgBuf);
                msgFile.close();
                MSG_Utils::loadNumMessages();
            }
            xSemaphoreGiveRecursive(spiMutex);
        }

        // Save to conversation file
        MSG_Utils::saveToConversation(String(to), String(msg), true);

        // Show confirmation popup
        UIPopups::showTxPacket(msg);

        // Clear inputs and go back
        lv_textarea_set_text(compose_to_input, "");
        lv_textarea_set_text(compose_msg_input, "");
        compose_screen_active = false;

        if (compose_return_screen && lv_obj_is_valid(compose_return_screen)) {
            if (compose_return_screen == screen_conversation) {
                refresh_conversation_messages();
            }
            lv_scr_load_anim(compose_return_screen, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
        } else {
            UIDashboard::returnToDashboard();
        }
    }
}

static void btn_compose_back_clicked(lv_event_t *e) {
    compose_screen_active = false;
    if (compose_return_screen && lv_obj_is_valid(compose_return_screen)) {
        lv_scr_load_anim(compose_return_screen, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
    } else {
        UIDashboard::returnToDashboard();
    }
}

void createComposeScreen() {
    if (screen_compose)
        return;

    screen_compose = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_compose, lv_color_hex(0x1a1a2e), 0);

    // Title bar
    lv_obj_t *title_bar = lv_obj_create(screen_compose);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, 35);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x006600), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 5, 0);

    // Back button
    lv_obj_t *btn_back = lv_btn_create(title_bar);
    lv_obj_set_size(btn_back, 60, 25);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x16213e), 0);
    lv_obj_add_event_cb(btn_back, btn_compose_back_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "< BACK");
    lv_obj_center(lbl_back);

    // Title
    lv_obj_t *title = lv_label_create(title_bar);
    lv_label_set_text(title, "Compose Message");
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 30, 0);

    // Send button
    lv_obj_t *btn_send = lv_btn_create(title_bar);
    lv_obj_set_size(btn_send, 60, 25);
    lv_obj_align(btn_send, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_bg_color(btn_send, lv_color_hex(0x16213e), 0);
    lv_obj_add_event_cb(btn_send, btn_send_msg_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_send = lv_label_create(btn_send);
    lv_label_set_text(lbl_send, "SEND");
    lv_obj_center(lbl_send);

    // "To:" label and input
    lv_obj_t *lbl_to = lv_label_create(screen_compose);
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
    lv_obj_t *lbl_msg = lv_label_create(screen_compose);
    lv_label_set_text(lbl_msg, "Msg:");
    lv_obj_set_pos(lbl_msg, 10, 80);
    lv_obj_set_style_text_color(lbl_msg, lv_color_hex(0xffffff), 0);

    compose_msg_input = lv_textarea_create(screen_compose);
    lv_obj_set_size(compose_msg_input, SCREEN_WIDTH - 20, 50);
    lv_obj_set_pos(compose_msg_input, 10, 95);
    lv_textarea_set_placeholder_text(compose_msg_input, "Your message...");
    lv_obj_add_event_cb(compose_msg_input, compose_input_focused, LV_EVENT_FOCUSED, NULL);

    // Virtual Keyboard (hidden by default)
    compose_keyboard = lv_keyboard_create(screen_compose);
    lv_obj_set_size(compose_keyboard, SCREEN_WIDTH, 90);
    lv_obj_align(compose_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(compose_keyboard, compose_to_input);
    lv_obj_add_event_cb(compose_keyboard, compose_keyboard_event, LV_EVENT_ALL, NULL);
    lv_obj_add_flag(compose_keyboard, LV_OBJ_FLAG_HIDDEN);

    // Toggle keyboard button
    lv_obj_t *btn_kbd = lv_btn_create(screen_compose);
    lv_obj_set_size(btn_kbd, 40, 30);
    lv_obj_align(btn_kbd, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_set_style_bg_color(btn_kbd, lv_color_hex(0x444466), 0);
    lv_obj_add_event_cb(btn_kbd, [](lv_event_t *e) {
        if (lv_obj_has_flag(compose_keyboard, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_clear_flag(compose_keyboard, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(compose_keyboard, LV_OBJ_FLAG_HIDDEN);
        }
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_kbd = lv_label_create(btn_kbd);
    lv_label_set_text(lbl_kbd, LV_SYMBOL_KEYBOARD);
    lv_obj_center(lbl_kbd);

    Serial.println("[UIMessaging] Compose screen created");
}

// Reply button uses compose
static void btn_conversation_reply_clicked(lv_event_t *e) {
    if (current_conversation_callsign.length() > 0) {
        createComposeScreen();
        compose_screen_active = true;
        compose_return_screen = lv_scr_act();
        lv_textarea_set_text(compose_to_input, current_conversation_callsign.c_str());
        current_focused_input = compose_msg_input;
        lv_keyboard_set_textarea(compose_keyboard, compose_msg_input);
        lv_scr_load_anim(screen_compose, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
    }
}

// =============================================================================
// Messages Screen Buttons
// =============================================================================

static void btn_back_clicked(lv_event_t *e) {
    Serial.println("[UIMessaging] BACK button pressed");
    UIPopups::closeAll();
    UIDashboard::returnToDashboard();
}

static void btn_compose_clicked(lv_event_t *e) {
    createComposeScreen();
    compose_screen_active = true;
    compose_return_screen = lv_scr_act();
    current_focused_input = compose_to_input;
    lv_scr_load_anim(screen_compose, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
}

static void btn_add_contact_clicked(lv_event_t *e) {
    show_contact_edit_screen(nullptr);
}

// =============================================================================
// Create Messages Screen
// =============================================================================

void createMsgScreen() {
    screen_msg = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_msg, lv_color_hex(0x1a1a2e), 0);

    // Title bar
    lv_obj_t *title_bar = lv_obj_create(screen_msg);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, 35);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x0000cc), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 5, 0);

    // Back button
    lv_obj_t *btn_back = lv_btn_create(title_bar);
    lv_obj_set_size(btn_back, 50, 25);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x16213e), 0);
    lv_obj_add_event_cb(btn_back, btn_back_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT);
    lv_obj_center(lbl_back);

    // Title
    lv_obj_t *title = lv_label_create(title_bar);
    lv_label_set_text(title, "Messages");
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, -40, 0);

   // --- 1. Compose button (Green) ---
    lv_obj_t *btn_compose = lv_btn_create(title_bar);
    lv_obj_set_size(btn_compose, 40, 25);
    lv_obj_align(btn_compose, LV_ALIGN_RIGHT_MID, -50, 0);
    lv_obj_set_style_bg_color(btn_compose, lv_color_hex(0x00aa55), 0);
    lv_obj_add_event_cb(btn_compose, btn_compose_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_compose = lv_label_create(btn_compose);
    lv_label_set_text(lbl_compose, LV_SYMBOL_EDIT);
    lv_obj_center(lbl_compose);

    // --- 2. Add Contact button (Blue) ---
    lv_obj_t *btn_add_contact = lv_btn_create(title_bar);
    lv_obj_set_size(btn_add_contact, 40, 25);
    lv_obj_align(btn_add_contact, LV_ALIGN_RIGHT_MID, -95, 0); 
    lv_obj_set_style_bg_color(btn_add_contact, lv_color_hex(0x82aaff), 0);
    lv_obj_add_event_cb(btn_add_contact, btn_add_contact_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_add = lv_label_create(btn_add_contact);
    lv_label_set_text(lbl_add, LV_SYMBOL_PLUS);
    lv_obj_center(lbl_add);

    // --- 3. Delete All button (Red) ---
    lv_obj_t *btn_delete = lv_btn_create(title_bar);
    lv_obj_set_size(btn_delete, 40, 25);
    lv_obj_align(btn_delete, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_bg_color(btn_delete, lv_color_hex(0xff4444), 0);
    lv_obj_add_event_cb(btn_delete, btn_delete_msgs_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_delete = lv_label_create(btn_delete);
    lv_label_set_text(lbl_delete, LV_SYMBOL_TRASH);
    lv_obj_center(lbl_delete);
    
    // Tabview
    msg_tabview = lv_tabview_create(screen_msg, LV_DIR_TOP, 30);
    if (!msg_tabview) {
        Serial.println("[UIMessaging] ERROR: Failed to create msg_tabview!");
        return;
    }
    lv_obj_set_size(msg_tabview, SCREEN_WIDTH, SCREEN_HEIGHT - 35);
    lv_obj_set_pos(msg_tabview, 0, 35);
    lv_obj_set_style_bg_color(msg_tabview, lv_color_hex(0x0f0f23), 0);
    lv_obj_add_event_cb(msg_tabview, msg_tab_changed, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *tab_bar = lv_tabview_get_tab_btns(msg_tabview);
    if (tab_bar) {
        lv_obj_set_style_pad_column(tab_bar, 0, 0);
        // Inactive tabs: white background (LVGL default), orange text
        lv_obj_set_style_text_color(tab_bar, lv_color_hex(0xFF8C00), LV_PART_ITEMS); // Orange
        lv_obj_set_style_border_color(tab_bar, lv_color_hex(0x9DB2CC), LV_PART_ITEMS);
        lv_obj_set_style_border_width(tab_bar, 1, LV_PART_ITEMS);
        lv_obj_set_style_border_side(tab_bar, LV_BORDER_SIDE_RIGHT, LV_PART_ITEMS);
        // Active tab: light blue background, dark blue border and text
        lv_obj_set_style_bg_color(tab_bar, lv_color_hex(0x86B8F7), LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_text_color(tab_bar, lv_color_hex(0x0952AD), LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(tab_bar, lv_color_hex(0x0952AD), LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_border_width(tab_bar, 3, LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_border_side(tab_bar, LV_BORDER_SIDE_BOTTOM, LV_PART_ITEMS | LV_STATE_CHECKED);
    }

    // APRS Tab
    lv_obj_t *tab_aprs = lv_tabview_add_tab(msg_tabview, "APRS");
    if (tab_aprs) {
        lv_obj_set_style_bg_color(tab_aprs, lv_color_hex(0x0f0f23), 0);
        lv_obj_set_style_pad_all(tab_aprs, 5, 0);

        list_aprs_global = lv_list_create(tab_aprs);
        if (list_aprs_global) {
            lv_obj_set_size(list_aprs_global, lv_pct(100), lv_pct(100));
            lv_obj_set_style_bg_color(list_aprs_global, lv_color_hex(0x0f0f23), 0);
            lv_obj_set_style_border_width(list_aprs_global, 0, 0);
            populate_msg_list(list_aprs_global, 0);
        }
    }

    // Winlink Tab
    lv_obj_t *tab_wlnk = lv_tabview_add_tab(msg_tabview, "Winlink");
    if (tab_wlnk) {
        lv_obj_set_style_bg_color(tab_wlnk, lv_color_hex(0x0f0f23), 0);
        lv_obj_set_style_pad_all(tab_wlnk, 5, 0);

        list_wlnk_global = lv_list_create(tab_wlnk);
        if (list_wlnk_global) {
            lv_obj_set_size(list_wlnk_global, lv_pct(100), lv_pct(100));
            lv_obj_set_style_bg_color(list_wlnk_global, lv_color_hex(0x0f0f23), 0);
            lv_obj_set_style_border_width(list_wlnk_global, 0, 0);
        }
    }

    // Contacts Tab
    lv_obj_t *tab_contacts = lv_tabview_add_tab(msg_tabview, "Contacts");
    if (tab_contacts) {
        lv_obj_set_style_bg_color(tab_contacts, lv_color_hex(0x0f0f23), 0);
        lv_obj_set_style_pad_all(tab_contacts, 5, 0);

        list_contacts_global = lv_list_create(tab_contacts);
        if (list_contacts_global) {
            lv_obj_set_size(list_contacts_global, lv_pct(100), lv_pct(100));
            lv_obj_set_style_bg_color(list_contacts_global, lv_color_hex(0x0f0f23), 0);
            lv_obj_set_style_border_width(list_contacts_global, 0, 0);
        }
    }

    // Frames Tab
    lv_obj_t *tab_frames = lv_tabview_add_tab(msg_tabview, "Frames");
    if (tab_frames) {
        lv_obj_set_style_bg_color(tab_frames, lv_color_hex(0x0f0f23), 0);
        lv_obj_set_style_pad_all(tab_frames, 5, 0);

        list_frames_global = lv_list_create(tab_frames);
        if (list_frames_global) {
            lv_obj_set_size(list_frames_global, lv_pct(100), lv_pct(100));
            lv_obj_set_style_bg_color(list_frames_global, lv_color_hex(0x0f0f23), 0);
            lv_obj_set_style_border_width(list_frames_global, 0, 0);
        }
    }

    // Stats Tab
    lv_obj_t *tab_stats = lv_tabview_add_tab(msg_tabview, "Stats");
    if (tab_stats) {
        lv_obj_set_style_bg_color(tab_stats, lv_color_hex(0x0f0f23), 0);
        lv_obj_set_style_pad_all(tab_stats, 5, 0); // Réduction du padding pour augmenter la largeur utile

        cont_stats_global = lv_obj_create(tab_stats);
        if (cont_stats_global) {
            lv_obj_set_size(cont_stats_global, lv_pct(100), lv_pct(100));
            lv_obj_set_style_bg_color(cont_stats_global, lv_color_hex(0x0f0f23), 0);
            lv_obj_set_style_border_width(cont_stats_global, 0, 0);
            lv_obj_set_flex_flow(cont_stats_global, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(cont_stats_global, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
            lv_obj_set_style_pad_row(cont_stats_global, 4, 0);
        }
    }

    Serial.println("[UIMessaging] Messages screen created with tabs");
}

// =============================================================================
// Public Navigation Functions
// =============================================================================

void openMessagesScreen() {
    if (!screen_msg) {
        createMsgScreen();
    } else {
        if (list_aprs_global) {
            populate_msg_list(list_aprs_global, 0);
        }
    }
    lv_scr_load_anim(screen_msg, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
}

void openComposeWithCallsign(const String& callsign) {
    createComposeScreen();
    compose_return_screen = lv_scr_act();

    if (compose_to_input) {
        lv_textarea_set_text(compose_to_input, callsign.c_str());
    }

    compose_screen_active = true;
    current_focused_input = compose_msg_input;
    lv_keyboard_set_textarea(compose_keyboard, compose_msg_input);

    lv_scr_load_anim(screen_compose, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
    Serial.printf("[UIMessaging] Opening compose for: %s\n", callsign.c_str());
}

void refreshConversationsList() {
    if (list_aprs_global) {
        populate_msg_list(list_aprs_global, 0);
    }
}

void refreshContactsList() {
    if (list_contacts_global) {
        populate_contacts_list(list_contacts_global);
    }
}

void refreshFramesList() {
    // Only refresh if data changed AND Messages screen is active AND Frames tab is visible
    if (!STORAGE_Utils::isFramesDirty()) return;

    if (screen_msg && lv_scr_act() == screen_msg && msg_tabview) {
        if (lv_tabview_get_tab_act(msg_tabview) == 3 && list_frames_global) {
            // With object pooling, just update all items (no alloc/dealloc)
            populate_frames_list(list_frames_global);
            lv_obj_scroll_to_y(list_frames_global, 0, LV_ANIM_ON);
            STORAGE_Utils::clearFramesDirty();
        }
    }
}

void refreshStatsIfActive() {
    // Only refresh if data changed AND Stats tab is currently active
    if (!STORAGE_Utils::isStatsDirty()) return;

    if (screen_msg && lv_scr_act() == screen_msg && msg_tabview) {
        if (lv_tabview_get_tab_act(msg_tabview) == 4 && cont_stats_global) {
            populate_stats(cont_stats_global);
            STORAGE_Utils::clearStatsDirty();
        }
    }
}

// =============================================================================
// Physical Keyboard Handler
// =============================================================================

static char getSymbolChar(char key) {
    switch (key) {
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
    case 'z': return '7';
    case 'x': return '8';
    case 'c': return '9';
    case 'v': return '?';
    case 'b': return '!';
    case 'n': return ',';
    case 'm': return '.';
    default: return key;
    }
}

void handleComposeKeyboard(char key) {
    // 1. Réinitialiser le minuteur d'inactivité et réveil écran
    lastActivityTime = millis();
    if (screenDimmed) {
        screenDimmed = false;
        #ifdef BOARD_BL_PIN
            analogWrite(BOARD_BL_PIN, screenBrightness);
        #endif
        // On continue pour traiter la touche immédiatement
    }

    // 2. Identifier la cible (Compose Message OU Contact Edit)
    lv_obj_t* target_input = nullptr;
    bool is_compose_mode = false;
    bool is_contact_mode = false;

    // Cas A : Mode Compose Message
    if (compose_screen_active && current_focused_input) {
        target_input = current_focused_input;
        is_compose_mode = true;
    }
    // Cas B : Mode Édition Contact (Vérifier si l'écran est affiché)
    else if (screen_contact_edit && lv_scr_act() == screen_contact_edit) {
        // Sécurité : si le pointeur est null, on force le premier champ
        if (!contact_current_input) contact_current_input = contact_callsign_input;
        
        target_input = contact_current_input;
        is_contact_mode = true;
    }

    // Si aucun champ texte n'est actif, on sort
    if (!target_input) return;

    uint32_t now = millis();

    // --- Gestion des touches modificatrices (Shift/Symbol) ---
    if (key == KEY_SHIFT) {
        if (now - lastShiftTime < DOUBLE_TAP_MS) {
            capsLockActive = !capsLockActive;
            symbolLockActive = false;
            UIPopups::showCapsLockPopup(capsLockActive);
            Serial.printf("[UIMessaging] Caps Lock %s\n", capsLockActive ? "ON" : "OFF");
        }
        lastShiftTime = now;
        return;
    }

    if (key == KEY_SYMBOL) {
        if (now - lastSymbolTime < DOUBLE_TAP_MS) {
            symbolLockActive = !symbolLockActive;
            capsLockActive = false;
            Serial.printf("[UIMessaging] Symbol Lock %s\n", symbolLockActive ? "ON" : "OFF");
        }
        lastSymbolTime = now;
        return;
    }

    // --- Gestion Backspace ---
    if (key == '\b' || key == 0x08) {
        lv_textarea_del_char(target_input);
        return;
    }

    // --- Gestion Navigation (Enter / Tab) ---
    if (key == '\n' || key == '\r' || key == '\t') {
        
        if (is_compose_mode) {
            // Navigation Compose (To <-> Msg)
            if (current_focused_input == compose_to_input) {
                lv_obj_clear_state(compose_to_input, LV_STATE_FOCUSED);
                lv_obj_add_state(compose_msg_input, LV_STATE_FOCUSED);
                current_focused_input = compose_msg_input;
            } else if (key == '\t') { // Shift+Tab simulé par Tab simple pour remonter
                lv_obj_clear_state(compose_msg_input, LV_STATE_FOCUSED);
                lv_obj_add_state(compose_to_input, LV_STATE_FOCUSED);
                current_focused_input = compose_to_input;
            }
        }
        else if (is_contact_mode) {
            // Navigation Contact (Callsign -> Name -> Note -> Callsign)
            if (contact_current_input == contact_callsign_input) {
                lv_obj_clear_state(contact_callsign_input, LV_STATE_FOCUSED);
                lv_obj_add_state(contact_name_input, LV_STATE_FOCUSED);
                contact_current_input = contact_name_input;
            } else if (contact_current_input == contact_name_input) {
                lv_obj_clear_state(contact_name_input, LV_STATE_FOCUSED);
                lv_obj_add_state(contact_comment_input, LV_STATE_FOCUSED);
                contact_current_input = contact_comment_input;
            } else if (contact_current_input == contact_comment_input) {
                // Sur le dernier champ, Entrée ou Tab boucle au début
                lv_obj_clear_state(contact_comment_input, LV_STATE_FOCUSED);
                lv_obj_add_state(contact_callsign_input, LV_STATE_FOCUSED);
                contact_current_input = contact_callsign_input;
            }
            
            // Mise à jour du clavier virtuel si présent
            if (contact_edit_keyboard) {
                lv_keyboard_set_textarea(contact_edit_keyboard, contact_current_input);
            }
        }
        return;
    }

    // --- Saisie de caractères ---
    char output = key;
    if (symbolLockActive && key >= 'a' && key <= 'z') {
        output = getSymbolChar(key);
    } else if (capsLockActive && key >= 'a' && key <= 'z') {
        output = key - 32; // Convert to uppercase
    }

    // IMPORTANT : On écrit dans target_input (qui peut être Compose OU Contact)
    char str[2] = {output, '\0'};
    lv_textarea_add_text(target_input, str);
}
} // namespace UIMessaging

#endif // USE_LVGL_UI
