/* LVGL UI Common Definitions
 * Shared constants and screen accessors for UI modules
 */

#ifndef UI_COMMON_H
#define UI_COMMON_H

#ifdef USE_LVGL_UI

#include <lvgl.h>

// =============================================================================
// Display Constants
// =============================================================================

#define UI_SCREEN_WIDTH 320
#define UI_SCREEN_HEIGHT 240

// =============================================================================
// Color Constants (APRS-inspired palette)
// =============================================================================

namespace UIColors {
    constexpr uint32_t BG_DARK      = 0x1a1a2e;
    constexpr uint32_t BG_DARKER    = 0x0f0f23;
    constexpr uint32_t BG_HEADER    = 0x16213e;
    constexpr uint32_t TEXT_WHITE   = 0xffffff;
    constexpr uint32_t TEXT_GRAY    = 0x888888;
    constexpr uint32_t TEXT_CYAN    = 0x759a9e;
    constexpr uint32_t TEXT_ORANGE  = 0xffa500;
    constexpr uint32_t TEXT_RED     = 0xff6b6b;
    constexpr uint32_t TEXT_GREEN   = 0x006600;
    constexpr uint32_t TEXT_BLUE    = 0x0066cc;
    constexpr uint32_t TEXT_PURPLE  = 0xc792ea;
    constexpr uint32_t TEXT_YELLOW  = 0xffcc00;
    constexpr uint32_t BTN_RED      = 0xcc0000;
    constexpr uint32_t BTN_BLUE     = 0x0066cc;
    constexpr uint32_t BTN_GREEN    = 0x009933;
    constexpr uint32_t BTN_PURPLE   = 0xc792ea;
}

// =============================================================================
// Screen Accessors (implemented in lvgl_ui.cpp)
// =============================================================================

namespace UIScreens {
    // Get main dashboard screen (for popup visibility checks)
    lv_obj_t* getMainScreen();

    // Get messages screen
    lv_obj_t* getMsgScreen();

    // Get messages tabview
    lv_obj_t* getMsgTabview();

    // Get contacts list
    lv_obj_t* getContactsList();

    // Check if UI is initialized
    bool isInitialized();

    // Populate contacts list (needed by add contact popup)
    void populateContactsList();
}

#endif // USE_LVGL_UI
#endif // UI_COMMON_H
