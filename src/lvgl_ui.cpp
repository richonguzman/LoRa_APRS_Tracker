/* LVGL UI for T-Deck Plus
 * Touchscreen-based user interface using LVGL library
 */

#ifdef USE_LVGL_UI

#include <APRSPacketLib.h>
#include <Arduino.h>
#include <FS.h>
#include <TFT_eSPI.h>
#include <TinyGPS++.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <lvgl.h>
#define TOUCH_MODULES_GT911
#include "battery_utils.h"
#include "ble_utils.h"
#include "board_pinout.h"
#include "configuration.h"
#include "custom_characters.h"
#include "lora_aprs_bg.h"
#include "lora_aprs_logo.h"
#include "lora_utils.h"
#include "lvgl_ui.h"
#include "msg_utils.h"
#include "notification_utils.h"
#include "sd_logger.h"
#include "station_utils.h"
#include "storage_utils.h"
#include "utils.h"
#include "wifi_utils.h"
#include <SD.h> // Added because used for sent messages
#include <TouchLib.h>
#include <Wire.h>
#include <algorithm> // Pour std::sort
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "ui_map_manager.h" // Inclure le nouveau gestionnaire de carte
#include "ui_common.h"       // UI shared constants and accessors
#include "ui_popups.h"       // Popup notifications module
#include "ui_settings.h"     // Settings screens module
#include "ui_dashboard.h"    // Dashboard screen module
#include "ui_messaging.h"    // Messaging screens module

SemaphoreHandle_t spiMutex = NULL;

// APRS symbol mapping (defined once here for extern declarations in
// custom_characters.h)
const char *symbolArray[] = {"[", ">", "j", "b", "<", "s", "u", "R",
                             "v", "(", ";", "-", "k", "C", "a", "Y",
                             "O", "'", "=", "y", "U", "p", "_", ")"};
const int symbolArraySize = sizeof(symbolArray) / sizeof(symbolArray[0]);
const uint8_t *symbolsAPRS[] = {runnerSymbol,     carSymbol,
                                jeepSymbol,       bikeSymbol,
                                motorcycleSymbol, shipSymbol,
                                truck18Symbol,    recreationalVehicleSymbol,
                                vanSymbol,        carsateliteSymbol,
                                tentSymbol,       houseSymbol,
                                truckSymbol,      canoeSymbol,
                                ambulanceSymbol,  yatchSymbol,
                                baloonSymbol,     aircraftSymbol,
                                trainSymbol,      yagiSymbol,
                                busSymbol,        dogSymbol,
                                wxSymbol,         wheelchairSymbol};

// Expose variables defined in this file to UIMapManager namespace
namespace UIMapManager {
SemaphoreHandle_t &spiMutex = ::spiMutex;
const char *const *symbolArray = ::symbolArray;
const int &symbolArraySize = ::symbolArraySize;
const uint8_t *const *symbolsAPRS = ::symbolsAPRS;
}

// External data sources
extern Configuration Config;
extern uint8_t myBeaconsIndex;
extern int myBeaconsSize;
extern TinyGPSPlus gps;
extern bool WiFiConnected;
extern bool WiFiEcoMode;
extern bool WiFiUserDisabled;
extern bool bluetoothActive;
extern bool bluetoothConnected;
extern String batteryVoltage;
extern APRSPacket lastReceivedPacket;
extern bool sendUpdate; // Set to true to trigger beacon transmission
extern uint8_t loraIndex;
extern int loraIndexSize;
extern bool displayEcoMode;
extern uint8_t screenBrightness;
extern int mapStationsCount; // Compteur de stations pour la barre d'information
                             // de la carte

// Display dimensions
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// LVGL buffer size (use partial buffer to save memory, full buffer in PSRAM)
#define LVGL_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT)

// External TFT instance from display.cpp
extern TFT_eSPI tft;

// External touch module address (found by I2C scan in utils.cpp)
extern uint8_t touchModuleAddress;

// Touch controller - static instance, initialized in setup()
static TouchLib touch(Wire, BOARD_I2C_SDA, BOARD_I2C_SCL, 0x00);
static bool touchInitialized = false;

// Touch calibration (same as touch_utils.cpp)
static const int16_t xCalibratedMin = 5;
static const int16_t xCalibratedMax = 314;
static const int16_t yCalibratedMin = 6;
static const int16_t yCalibratedMax = 233;

