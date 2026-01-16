/* LVGL UI for T-Deck Plus
 * Touchscreen-based user interface using LVGL library
 */

#ifndef LVGL_UI_H
#define LVGL_UI_H

#ifdef USE_LVGL_UI

#include <lvgl.h>

namespace LVGL_UI {
    void setup();
    void loop();
    void updateGPS(double lat, double lng, double alt, double speed, int sats);
    void updateBattery(int percent, float voltage);
    void updateLoRa(const char* lastRx, int rssi);
    void updateWiFi(bool connected, int rssi);
    void updateCallsign(const char* callsign);
    void updateTime(int hour, int minute, int second);
    void showMessage(const char* from, const char* message);
}

#endif // USE_LVGL_UI
#endif // LVGL_UI_H
