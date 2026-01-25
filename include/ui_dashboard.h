/* LVGL UI Dashboard Module
 * Main dashboard screen with status bar, content area, and button bar
 */

#ifndef UI_DASHBOARD_H
#define UI_DASHBOARD_H

#ifdef USE_LVGL_UI

#include <lvgl.h>

namespace UIDashboard {

    // Initialize dashboard module
    void init();

    // Create main dashboard screen
    void createDashboard();

    // Get main screen pointer
    lv_obj_t* getMainScreen();

    // APRS symbol drawing
    void drawAPRSSymbol(const char* symbolStr);

    // Update functions for dashboard labels
    void updateGPS(double lat, double lng, double alt, double speed, int sats, double hdop);
    void updateBattery(int percent, float voltage);
    void updateLoRa(const char* lastRx, int rssi);
    void refreshLoRaInfo();
    void updateWiFi(bool connected, int rssi);
    void updateCallsign(const char* callsign);
    void updateTime(int day, int month, int year, int hour, int minute, int second);
    void updateBluetooth();

    // Navigation
    void returnToDashboard();

    // Button callbacks (public for external use if needed)
    void onBeaconClicked();
    void onMsgClicked();
    void onMapClicked();
    void onSetupClicked();

    // Label getters for external modules (UISettings needs callsign/wifi labels)
    lv_obj_t* getLabelCallsign();
    lv_obj_t* getLabelWifi();

}

#endif // USE_LVGL_UI
#endif // UI_DASHBOARD_H
