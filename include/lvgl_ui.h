/* LVGL UI for T-Deck Plus
 * Touchscreen-based user interface using LVGL library
 */

#ifndef LVGL_UI_H
#define LVGL_UI_H

#ifdef USE_LVGL_UI

#include <lvgl.h>

namespace LVGL_UI {
    void showSplashScreen(uint8_t loraIndex, const char* version);
    void showInitScreen();                      // Show init screen with spinner
    void updateInitStatus(const char* status);  // Update init status text
    void hideInitScreen();                      // Hide init screen before dashboard
    void setup();
    void loop();
    void updateGPS(double lat, double lng, double alt, double speed, int sats);
    void updateBattery(int percent, float voltage);
    void updateLoRa(const char* lastRx, int rssi);
    void refreshLoRaInfo();  // Refresh freq/speed display after settings change
    void updateWiFi(bool connected, int rssi);
    void updateCallsign(const char* callsign);
    void updateTime(int day, int month, int year, int hour, int minute, int second);
    void showMessage(const char* from, const char* message);
    void showTxPacket(const char* packet);  // Display TX packet on screen (green)
    void showRxPacket(const char* packet);  // Display RX packet on screen (blue)
    void showWiFiEcoMode();  // Display WiFi eco mode popup
    void handleComposeKeyboard(char key);  // Handle physical keyboard for compose screen
    void showCapsLockPopup(bool active);  // Display Caps Lock status popup
}

#endif // USE_LVGL_UI
#endif // LVGL_UI_H
