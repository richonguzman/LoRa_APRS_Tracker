/* LVGL UI for T-Deck Plus
 * Touchscreen-based user interface using LVGL library
 */

#ifndef LVGL_UI_H
#define LVGL_UI_H

#ifdef USE_LVGL_UI

#include <lvgl.h>

// External declarations for custom JetBrains Mono fonts
// These must match the 'Font Name' you used in the converter
LV_FONT_DECLARE(lv_font_mono_12);
LV_FONT_DECLARE(lv_font_mono_14);
LV_FONT_DECLARE(lv_font_mono_16);


namespace LVGL_UI {
    void showSplashScreen(uint8_t loraIndex, const char* version);
    void showInitScreen();                      // Show init screen with spinner
    void updateInitStatus(const char* status);  // Update init status text
    void hideInitScreen();                      // Hide init screen before dashboard
    void setup();
    void loop();
    void updateGPS(double lat, double lng, double alt, double speed, int sats, double hdop);
    void updateBattery(int percent, float voltage);
    void updateLoRa(const char* lastRx, int rssi);
    void refreshLoRaInfo();  // Refresh freq/speed display after settings change
    void updateWiFi(bool connected, int rssi);
    void updateCallsign(const char* callsign);
    void updateTime(int day, int month, int year, int hour, int minute, int second);
    void showMessage(const char* from, const char* message);
    void showTxPacket(const char* packet);  // Display TX packet on screen (green)
    void showRxPacket(const char* packet);  // Display RX packet on screen (blue)
    void showBeaconPending();  // Display beacon pending popup (orange) - waiting for GPS
    void hideBeaconPending();  // Hide beacon pending popup
    void closeAllPopups();     // Close all popups (TX, RX, beacon pending)
    void showWiFiEcoMode();  // Display WiFi eco mode popup
    void handleComposeKeyboard(char key);  // Handle physical keyboard for compose screen
    void showCapsLockPopup(bool active);  // Display Caps Lock status popup
    void showBootWebConfig();  // Show web-conf screen at boot (blocking, LVGL-based)
    void showAddContactPrompt(const char* callsign);  // Prompt to add unknown sender as contact
    void open_compose_with_callsign(const String& callsign); // Open compose screen with pre-filled callsign
    void return_to_dashboard();  // Return to main dashboard screen
    void refreshFramesList();  // Refresh frames list if visible
    void openMessagesScreen();  // Open messages screen
}

#endif // USE_LVGL_UI
#endif // LVGL_UI_H
