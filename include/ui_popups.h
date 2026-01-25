/* LVGL UI Popups Module
 * TX/RX notifications, beacon pending, WiFi eco, caps lock, add contact
 */

#ifndef UI_POPUPS_H
#define UI_POPUPS_H

#ifdef USE_LVGL_UI

namespace UIPopups {

    // Initialize popups module
    void init();

    // RX Message popup (for APRS messages addressed to user)
    void showMessage(const char* from, const char* message);

    // TX packet popup (green) - beacon/message sent
    void showTxPacket(const char* packet);

    // RX packet popup (blue) - LoRa frame received
    void showRxPacket(const char* packet);

    // Beacon pending popup (orange) - waiting for GPS fix
    void showBeaconPending();
    void hideBeaconPending();

    // WiFi Eco Mode popup
    void showWiFiEcoMode();

    // Caps Lock indicator popup
    void showCapsLockPopup(bool active);

    // Add contact prompt popup
    void showAddContactPrompt(const char* callsign);

    // Close all popups (useful when changing screens)
    void closeAll();

}

#endif // USE_LVGL_UI
#endif // UI_POPUPS_H