// LVGL display buffer (in PSRAM)
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1 = nullptr;
static lv_color_t *buf2 = nullptr;

// LVGL display and input drivers
static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;

// Display eco mode (screen dimming after inactivity)
// Timeout is configured via Config.display.timeout (in seconds)
uint32_t lastActivityTime = 0;        // Non-static: accessed by UISettings
bool screenDimmed = false;            // Non-static: accessed by UISettings

// Note: Dashboard screen and labels are now in UIDashboard module (ui_dashboard.cpp)

// Note: Messages screen is now in UIMessaging module (ui_messaging.cpp)

// Brightness range constants (PWM values)
static const uint8_t BRIGHT_MIN = 50;
static const uint8_t BRIGHT_MAX = 255;


// LVGL tick tracking
uint32_t last_tick = 0;  // Non-static: accessed by UISettings for blocking loops

// Track if LVGL display is already initialized
static bool lvgl_display_initialized = false;

// Display flush callback
static void disp_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area,
                          lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  // Check if Mutex exists AND take it
  if (spiMutex != NULL &&
      xSemaphoreTakeRecursive(spiMutex, portMAX_DELAY) == pdTRUE) {
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();
    xSemaphoreGiveRecursive(spiMutex);
  } else if (spiMutex == NULL) {
    // If mutex doesn't exist yet (very early at boot),
    // we write anyway to not block the display
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();
  }

  lv_disp_flush_ready(drv);
}

