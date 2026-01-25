/* LVGL UI Settings Module
 * Setup menu, frequency, speed, callsign, display, sound, WiFi, Bluetooth screens
 */

#ifndef UI_SETTINGS_H
#define UI_SETTINGS_H

#ifdef USE_LVGL_UI

#include <lvgl.h>

namespace UISettings {

    // Initialize settings module
    void init();

    // Screen creation functions
    void createSetupScreen();
    void createFreqScreen();
    void createSpeedScreen();
    void createCallsignScreen();
    void createDisplayScreen();
    void createSoundScreen();
    void createWifiScreen();
    void createBluetoothScreen();

    // Navigation functions (called from dashboard buttons)
    void openSetup();
    void openFrequency();
    void openSpeed();
    void openCallsign();
    void openDisplay();
    void openSound();
    void openWifi();
    void openBluetooth();

    // Back navigation
    void backToDashboard();
    void backToSetup();

    // Screen getters (for external navigation checks)
    lv_obj_t* getSetupScreen();
    lv_obj_t* getWifiScreen();
    lv_obj_t* getBluetoothScreen();

    // Cleanup (stop timers when leaving screens)
    void stopWifiTimer();
    void stopBluetoothTimer();

    // Web-Conf mode (blocking)
    void openWebConf();
    void showBootWebConfig();

}

#endif // USE_LVGL_UI
#endif // UI_SETTINGS_H
