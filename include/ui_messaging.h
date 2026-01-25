/* LVGL UI Messaging Module
 * Messages, conversations, contacts, compose, frames, stats screens
 */

#ifndef UI_MESSAGING_H
#define UI_MESSAGING_H

#ifdef USE_LVGL_UI

#include <lvgl.h>
#include <Arduino.h>

namespace UIMessaging {

    // Initialize messaging module
    void init();

    // Screen creation
    void createMsgScreen();
    void createComposeScreen();

    // Screen getters
    lv_obj_t* getMsgScreen();
    lv_obj_t* getMsgTabview();
    lv_obj_t* getContactsList();

    // Navigation
    void openMessagesScreen();
    void openComposeWithCallsign(const String& callsign);

    // Refresh functions
    void refreshConversationsList();
    void refreshContactsList();
    void refreshFramesList();
    void refreshStatsIfActive();  // Called from main loop to update stats tab

    // Physical keyboard handler for compose screen
    void handleComposeKeyboard(char key);
    bool isComposeScreenActive();

    // Caps lock state (for keyboard indicator)
    bool isCapsLockActive();

}

#endif // USE_LVGL_UI
#endif // UI_MESSAGING_H