// Touch read callback
static uint32_t lastTouchDebug = 0;
static void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  if (touchInitialized && touch.read()) {
    TP_Point t = touch.getPoint(0);
    // X and Y are swapped and Y is inverted because TFT screen is rotated
    uint16_t x = map(t.y, xCalibratedMin, xCalibratedMax, 0, SCREEN_WIDTH);
    uint16_t y = SCREEN_HEIGHT -
                 map(t.x, yCalibratedMin, yCalibratedMax, 0, SCREEN_HEIGHT);
    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;

    // Reset activity timer on touch
    lastActivityTime = millis();

    // Wake up screen if dimmed
    if (screenDimmed) {
      screenDimmed = false;
#ifdef BOARD_BL_PIN
      analogWrite(BOARD_BL_PIN, screenBrightness);
#endif
      // Boost CPU to 240 MHz if on map screen
      if (lv_scr_act() == UIMapManager::screen_map) {
        setCpuFrequencyMhz(240);
        Serial.printf("[LVGL] Screen woken up, CPU boosted to %d MHz (map)\n",
                      getCpuFrequencyMhz());
      } else {
        Serial.println("[LVGL] Screen woken up by touch");
      }
      SD_Logger::logScreenState(false); // Log screen active
    }

    // Debug: print touch coordinates
    if (millis() - lastTouchDebug > 500) {
      Serial.printf("[LVGL Touch] x=%d y=%d (raw: %d,%d)\n", x, y, t.x, t.y);
      lastTouchDebug = millis();
    }
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

// Note: Setup, Freq, Speed, Callsign, Display, Sound, WiFi, Bluetooth screens
// are now in UISettings module (ui_settings.cpp)

// Note: drawAPRSSymbol and button callbacks are now in UIDashboard module (ui_dashboard.cpp)

// Create the main dashboard screen
// Note: create_dashboard is now in UIDashboard module (ui_dashboard.cpp)

// =============================================================================
// UIScreens namespace - Getters for other modules (ui_popups, etc.)
// Delegates to UIMessaging module
// =============================================================================

namespace UIScreens {
    lv_obj_t* getMainScreen() { return UIDashboard::getMainScreen(); }
    lv_obj_t* getMsgScreen() { return UIMessaging::getMsgScreen(); }
    lv_obj_t* getMsgTabview() { return UIMessaging::getMsgTabview(); }
    lv_obj_t* getContactsList() { return UIMessaging::getContactsList(); }
    bool isInitialized() { return UIDashboard::getMainScreen() != nullptr; }
    void populateContactsList() { UIMessaging::refreshContactsList(); }
}

// Open messages screen (called from UIDashboard) - delegates to UIMessaging
void LVGL_UI::openMessagesScreen() {
    UIMessaging::openMessagesScreen();
}

// Open compose screen with prefilled callsign - delegates to UIMessaging
void LVGL_UI::open_compose_with_callsign(const String &callsign) {
    UIMessaging::openComposeWithCallsign(callsign);
}

  namespace LVGL_UI {

  // Splash and init screens shown during boot
  static lv_obj_t *screen_splash = nullptr;
  static lv_obj_t *screen_init = nullptr;
  static lv_obj_t *init_status_label = nullptr;

  void initLvglDisplay() {
    if (spiMutex == NULL) {
      spiMutex = xSemaphoreCreateRecursiveMutex();
    }
// Turn off backlight during init to avoid garbage display
#ifdef BOARD_BL_PIN
    pinMode(BOARD_BL_PIN, OUTPUT);
    digitalWrite(BOARD_BL_PIN, LOW);
#endif

    // Load saved brightness from storage
    STATION_Utils::loadIndex(2); // Screen Brightness value
    // Ensure brightness is within valid range
    if (screenBrightness < BRIGHT_MIN)
      screenBrightness = BRIGHT_MIN;
    if (screenBrightness > BRIGHT_MAX)
      screenBrightness = BRIGHT_MAX;

    // Init TFT
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK); // Clear to black before showing anything

// Now turn on backlight with saved brightness
#ifdef BOARD_BL_PIN
    analogWrite(BOARD_BL_PIN, screenBrightness);
#endif

    // Initialize LVGL if not already done
    if (!lvgl_display_initialized) {
      lv_init();

// Allocate display buffers in PSRAM
#ifdef BOARD_HAS_PSRAM
      buf1 = (lv_color_t *)ps_malloc(LVGL_BUF_SIZE * sizeof(lv_color_t));
      buf2 = (lv_color_t *)ps_malloc(LVGL_BUF_SIZE * sizeof(lv_color_t));
#else
      buf1 = (lv_color_t *)malloc(LVGL_BUF_SIZE * sizeof(lv_color_t));
      buf2 = nullptr;
#endif

      if (buf1) {
        lv_disp_draw_buf_init(&draw_buf, buf1, buf2, LVGL_BUF_SIZE);
        lv_disp_drv_init(&disp_drv);
        disp_drv.hor_res = SCREEN_WIDTH;
        disp_drv.ver_res = SCREEN_HEIGHT;
        disp_drv.flush_cb = disp_flush_cb;
        disp_drv.draw_buf = &draw_buf;
        disp_drv.full_refresh = (buf2 != nullptr) ? 1 : 0;
        lv_disp_drv_register(&disp_drv);
      }
      lvgl_display_initialized = true;
    }
  }

  void showSplashScreen(uint8_t loraIndex, const char *version) {
    Serial.println("[LVGL] Showing splash screen");

    initLvglDisplay();

    // Create splash screen
    screen_splash = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_splash, lv_color_hex(0xffffff),
                              0); // White background for logo

    // LoRa APRS Logo image
    lv_obj_t *logo = lv_img_create(screen_splash);
    lv_img_set_src(logo, &lora_aprs_logo);
    lv_obj_align(logo, LV_ALIGN_TOP_MID, 0, 30);

    // Subtitle: (TRACKER)
    lv_obj_t *subtitle = lv_label_create(screen_splash);
    lv_label_set_text(subtitle, "(TRACKER)");
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x0066cc),
                                0); // Blue to match logo
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_18, 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 115);

    // LoRa Frequency
    const char *region;
    switch (loraIndex) {
    case 0:
      region = "EU";
      break;
    case 1:
      region = "PL";
      break;
    case 2:
      region = "UK";
      break;
    case 3:
      region = "US";
      break;
    default:
      region = "??";
      break;
    }
    char freqBuf[32];
    snprintf(freqBuf, sizeof(freqBuf), "LoRa Freq [%s]", region);
    lv_obj_t *freq_label = lv_label_create(screen_splash);
    lv_label_set_text(freq_label, freqBuf);
    lv_obj_set_style_text_color(freq_label, lv_color_hex(0x0033cc),
                                0); // Blue to match LoRa logo
    lv_obj_align(freq_label, LV_ALIGN_CENTER, 0, 40);

    // Original author and version
    char verBuf[48];
    snprintf(verBuf, sizeof(verBuf), "CA2RXU  %s", version);
    lv_obj_t *ver_label = lv_label_create(screen_splash);
    lv_label_set_text(ver_label, verBuf);
    lv_obj_set_style_text_color(ver_label, lv_color_hex(0xcc0000),
                                0); // Red to match APRS logo
    lv_obj_align(ver_label, LV_ALIGN_BOTTOM_MID, 0, -50);

    // UI fork credit
    lv_obj_t *ui_label = lv_label_create(screen_splash);
    lv_label_set_text(ui_label, "F4MLV LVGL UI Edition");
    lv_obj_set_style_text_color(ui_label, lv_color_hex(0x0066cc), 0); // Blue
    lv_obj_set_style_text_font(ui_label, &lv_font_montserrat_12, 0);
    lv_obj_align(ui_label, LV_ALIGN_BOTTOM_MID, 0, -32);

    // Load and display
    lv_scr_load(screen_splash);
    lv_refr_now(NULL);

    // Brief delay then switch to init screen
    delay(2400);

    Serial.println("[LVGL] Splash done, showing init screen");
  }

  void showInitScreen() {
    // Keep splash screen visible, just add init status label on it
    if (!screen_splash) {
      Serial.println("[LVGL] ERROR: splash screen not available for init");
      return;
    }

    // Add status label to splash screen (below the version info)
    init_status_label = lv_label_create(screen_splash);
    lv_label_set_text(init_status_label, "Initialisation...");
    lv_obj_set_style_text_color(init_status_label, lv_color_hex(0x0066cc),
                                0); // Blue to match logo
    lv_obj_set_style_text_font(init_status_label, &lv_font_montserrat_14, 0);
    lv_obj_align(init_status_label, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_refr_now(NULL);
  }

  void updateInitStatus(const char *status) {
    if (init_status_label && lv_obj_is_valid(init_status_label)) {
      lv_label_set_text(init_status_label, status);
      lv_refr_now(NULL);
    }
    Serial.printf("[LVGL] Init: %s\n", status);
  }

  void hideInitScreen() {
    // Delete splash screen (which now contains the init label)
    if (screen_splash) {
      lv_obj_del(screen_splash);
      screen_splash = nullptr;
      init_status_label = nullptr;
    }
  }

  void setup() {
    Serial.println("[LVGL] Initializing...");

    // Initialize tick counter
    last_tick = millis();

    // Load saved settings from storage
    STATION_Utils::loadIndex(2); // Screen Brightness value
    STATION_Utils::loadIndex(3); // Display Eco Mode
    // Ensure brightness is within valid range for slider
    if (screenBrightness < BRIGHT_MIN)
      screenBrightness = BRIGHT_MIN;
    if (screenBrightness > BRIGHT_MAX)
      screenBrightness = BRIGHT_MAX;
    Serial.printf("[LVGL] Loaded brightness: %d\n", screenBrightness);

    // Only initialize display if not already done by splash screen
    if (!lvgl_display_initialized) {
// Set backlight with saved brightness
#ifdef BOARD_BL_PIN
      pinMode(BOARD_BL_PIN, OUTPUT);
      analogWrite(BOARD_BL_PIN, screenBrightness);
#endif

      // Re-init TFT for LVGL
      tft.init();
      tft.setRotation(1); // Landscape, keyboard at bottom

      // Initialize LVGL
      lv_init();

// Allocate display buffers in PSRAM
#ifdef BOARD_HAS_PSRAM
      buf1 = (lv_color_t *)ps_malloc(LVGL_BUF_SIZE * sizeof(lv_color_t));
      buf2 = (lv_color_t *)ps_malloc(LVGL_BUF_SIZE * sizeof(lv_color_t));
      Serial.println("[LVGL] Using PSRAM for display buffers");
#else
      buf1 = (lv_color_t *)malloc(LVGL_BUF_SIZE * sizeof(lv_color_t));
      buf2 = nullptr;
      Serial.println("[LVGL] Using RAM for display buffer");
#endif

      if (!buf1) {
        Serial.println("[LVGL] ERROR: Failed to allocate display buffer!");
        return;
      }

      // Initialize display buffer
      lv_disp_draw_buf_init(&draw_buf, buf1, buf2, LVGL_BUF_SIZE);

      // Initialize display driver
      lv_disp_drv_init(&disp_drv);
      disp_drv.hor_res = SCREEN_WIDTH;
      disp_drv.ver_res = SCREEN_HEIGHT;
      disp_drv.flush_cb = disp_flush_cb;
      disp_drv.draw_buf = &draw_buf;
      disp_drv.full_refresh =
          (buf2 != nullptr) ? 1 : 0; // Full refresh if double buffered
      lv_disp_drv_register(&disp_drv);
      lvgl_display_initialized = true;
    } else {
      Serial.println("[LVGL] Display already initialized by splash screen");
    }

    // Initialize touch input
    if (touchModuleAddress != 0x00) {
      Serial.printf("[LVGL] Touch module found at 0x%02X\n",
                    touchModuleAddress);
      if (touchModuleAddress == 0x14) {
        touch =
            TouchLib(Wire, BOARD_I2C_SDA, BOARD_I2C_SCL, GT911_SLAVE_ADDRESS2);
      } else if (touchModuleAddress == 0x5D) {
        touch =
            TouchLib(Wire, BOARD_I2C_SDA, BOARD_I2C_SCL, GT911_SLAVE_ADDRESS1);
      }
      touch.init();
      touchInitialized = true;

      // Register LVGL input device
      lv_indev_drv_init(&indev_drv);
      indev_drv.type = LV_INDEV_TYPE_POINTER;
      indev_drv.read_cb = touch_read_cb;
      lv_indev_drv_register(&indev_drv);
      Serial.println("[LVGL] Touch input registered");
    } else {
      Serial.println("[LVGL] No touch module detected");
    }

    // Create the UI (dashboard module)
    UIDashboard::createDashboard();

    // Clean up any remaining init screens
    if (screen_splash) {
      lv_obj_del(screen_splash);
      screen_splash = nullptr;
    }
    if (screen_init) {
      lv_obj_del(screen_init);
      screen_init = nullptr;
      init_status_label = nullptr;
    }

    // Force initial refresh
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);
    Serial.println("[LVGL] Forced initial refresh");

    // Initialize activity timer for eco mode
    lastActivityTime = millis();

    Serial.println("[LVGL] UI Ready");
  }

  static uint32_t last_data_update = 0;
  static String last_callsign = "";

  void loop() {
    // Update LVGL tick
    uint32_t now = millis();
    uint32_t elapsed = now - last_tick;
    lv_tick_inc(elapsed);
    last_tick = now;

#ifdef DEBUG
    // Debug heartbeat every 5 seconds
    static uint32_t lastHeartbeat = 0;
    if (now - lastHeartbeat >= 5000) {
      lastHeartbeat = now;
      Serial.printf("[LVGL-HB] heap=%u eco=%d dimmed=%d bright=%d\n",
                    ESP.getFreeHeap(), displayEcoMode ? 1 : 0,
                    screenDimmed ? 1 : 0, screenBrightness);
    }
#endif

    // Handle LVGL tasks
    lv_timer_handler();

    // Display eco mode: dim screen after inactivity timeout
    // Use fresh millis() value since lastActivityTime may have been updated
    // during lv_timer_handler()
    if (displayEcoMode && !screenDimmed) {
      uint32_t currentTime = millis();
      uint32_t ecoTimeoutMs =
          Config.display.timeout * 1000; // Config is in seconds
      if (currentTime - lastActivityTime >= ecoTimeoutMs) {
        screenDimmed = true;
#ifdef BOARD_BL_PIN
        analogWrite(BOARD_BL_PIN, 0); // Turn off backlight
#endif
        // Reduce CPU to 80 MHz if on map screen
        if (lv_scr_act() == UIMapManager::screen_map) {
          setCpuFrequencyMhz(80);
          Serial.printf(
              "[LVGL] Screen dimmed (eco mode), CPU reduced to %d MHz (map)\n",
              getCpuFrequencyMhz());
        } else {
          Serial.println("[LVGL] Screen dimmed (eco mode)");
        }
        SD_Logger::logScreenState(true); // Log screen dimmed
      }
    }

    // Update data every second
    if (now - last_data_update >= 1000) {
      last_data_update = now;

      // Update callsign and symbol if changed
      Beacon *currentBeacon = &Config.beacons[myBeaconsIndex];
      if (currentBeacon->callsign != last_callsign) {
        last_callsign = currentBeacon->callsign;
        UIDashboard::updateCallsign(last_callsign.c_str());
        // Update APRS symbol when beacon changes (overlay + symbol)
        String fullSymbol = currentBeacon->overlay + currentBeacon->symbol;
        UIDashboard::drawAPRSSymbol(fullSymbol.c_str());
      }

      // Update GPS data
      if (gps.location.isValid()) {
        UIDashboard::updateGPS(gps.location.lat(), gps.location.lng(), gps.altitude.meters(),
                  gps.speed.kmph(), gps.satellites.value(), gps.hdop.hdop());
      }

      // Update date/time from GPS
      if (gps.time.isValid() && gps.date.isValid()) {
        UIDashboard::updateTime(gps.date.day(), gps.date.month(), gps.date.year(),
                   gps.time.hour(), gps.time.minute(), gps.time.second());
      }

      // Update battery
      if (batteryVoltage.length() > 0) {
        float voltage = batteryVoltage.toFloat();
        int percent = (int)(((voltage - 3.0) / (4.2 - 3.0)) * 100);
        if (percent > 100)
          percent = 100;
        if (percent < 0)
          percent = 0;
        UIDashboard::updateBattery(percent, voltage);
      }

      // Update WiFi status
      UIDashboard::updateWiFi(WiFiConnected, WiFiConnected ? WiFi.RSSI() : 0);

      // Update Bluetooth status
      UIDashboard::updateBluetooth();

      // Update LoRa info and last RX stations
      UIDashboard::refreshLoRaInfo();
      UIDashboard::updateLastRx();

      // Update Stats tab if currently active
      UIMessaging::refreshStatsIfActive();
    }
  }

  // Update functions - delegated to UIDashboard module
  void updateGPS(double lat, double lng, double alt, double speed, int sats, double hdop) {
    UIDashboard::updateGPS(lat, lng, alt, speed, sats, hdop);
  }

  void updateBattery(int percent, float voltage) {
    UIDashboard::updateBattery(percent, voltage);
  }

  void updateLoRa(const char *lastRx, int rssi) {
    UIDashboard::updateLoRa(lastRx, rssi);
  }

  void refreshLoRaInfo() {
    UIDashboard::refreshLoRaInfo();
  }

  void updateWiFi(bool connected, int rssi) {
    UIDashboard::updateWiFi(connected, rssi);
  }

  // RX Message popup - delegated to UIPopups module
  void showMessage(const char *from, const char *message) {
    UIPopups::showMessage(from, message);
  }

  void updateCallsign(const char *callsign) {
    UIDashboard::updateCallsign(callsign);
  }

  void updateTime(int day, int month, int year, int hour, int minute, int second) {
    UIDashboard::updateTime(day, month, year, hour, minute, second);
  }

  // Beacon pending popup - delegated to UIPopups module
  void showBeaconPending() {
    UIPopups::showBeaconPending();
  }

  void hideBeaconPending() {
    UIPopups::hideBeaconPending();
  }

  // Close all popups - delegated to UIPopups module
  void closeAllPopups() {
    UIPopups::closeAll();
  }

  // TX packet popup - delegated to UIPopups module
  void showTxPacket(const char *packet) {
    UIPopups::showTxPacket(packet);
  }

  // RX LoRa packet popup - delegated to UIPopups module
  void showRxPacket(const char *packet) {
    UIPopups::showRxPacket(packet);
  }

  // WiFi Eco Mode popup - delegated to UIPopups module
  void showWiFiEcoMode() {
    UIPopups::showWiFiEcoMode();
  }

  // Caps Lock popup - delegated to UIPopups module
  void showCapsLockPopup(bool active) {
    UIPopups::showCapsLockPopup(active);
  }

  // Add Contact prompt - delegated to UIPopups module
  void showAddContactPrompt(const char *callsign) {
    UIPopups::showAddContactPrompt(callsign);
  }

  // Handle physical keyboard for compose screen - delegates to UIMessaging
  void handleComposeKeyboard(char key) {
    UIMessaging::handleComposeKeyboard(key);
  }

  void showBootWebConfig() {
    UISettings::showBootWebConfig();
  }

  // Return to main dashboard screen - delegated to UIDashboard module
  void return_to_dashboard() {
    UIDashboard::returnToDashboard();
  }

  // Refresh frames list if visible - delegates to UIMessaging
  void refreshFramesList() {
    UIMessaging::refreshFramesList();
  }

  }

#endif // USE_LVGL_UI
