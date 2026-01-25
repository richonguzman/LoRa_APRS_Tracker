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

// UI Elements - Main screen
static lv_obj_t *screen_main = nullptr;
lv_obj_t *label_callsign = nullptr;   // Non-static: accessed by UISettings
static lv_obj_t *label_gps = nullptr;
static lv_obj_t *label_battery = nullptr;
static lv_obj_t *label_lora = nullptr;
lv_obj_t *label_wifi = nullptr;       // Non-static: accessed by UISettings
static lv_obj_t *label_bluetooth = nullptr;
static lv_obj_t *label_storage = nullptr;
static lv_obj_t *label_time = nullptr;
static lv_obj_t *aprs_symbol_canvas = nullptr;
static lv_color_t *aprs_symbol_buf = nullptr;

// UI Elements - Messages screen
static lv_obj_t *screen_msg = nullptr;
static lv_obj_t *list_aprs_global = nullptr;
static void populate_msg_list(lv_obj_t *list, int type); // Forward declaration

// Stats guard
static uint32_t last_processed_rx_count = 0;

// Brightness range constants (PWM values)
static const uint8_t BRIGHT_MIN = 50;
static const uint8_t BRIGHT_MAX = 255;

// UI Elements - Stats Tab (for persistent update)
static lv_obj_t *stats_title_lbl = nullptr;
static lv_obj_t *stats_rx_tx_counts_lbl = nullptr;
static lv_obj_t *stats_rssi_stats_lbl = nullptr;
static lv_obj_t *stats_snr_stats_lbl = nullptr;
static lv_obj_t *stats_rssi_chart_legend_lbl = nullptr;
static lv_obj_t *stats_rssi_chart_obj = nullptr;
static lv_chart_series_t *stats_rssi_chart_ser = nullptr;
static lv_obj_t *stats_snr_chart_legend_lbl = nullptr;
static lv_obj_t *stats_snr_chart_obj = nullptr;
static lv_chart_series_t *stats_snr_chart_ser = nullptr;
static lv_obj_t *stats_digi_title_lbl = nullptr;
static lv_obj_t *stats_no_digi_lbl = nullptr;
static lv_obj_t *stats_digi_list_container =
    nullptr; // A container for digipeater labels

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

// Forward declarations
static void create_msg_screen();

// Note: Setup, Freq, Speed, Callsign, Display, Sound, WiFi, Bluetooth screens
// are now in UISettings module (ui_settings.cpp)

// Dessine le symbole APRS sur le canevas du tableau de bord
#define APRS_CANVAS_WIDTH SYMBOL_WIDTH
#define APRS_CANVAS_HEIGHT SYMBOL_HEIGHT

void drawAPRSSymbol(const char *symbolStr) {
  if (!aprs_symbol_canvas || !aprs_symbol_buf)
    return;

  // Extract symbol character from full format (e.g., "/>" or "\>" or ">")
  // Symbol is always second char in 2-char format, first char in 1-char format
  char symbolChar[2] = {0, 0};
  if (symbolStr && strlen(symbolStr) >= 2) {
    symbolChar[0] = symbolStr[1]; // Second character is the symbol
  } else if (symbolStr && strlen(symbolStr) >= 1) {
    symbolChar[0] = symbolStr[0];
  }

  // Trouver l'index du symbole
  int symbolIndex = -1;
  for (int i = 0; i < symbolArraySize; i++) {
    if (strcmp(symbolChar, symbolArray[i]) == 0) {
      symbolIndex = i;
      break;
    }
  }

  // Effacer le canevas avec un fond transparent/sombre
  lv_canvas_fill_bg(aprs_symbol_canvas, lv_color_hex(0x16213e), LV_OPA_COVER);

  if (symbolIndex < 0)
    return; // Symbol not found

  const uint8_t *bitMap = symbolsAPRS[symbolIndex];
  lv_color_t white = lv_color_hex(0xffffff); // Blanc comme l'indicatif

  // Dessiner le bitmap 1:1
  for (int y = 0; y < SYMBOL_HEIGHT; y++) {
    for (int x = 0; x < SYMBOL_WIDTH; x++) {
      int byteIndex = (y * ((SYMBOL_WIDTH + 7) / 8)) + (x / 8);
      int bitIndex = 7 - (x % 8);
      if (bitMap[byteIndex] & (1 << bitIndex)) {
        lv_canvas_set_px_color(aprs_symbol_canvas, x, y, white);
      }
    }
  }
  lv_obj_invalidate(aprs_symbol_canvas);
}

// Button event callbacks
static void btn_beacon_clicked(lv_event_t *e) {
  sendUpdate = true;
  Serial.println("[LVGL] BEACON button pressed - requesting beacon");
  LVGL_UI::showBeaconPending(); // Show orange "waiting for GPS" popup
}

static void btn_setup_clicked(lv_event_t *e) {
  Serial.println("[LVGL] SETUP button pressed");
  LVGL_UI::closeAllPopups();
  UISettings::openSetup();
}

static void btn_back_clicked(lv_event_t *e) {
  Serial.println("[LVGL] BACK button pressed");
  LVGL_UI::closeAllPopups();
  lv_scr_load_anim(screen_main, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
}

static void btn_msg_clicked(lv_event_t *e) {
  Serial.println("[LVGL] MSG button pressed");
  LVGL_UI::closeAllPopups();
  if (!screen_msg) {
    create_msg_screen();
  } else {
    // Refresh conversations list when returning to screen
    if (list_aprs_global) {
      populate_msg_list(list_aprs_global, 0);
    }
  }
  lv_scr_load_anim(screen_msg, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
}

static void btn_map_clicked(lv_event_t *e) {
  Serial.println("[LVGL] MAP button pressed");
  Serial.printf("[LVGL-DEBUG] Free heap before MAP: %u bytes\n",
                ESP.getFreeHeap());
  // Close any open popups before changing screen
  LVGL_UI::closeAllPopups();
  Serial.println("[LVGL-DEBUG] Popups closed");
  // Recreate map screen each time to update positions
  if (UIMapManager::screen_map) {
    Serial.println("[LVGL-DEBUG] Deleting old screen_map");
    lv_obj_del(UIMapManager::screen_map);
    UIMapManager::screen_map = nullptr;
  }
  Serial.println("[LVGL-DEBUG] Creating new map screen");
  UIMapManager::create_map_screen(); // Call the new map creation function
  Serial.println("[LVGL-DEBUG] Map screen created, loading animation");
  lv_scr_load_anim(UIMapManager::screen_map, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0,
                   false);
  Serial.println("[LVGL-DEBUG] btn_map_clicked DONE");
}

// Create the main dashboard screen
static void create_dashboard() {
  // Create main screen
  screen_main = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(screen_main, lv_color_hex(0x1a1a2e), 0);

  // Status bar at top
  lv_obj_t *status_bar = lv_obj_create(screen_main);
  lv_obj_set_size(status_bar, SCREEN_WIDTH, 30);
  lv_obj_set_pos(status_bar, 0, 0);
  lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x16213e), 0);
  lv_obj_set_style_border_width(status_bar, 0, 0);
  lv_obj_set_style_radius(status_bar, 0, 0);
  lv_obj_set_style_pad_all(status_bar, 5, 0);
  lv_obj_set_flex_flow(status_bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(status_bar, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  // Callsign label (left)
  label_callsign = lv_label_create(status_bar);
  lv_label_set_text(label_callsign, "NOCALL");
  lv_obj_set_style_text_color(label_callsign, lv_color_hex(0xffffff),
                              0); // White
  lv_obj_set_style_text_font(label_callsign, &lv_font_montserrat_14, 0);

  // APRS symbol canvas (center) - bitmap symbol scaled 2x
  aprs_symbol_buf = (lv_color_t *)malloc(
      APRS_CANVAS_WIDTH * APRS_CANVAS_HEIGHT * sizeof(lv_color_t));
  if (aprs_symbol_buf) {
    aprs_symbol_canvas = lv_canvas_create(status_bar);
    lv_canvas_set_buffer(aprs_symbol_canvas, aprs_symbol_buf, APRS_CANVAS_WIDTH,
                         APRS_CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_size(aprs_symbol_canvas, APRS_CANVAS_WIDTH, APRS_CANVAS_HEIGHT);
    // Draw initial symbol from current beacon (overlay + symbol)
    Beacon *currentBeacon = &Config.beacons[myBeaconsIndex];
    String fullSymbol = currentBeacon->overlay + currentBeacon->symbol;
    drawAPRSSymbol(fullSymbol.c_str());
  }

  // Date/Time label (right)
  label_time = lv_label_create(status_bar);
  lv_label_set_text(label_time, "--/--/---- --:--:-- UTC");
  lv_obj_set_style_text_color(label_time, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_text_font(label_time, &lv_font_montserrat_14, 0);

  // Main content area
  lv_obj_t *content = lv_obj_create(screen_main);
  lv_obj_set_size(content, SCREEN_WIDTH - 10, SCREEN_HEIGHT - 80);
  lv_obj_set_pos(content, 5, 35);
  lv_obj_set_style_bg_color(content, lv_color_hex(0x0f0f23), 0);
  lv_obj_set_style_border_color(content, lv_color_hex(0x16213e), 0);
  lv_obj_set_style_radius(content, 8, 0);
  lv_obj_set_style_pad_all(content, 10, 0);

  // GPS info
  label_gps = lv_label_create(content);
  lv_label_set_text(label_gps, "GPS: -- sat  Loc: --------\nLat: --.----  Lon: "
                               "--.----\nAlt: ---- m  Spd: --- km/h");
  lv_obj_set_style_text_color(label_gps, lv_color_hex(0x759a9e), 0);
  lv_obj_set_style_text_font(label_gps, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(label_gps, 0, 0);

  // LoRa info
  label_lora = lv_label_create(content);
  char lora_init[96];
  float freq = Config.loraTypes[loraIndex].frequency / 1000000.0;
  int rate = Config.loraTypes[loraIndex].dataRate;
  snprintf(lora_init, sizeof(lora_init), "LoRa: %.3f MHz  %d bps\nLast RX: ---",
           freq, rate);
  lv_label_set_text(label_lora, lora_init);
  lv_obj_set_style_text_color(label_lora, lv_color_hex(0xff6b6b), 0);
  lv_obj_set_style_text_font(label_lora, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(label_lora, 0, 55);

  // WiFi info
  label_wifi = lv_label_create(content);
  lv_label_set_text(label_wifi, "WiFi: ---");
  lv_obj_set_style_text_color(label_wifi, lv_color_hex(0x759a9e), 0);
  lv_obj_set_style_text_font(label_wifi, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(label_wifi, 0, 95);

  // Bluetooth info
  label_bluetooth = lv_label_create(content);
  if (!bluetoothActive) {
    lv_label_set_text(label_bluetooth, "BT: Disabled");
    lv_obj_set_style_text_color(label_bluetooth, lv_color_hex(0x666666),
                                0); // Gray
  } else if (bluetoothConnected) {
    String addr = BLE_Utils::getConnectedDeviceAddress();
    if (addr.length() > 0) {
      String btText = "BT: > " + addr;
      lv_label_set_text(label_bluetooth, btText.c_str());
    } else {
      lv_label_set_text(label_bluetooth, "BT: Connected");
    }
    lv_obj_set_style_text_color(label_bluetooth, lv_color_hex(0xc792ea),
                                0); // Purple
  } else {
    lv_label_set_text(label_bluetooth, "BT: Waiting...");
    lv_obj_set_style_text_color(label_bluetooth, lv_color_hex(0xffa500),
                                0); // Orange
  }
  lv_obj_set_style_text_font(label_bluetooth, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(label_bluetooth, 0, 115);

  // Battery info
  label_battery = lv_label_create(content);
  lv_label_set_text(label_battery, "Bat: --.-- V (--%)");
  lv_obj_set_style_text_color(label_battery, lv_color_hex(0xff6b6b),
                              0); // Red/coral color
  lv_obj_set_style_text_font(label_battery, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(label_battery, 0, 135);

  // Storage info
  label_storage = lv_label_create(content);
  String storageInfo = "Storage: " + STORAGE_Utils::getStorageType();
  if (STORAGE_Utils::isSDAvailable()) {
    storageInfo +=
        " (" + String(STORAGE_Utils::getTotalBytes() / (1024 * 1024)) + "MB)";
  }
  lv_label_set_text(label_storage, storageInfo.c_str());
  lv_obj_set_style_text_color(label_storage, lv_color_hex(0xffcc00),
                              0); // Yellow/gold
  lv_obj_set_style_text_font(label_storage, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(label_storage, 0, 155);

  // Bottom button bar
  lv_obj_t *btn_bar = lv_obj_create(screen_main);
  lv_obj_set_size(btn_bar, SCREEN_WIDTH, 40);
  lv_obj_set_pos(btn_bar, 0, SCREEN_HEIGHT - 40);
  lv_obj_set_style_bg_color(btn_bar, lv_color_hex(0x16213e), 0);
  lv_obj_set_style_border_width(btn_bar, 0, 0);
  lv_obj_set_style_radius(btn_bar, 0, 0);
  lv_obj_set_style_pad_all(btn_bar, 5, 0);
  lv_obj_set_flex_flow(btn_bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btn_bar, LV_FLEX_ALIGN_SPACE_EVENLY,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  // Beacon button (APRS red)
  lv_obj_t *btn_beacon = lv_btn_create(btn_bar);
  lv_obj_set_size(btn_beacon, 70, 30);
  lv_obj_set_style_bg_color(btn_beacon, lv_color_hex(0xcc0000), 0); // APRS red
  lv_obj_add_event_cb(btn_beacon, btn_beacon_clicked, LV_EVENT_CLICKED, NULL);
  lv_obj_t *lbl_beacon = lv_label_create(btn_beacon);
  lv_label_set_text(lbl_beacon, "BCN");
  lv_obj_center(lbl_beacon);
  lv_obj_set_style_text_color(lbl_beacon, lv_color_hex(0xffffff),
                              0); // White text

  // Messages button (APRS blue)
  lv_obj_t *btn_msg = lv_btn_create(btn_bar);
  lv_obj_set_size(btn_msg, 70, 30);
  lv_obj_set_style_bg_color(btn_msg, lv_color_hex(0x0066cc),
                            0); // APRS blue (globe)
  lv_obj_add_event_cb(btn_msg, btn_msg_clicked, LV_EVENT_CLICKED, NULL);
  lv_obj_t *lbl_msg = lv_label_create(btn_msg);
  lv_label_set_text(lbl_msg, "MSG");
  lv_obj_center(lbl_msg);
  lv_obj_set_style_text_color(lbl_msg, lv_color_hex(0xffffff), 0); // White text

  // Map button (green)
  lv_obj_t *btn_map = lv_btn_create(btn_bar);
  lv_obj_set_size(btn_map, 70, 30);
  lv_obj_set_style_bg_color(btn_map, lv_color_hex(0x009933),
                            0); // Green for map
  lv_obj_add_event_cb(btn_map, btn_map_clicked, LV_EVENT_CLICKED, NULL);
  lv_obj_t *lbl_map = lv_label_create(btn_map);
  lv_label_set_text(lbl_map, "MAP");
  lv_obj_center(lbl_map);
  lv_obj_set_style_text_color(lbl_map, lv_color_hex(0xffffff), 0); // White text

  // Settings button
  lv_obj_t *btn_settings = lv_btn_create(btn_bar);
  lv_obj_set_size(btn_settings, 70, 30);
  lv_obj_set_style_bg_color(btn_settings, lv_color_hex(0xc792ea), 0);
  lv_obj_add_event_cb(btn_settings, btn_setup_clicked, LV_EVENT_CLICKED, NULL);
  lv_obj_t *lbl_settings = lv_label_create(btn_settings);
  lv_label_set_text(lbl_settings, "SET");
  lv_obj_center(lbl_settings);
  lv_obj_set_style_text_color(lbl_settings, lv_color_hex(0x000000), 0);

  // Load the screen
  lv_scr_load(screen_main);
}

// Setup menu callbacks and screens are now in UISettings module (ui_settings.cpp)
// Settings screens (Setup, Freq, Speed, Callsign, Display, Sound, WiFi, Bluetooth)
// are now in ui_settings.cpp / UISettings namespace



// ============================================================================

// ============================================================================

// Messages screen variables
static lv_obj_t *msg_tabview = nullptr;
// list_aprs_global declared at top of file
static lv_obj_t *list_wlnk_global = nullptr;
static lv_obj_t *list_contacts_global = nullptr;
static lv_obj_t *list_frames_global = nullptr;
static lv_obj_t *cont_stats_global = nullptr;
static int current_msg_type =
    0; // 0 = APRS, 1 = Winlink, 2 = Contacts, 3 = Frames, 4 = Stats

// Compose screen variables (declared early for use in callbacks)
static lv_obj_t *screen_compose = nullptr;
static lv_obj_t *compose_to_input = nullptr;
static lv_obj_t *compose_msg_input = nullptr;
static lv_obj_t *compose_keyboard = nullptr;
static lv_obj_t *current_focused_input = nullptr;
static bool compose_screen_active = false;
static lv_obj_t *compose_return_screen =
    nullptr; // Screen to return to after compose

// Forward declaration
static void create_compose_screen();
static void populate_contacts_list(lv_obj_t *list);

// =============================================================================
// UIScreens namespace - Getters for other modules (ui_popups, etc.)
// =============================================================================

namespace UIScreens {
    lv_obj_t* getMainScreen() { return screen_main; }
    lv_obj_t* getMsgScreen() { return screen_msg; }
    lv_obj_t* getMsgTabview() { return msg_tabview; }
    lv_obj_t* getContactsList() { return list_contacts_global; }
    bool isInitialized() { return screen_main != nullptr; }
    void populateContactsList() {
        if (list_contacts_global) {
            populate_contacts_list(list_contacts_global);
        }
    }
}

// Open compose screen with prefilled callsign (now public function)
void LVGL_UI::open_compose_with_callsign(const String &callsign) {
  // Create compose screen if necessary
  create_compose_screen();

  // Store current screen to return to after sending/canceling
  compose_return_screen = lv_scr_act();

  // Pré-remplir la destination avec l'indicatif
  if (compose_to_input) {
    lv_textarea_set_text(compose_to_input, callsign.c_str());
  }

  // Naviguer vers l'écran de composition
  lv_scr_load_anim(screen_compose, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
  Serial.printf("[LVGL] Ouverture de la composition pour : %s\n",
                callsign.c_str());
}

// Show message detail popup
static lv_obj_t *detail_msgbox = nullptr;

static void detail_msgbox_deleted_cb(lv_event_t *e) { detail_msgbox = nullptr; }

static void show_message_detail(const char *msg) {
  // Close previous msgbox if it still exists
  if (detail_msgbox && lv_obj_is_valid(detail_msgbox)) {
    lv_msgbox_close(detail_msgbox);
  }
  detail_msgbox = nullptr;

  detail_msgbox = lv_msgbox_create(NULL, "Message", msg, NULL, true);
  lv_obj_set_style_bg_color(detail_msgbox, lv_color_hex(0x1a1a2e), 0);
  lv_obj_set_style_text_color(detail_msgbox, lv_color_hex(0xffffff),
                              LV_PART_MAIN);
  lv_obj_set_width(detail_msgbox, SCREEN_WIDTH - 40);
  lv_obj_center(detail_msgbox);

  // Track when msgbox is deleted (any way it gets destroyed)
  lv_obj_add_event_cb(detail_msgbox, detail_msgbox_deleted_cb, LV_EVENT_DELETE,
                      NULL);
}

// Forward declaration for message list
static void populate_msg_list(lv_obj_t *list, int type);

// Confirmation popup for delete operations
static lv_obj_t *confirm_msgbox = nullptr;
static lv_obj_t *confirm_msgbox_to_delete = nullptr;
static int pending_delete_msg_index =
    -1; // Index of message to delete (-1 = all)
static bool msg_longpress_handled = false;
static bool need_aprs_list_refresh = false;

// Timer callback for deferred confirm_msgbox deletion
static void delete_confirm_msgbox_timer_cb(lv_timer_t *timer) {
  Serial.println("[LVGL] delete_confirm_msgbox_timer_cb called");
  if (confirm_msgbox_to_delete && lv_obj_is_valid(confirm_msgbox_to_delete)) {
    lv_obj_del(confirm_msgbox_to_delete);
    Serial.println("[LVGL] Msgbox deleted");
  }
  confirm_msgbox_to_delete = nullptr;

  lv_obj_invalidate(lv_layer_top());
  lv_refr_now(NULL);

  Serial.printf("[LVGL] need_aprs_list_refresh=%d, list_aprs_global=%p\n",
                need_aprs_list_refresh, list_aprs_global);
  if (need_aprs_list_refresh) {
    // Refresh both lists to be safe
    if (list_aprs_global) {
      Serial.println("[LVGL] Refreshing APRS list after delete");
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
  Serial.printf("[LVGL] confirm_delete_cb: btn=%s, pending_index=%d\n",
                btn_text ? btn_text : "NULL", pending_delete_msg_index);

  need_aprs_list_refresh = false;
  if (btn_text && strcmp(btn_text, "Yes") == 0) {
    if (pending_delete_msg_index == -1) {
      Serial.printf("[LVGL] Deleting ALL messages type %d\n", current_msg_type);
      MSG_Utils::deleteFile(current_msg_type);
    } else {
      MSG_Utils::deleteMessageByIndex(current_msg_type,
                                      pending_delete_msg_index);
    }
    need_aprs_list_refresh = true;
    Serial.println("[LVGL] need_aprs_list_refresh set to true");
  }

  // Schedule deletion via timer
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
  confirm_msgbox =
      lv_msgbox_create(lv_layer_top(), "Confirmation", message, btns, false);
  lv_obj_set_style_bg_color(confirm_msgbox, lv_color_hex(0x1a1a2e), 0);
  lv_obj_set_style_bg_opa(confirm_msgbox, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(confirm_msgbox, lv_color_hex(0xffffff),
                              LV_PART_MAIN);
  lv_obj_set_width(confirm_msgbox, 220);
  lv_obj_center(confirm_msgbox);
  lv_obj_add_event_cb(confirm_msgbox, confirm_delete_cb, LV_EVENT_VALUE_CHANGED,
                      NULL);
}

// Message item click callback
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

// Conversation screen variables
static lv_obj_t *screen_conversation = nullptr;
static lv_obj_t *conversation_list = nullptr;
static String current_conversation_callsign = "";
static int pending_conversation_msg_delete =
    -1; // Index of message to delete in conversation
static bool conversation_msg_longpress_handled = false;
static lv_obj_t *conversation_confirm_msgbox = nullptr;
static lv_obj_t *msgbox_to_delete = nullptr; // For deferred deletion
static bool need_conversation_refresh = false;

// Forward declaration
static void refresh_conversation_messages();

// Timer callback for deferred msgbox deletion
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

// Forward declarations for conversation screen
static void create_conversation_screen(const String &callsign);
static void conversation_msg_clicked(lv_event_t *e);
static void conversation_msg_longpress(lv_event_t *e);

// Refresh only the message list content (not the whole screen)
static void refresh_conversation_messages() {
  if (!conversation_list)
    return;

  // Clear all children of the list
  lv_obj_clean(conversation_list);

  // Reload messages
  std::vector<String> messages =
      MSG_Utils::getMessagesForContact(current_conversation_callsign);

  if (messages.size() == 0) {
    lv_obj_t *empty = lv_label_create(conversation_list);
    lv_label_set_text(empty, "No messages");
    lv_obj_set_style_text_color(empty, lv_color_hex(0x888888), 0);
  } else {
    for (int i = messages.size() - 1; i >= 0; i--) {
      String msg = messages[i];
      int firstComma = msg.indexOf(',');
      int secondComma = msg.indexOf(',', firstComma + 1);

      if (secondComma > 0) {
        String direction = msg.substring(firstComma + 1, secondComma);
        String content = msg.substring(secondComma + 1);
        bool isOutgoing = (direction == "OUT");

        // Message bubble container
        lv_obj_t *bubble_container = lv_obj_create(conversation_list);
        lv_obj_set_width(bubble_container, lv_pct(100));
        lv_obj_set_height(bubble_container, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(bubble_container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(bubble_container, 0, 0);
        lv_obj_set_style_pad_all(bubble_container, 2, 0);
        lv_obj_clear_flag(bubble_container, LV_OBJ_FLAG_SCROLLABLE);

        // Message bubble
        lv_obj_t *bubble = lv_obj_create(bubble_container);
        lv_obj_set_width(bubble, lv_pct(75));
        lv_obj_set_height(bubble, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_all(bubble, 8, 0);
        lv_obj_set_style_radius(bubble, 10, 0);
        lv_obj_set_style_border_width(bubble, 0, 0);

        // Make bubble clickable for long-press delete
        lv_obj_add_flag(bubble, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(bubble, conversation_msg_clicked, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
        lv_obj_add_event_cb(bubble, conversation_msg_longpress,
                            LV_EVENT_LONG_PRESSED, (void *)(intptr_t)i);

        if (isOutgoing) {
          lv_obj_align(bubble, LV_ALIGN_RIGHT_MID, 0, 0);
          lv_obj_set_style_bg_color(bubble, lv_color_hex(0x82aaff), 0);
        } else {
          lv_obj_align(bubble, LV_ALIGN_LEFT_MID, 0, 0);
          lv_obj_set_style_bg_color(bubble, lv_color_hex(0x2a2a3e), 0);
        }

        // Message text
        lv_obj_t *msg_label = lv_label_create(bubble);
        lv_label_set_text(msg_label, content.c_str());
        lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(msg_label, lv_pct(100));
        lv_obj_set_style_text_color(msg_label, lv_color_hex(0xffffff), 0);
      }
    }
  }

  lv_obj_scroll_to_y(conversation_list, 0, LV_ANIM_OFF);
  Serial.printf("[LVGL] Conversation messages refreshed: %d messages\n",
                messages.size());
}

// Confirmation callback for conversation message deletion
static void confirm_conversation_delete_cb(lv_event_t *e) {
  (void)e;
  if (!conversation_confirm_msgbox)
    return;

  const char *btn_text =
      lv_msgbox_get_active_btn_text(conversation_confirm_msgbox);

  need_conversation_refresh = false;
  if (btn_text && strcmp(btn_text, "Yes") == 0) {
    MSG_Utils::deleteMessageFromConversation(current_conversation_callsign,
                                             pending_conversation_msg_delete);
    need_conversation_refresh = true;
  }

  // Schedule deletion via timer (can't delete object during its own event)
  msgbox_to_delete = conversation_confirm_msgbox;
  conversation_confirm_msgbox = nullptr;
  pending_conversation_msg_delete = -1;
  lv_timer_create(delete_msgbox_timer_cb, 10, NULL);
}

// Show delete confirmation for conversation message
static void show_conversation_delete_confirmation(int msg_index) {
  // Prevent creating duplicate msgbox if one already exists
  if (conversation_confirm_msgbox != nullptr)
    return;

  pending_conversation_msg_delete = msg_index;

  static const char *btns[] = {"Yes", "No", ""};
  conversation_confirm_msgbox = lv_msgbox_create(
      lv_layer_top(), "Delete message?", "Delete this message?", btns, false);
  lv_obj_set_style_bg_color(conversation_confirm_msgbox, lv_color_hex(0x1a1a2e),
                            0);
  lv_obj_set_style_bg_opa(conversation_confirm_msgbox, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(conversation_confirm_msgbox,
                              lv_color_hex(0xffffff), LV_PART_MAIN);
  lv_obj_set_width(conversation_confirm_msgbox, 240);
  lv_obj_center(conversation_confirm_msgbox);
  lv_obj_add_event_cb(conversation_confirm_msgbox,
                      confirm_conversation_delete_cb, LV_EVENT_VALUE_CHANGED,
                      NULL);
}

// Long-press callback for conversation message bubble
static void conversation_msg_longpress(lv_event_t *e) {
  int msg_index = (int)(intptr_t)lv_event_get_user_data(e);
  Serial.printf("[LVGL] Conversation message long-press: index %d\n",
                msg_index);
  conversation_msg_longpress_handled = true;
  show_conversation_delete_confirmation(msg_index);
}

// Click callback for conversation message bubble (to prevent action after
// long-press)
static void conversation_msg_clicked(lv_event_t *e) {
  if (conversation_msg_longpress_handled) {
    conversation_msg_longpress_handled = false;
    return;
  }
  // No action on simple click for now
}

// Conversation item clicked - open conversation screen
static void conversation_item_clicked(lv_event_t *e) {
  if (msg_longpress_handled) {
    msg_longpress_handled = false;
    return;
  }

  const char *callsign = (const char *)lv_event_get_user_data(e);
  if (!callsign)
    return;

  Serial.printf("[LVGL] Conversation clicked: %s\n", callsign);
  create_conversation_screen(String(callsign));
}

// Message item long-press - delete single message
static void msg_item_longpress(lv_event_t *e) {
  int msg_index = (int)(intptr_t)lv_event_get_user_data(e);
  Serial.printf("[LVGL] Message long-press: index %d\n", msg_index);
  msg_longpress_handled = true;
  show_delete_confirmation("Delete this message?", msg_index);
}

// Populate message list - now shows conversations instead of all messages
static void populate_msg_list(lv_obj_t *list, int type) {
  lv_obj_clean(list);

  /*if (type == 0) {
    // APRS messages - show conversations
    std::vector<String> conversations = MSG_Utils::getConversationsList();

    // IMPORTANT: Clear static storage to prevent memory leak and stale pointers
    static std::vector<String> callsign_storage;
    callsign_storage.clear(); // Vider le vecteur à chaque appel

    if (conversations.size() == 0) {
      lv_obj_t *empty = lv_label_create(list);
      lv_label_set_text(empty, "No conversations");
      lv_obj_set_style_text_color(empty, lv_color_hex(0x888888), 0);
    } else {
      // Remplir d'abord callsign_storage avec toutes les String
      callsign_storage.reserve(conversations.size()); // Pré-allouer pour éviter les réallocations fréquentes
      for (size_t i = 0; i < conversations.size(); i++) {
          callsign_storage.push_back(conversations[i]);
      }

      // Ensuite, créer les boutons en utilisant les pointeurs stables
      for (size_t i = 0; i < conversations.size(); i++) {
        // Get last message preview
        std::vector<String> messages =
            MSG_Utils::getMessagesForContact(conversations[i]);
        String preview = conversations[i];
        if (messages.size() > 0) {
          // Parse last message: timestamp,direction,content
          String lastMsg = messages[messages.size() - 1];
          int firstComma = lastMsg.indexOf(',');
          int secondComma = lastMsg.indexOf(',', firstComma + 1);
          if (secondComma > 0) {
            String msgContent = lastMsg.substring(secondComma + 1);
            if (msgContent.length() > 30) {
              msgContent = msgContent.substring(0, 27) + "...";
            }
            preview += "\n" + msgContent;
          }
        }
          
        lv_obj_t *btn =
            lv_list_add_btn(list, LV_SYMBOL_ENVELOPE, preview.c_str());
        // Utiliser le pointeur de l'élément correspondant dans callsign_storage
        lv_obj_add_event_cb(
            btn, conversation_item_clicked, LV_EVENT_CLICKED,
            (void *)callsign_storage[i].c_str());
      }
    }
  } else {
    // Winlink messages - keep old behavior for now
    MSG_Utils::loadMessagesFromMemory(1);
    std::vector<String> &messages = MSG_Utils::getLoadedWLNKMails();

    if (messages.size() == 0) {
      lv_obj_t *empty = lv_label_create(list);
      lv_label_set_text(empty, "No Winlink mails");
      lv_obj_set_style_text_color(empty, lv_color_hex(0x888888), 0);
    } else {
      for (size_t i = 0; i < messages.size(); i++) {
        lv_obj_t *btn =
            lv_list_add_btn(list, LV_SYMBOL_ENVELOPE, messages[i].c_str());
        lv_obj_add_event_cb(btn, msg_item_clicked, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(btn, msg_item_longpress, LV_EVENT_LONG_PRESSED,
                            (void *)(intptr_t)i);
      }
    }
  }
}*/
  
  if (type == 0) {                                                                                                                                                                                                                                                                        
      // APRS messages - show conversations                                                                                                                                                                                                                                                 
      std::vector<String> conversations = MSG_Utils::getConversationsList();                                                                                                                                                                                                                
                                                                                                                                                                                                                                                                                            
      // IMPORTANT: Clear static storage to prevent memory leak and stale pointers                                                                                                                                                                                                          
      static std::vector<String> callsign_storage;                                                                                                                                                                                                                                          
      callsign_storage.clear(); // Vider le vecteur à chaque appel                                                                                                                                                                                                                          
                                                                                                                                                                                                                                                                                            
      if (conversations.size() == 0) {                                                                                                                                                                                                                                                      
        lv_obj_t *empty = lv_label_create(list);                                                                                                                                                                                                                                            
        lv_label_set_text(empty, "No conversations");                                                                                                                                                                                                                                       
        lv_obj_set_style_text_color(empty, lv_color_hex(0x888888), 0);                                                                                                                                                                                                                      
      } else {                                                                                                                                                                                                                                                                              
        // Remplir d'abord callsign_storage avec toutes les String                                                                                                                                                                                                                          
        callsign_storage.reserve(conversations.size()); // Pré-allouer pour éviter les réallocations fréquentes                                                                                                                                                                             
        for (size_t i = 0; i < conversations.size(); i++) {                                                                                                                                                                                                                                 
            callsign_storage.push_back(conversations[i]);                                                                                                                                                                                                                                   
        }                                                                                                                                                                                                                                                                                   
                                                                                                                                                                                                                                                                                            
        // Ensuite, créer les boutons en utilisant les pointeurs stables
        // Afficher du plus récent (fin du vecteur) au plus ancien (début)
        for (int i = conversations.size() - 1; i >= 0; i--) {
          // Get last message preview
          std::vector<String> messages =
              MSG_Utils::getMessagesForContact(conversations[i]);
          String preview = conversations[i];
          if (messages.size() > 0) {
            // Parse last message: timestamp,direction,content
            String lastMsg = messages[messages.size() - 1];
            int firstComma = lastMsg.indexOf(',');
            int secondComma = lastMsg.indexOf(',', firstComma + 1);
            if (secondComma > 0) {
              String msgContent = lastMsg.substring(secondComma + 1);
              if (msgContent.length() > 30) {
                msgContent = msgContent.substring(0, 27) + "...";
              }
              preview += "\n" + msgContent;
            }
          }
          
          lv_obj_t *btn =
              lv_list_add_btn(list, LV_SYMBOL_ENVELOPE, preview.c_str());
          // Utiliser le pointeur de l'élément correspondant dans callsign_storage
          lv_obj_add_event_cb(
              btn, conversation_item_clicked, LV_EVENT_CLICKED,
              (void *)callsign_storage[i].c_str());
        }
      }                                                                                                                                                                                                                                                                                     
    } else {                                                                                                                                                                                                                                                                                
      // Winlink messages - show from newest to oldest                                                                                                                                                                                                                                      
      MSG_Utils::loadMessagesFromMemory(1);                                                                                                                                                                                                                                                 
      std::vector<String> &messages = MSG_Utils::getLoadedWLNKMails();                                                                                                                                                                                                                      
                                                                                                                                                                                                                                                                                            
      if (messages.size() == 0) {                                                                                                                                                                                                                                                           
        lv_obj_t *empty = lv_label_create(list);                                                                                                                                                                                                                                            
        lv_label_set_text(empty, "No Winlink mails");                                                                                                                                                                                                                                       
        lv_obj_set_style_text_color(empty, lv_color_hex(0x888888), 0);                                                                                                                                                                                                                      
      } else {                                                                                                                                                                                                                                                                              
        // Afficher du plus récent (fin du vecteur) au plus ancien (début)                                                                                                                                                                                                                  
        for (int i = messages.size() - 1; i >= 0; i--) {                                                                                                                                                                                                                                    
          lv_obj_t *btn =                                                                                                                                                                                                                                                                   
              lv_list_add_btn(list, LV_SYMBOL_ENVELOPE, messages[i].c_str());                                                                                                                                                                                                               
          lv_obj_add_event_cb(btn, msg_item_clicked, LV_EVENT_CLICKED, NULL);                                                                                                                                                                                                               
          lv_obj_add_event_cb(btn, msg_item_longpress, LV_EVENT_LONG_PRESSED,                                                                                                                                                                                                               
                              (void *)(intptr_t)i);                                                                                                                                                                                                                                         
        }                                                                                                                                                                                                                                                                                   
      }                                                                                                                                                                                                                                                                                     
    }
  }

// Back button callback for conversation screen
static void btn_conversation_back_clicked(lv_event_t *e) {
  if (screen_msg) {
    // Refresh the conversations list before going back
    if (list_aprs_global) {
      populate_msg_list(list_aprs_global, 0);
    }
    lv_scr_load_anim(screen_msg, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
  }
}

// Reply button callback for conversation screen
static void btn_conversation_reply_clicked(lv_event_t *e) {
  if (current_conversation_callsign.length() > 0) {
    create_compose_screen();
    compose_screen_active = true;
    compose_return_screen = lv_scr_act(); // Store current screen to return to
    lv_textarea_set_text(compose_to_input,
                         current_conversation_callsign.c_str());
    current_focused_input = compose_msg_input;
    lv_keyboard_set_textarea(compose_keyboard, compose_msg_input);
    lv_scr_load_anim(screen_compose, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
  }
}

// Create conversation screen
static void create_conversation_screen(const String &callsign) {
  current_conversation_callsign = callsign;

  // Check if we're refreshing the same screen (for delete message case)
  bool isRefresh =
      (screen_conversation != nullptr && lv_scr_act() == screen_conversation);
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
  lv_obj_add_event_cb(btn_back, btn_conversation_back_clicked, LV_EVENT_CLICKED,
                      NULL);
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
  lv_obj_add_event_cb(btn_reply, btn_conversation_reply_clicked,
                      LV_EVENT_CLICKED, NULL);
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
  lv_obj_set_flex_align(conversation_list, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  // Load and display messages
  std::vector<String> messages = MSG_Utils::getMessagesForContact(callsign);

  if (messages.size() == 0) {
    lv_obj_t *empty = lv_label_create(conversation_list);
    lv_label_set_text(empty, "No messages");
    lv_obj_set_style_text_color(empty, lv_color_hex(0x888888), 0);
  } else {
    for (int i = messages.size() - 1; i >= 0; i--) {
      // Parse: timestamp,direction,content
      String msg = messages[i];
      int firstComma = msg.indexOf(',');
      int secondComma = msg.indexOf(',', firstComma + 1);

      if (secondComma > 0) {
        String direction = msg.substring(firstComma + 1, secondComma);
        String content = msg.substring(secondComma + 1);
        bool isOutgoing = (direction == "OUT");

        // Message bubble container
        lv_obj_t *bubble_container = lv_obj_create(conversation_list);
        lv_obj_set_width(bubble_container, lv_pct(100));
        lv_obj_set_height(bubble_container, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(bubble_container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(bubble_container, 0, 0);
        lv_obj_set_style_pad_all(bubble_container, 2, 0);
        lv_obj_clear_flag(bubble_container, LV_OBJ_FLAG_SCROLLABLE);

        // Message bubble
        lv_obj_t *bubble = lv_obj_create(bubble_container);
        lv_obj_set_width(bubble, lv_pct(75));
        lv_obj_set_height(bubble, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_all(bubble, 8, 0);
        lv_obj_set_style_radius(bubble, 10, 0);
        lv_obj_set_style_border_width(bubble, 0, 0);

        // Make bubble clickable for long-press delete
        lv_obj_add_flag(bubble, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(bubble, conversation_msg_clicked, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
        lv_obj_add_event_cb(bubble, conversation_msg_longpress,
                            LV_EVENT_LONG_PRESSED, (void *)(intptr_t)i);

        if (isOutgoing) {
          // Outgoing message - align right, blue
          lv_obj_align(bubble, LV_ALIGN_RIGHT_MID, 0, 0);
          lv_obj_set_style_bg_color(bubble, lv_color_hex(0x82aaff), 0);
        } else {
          // Incoming message - align left, gray
          lv_obj_align(bubble, LV_ALIGN_LEFT_MID, 0, 0);
          lv_obj_set_style_bg_color(bubble, lv_color_hex(0x2a2a3e), 0);
        }

        // Message text
        lv_obj_t *msg_label = lv_label_create(bubble);
        lv_label_set_text(msg_label, content.c_str());
        lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(msg_label, lv_pct(100));
        lv_obj_set_style_text_color(msg_label, lv_color_hex(0xffffff), 0);
      }
    }
  }

  // Scroll to bottom to show oldest messages
  lv_obj_scroll_to_y(conversation_list, 0, LV_ANIM_OFF);

  if (isRefresh) {
    // Refreshing same screen - no animation, just swap
    lv_disp_load_scr(screen_conversation);
    if (old_screen) {
      lv_obj_del(old_screen);
    }
  } else {
    // Opening from another screen - use animation
    if (old_screen) {
      lv_obj_del(old_screen);
    }
    lv_scr_load_anim(screen_conversation, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0,
                     false);
  }
  Serial.printf("[LVGL] Conversation screen created for %s with %d messages\n",
                callsign.c_str(), messages.size());
}

// Contact add/edit screen variables
static lv_obj_t *screen_contact_edit = nullptr;
static lv_obj_t *contact_callsign_input = nullptr;
static lv_obj_t *contact_name_input = nullptr;
static lv_obj_t *contact_comment_input = nullptr;
static lv_obj_t *contact_edit_keyboard = nullptr;
static lv_obj_t *contact_current_input = nullptr;
static String editing_contact_callsign =
    ""; // Empty = adding new, non-empty = editing

// Forward declarations for contacts
static void populate_contacts_list(lv_obj_t *list);
static void show_contact_edit_screen(const Contact *contact);

// Forward declaration
static void create_compose_screen();

// Flag to prevent click after long-press
static bool contact_longpress_handled = false;

// Contact item clicked - open compose with callsign pre-filled
static void contact_item_clicked(lv_event_t *e) {
  // Skip if this click follows a long-press
  if (contact_longpress_handled) {
    contact_longpress_handled = false;
    return;
  }

  Contact *contact = (Contact *)lv_event_get_user_data(e);
  if (!contact)
    return;

  Serial.printf("[LVGL] Contact clicked: %s - opening compose\n",
                contact->callsign.c_str());

  // Create compose screen and pre-fill recipient
  create_compose_screen();
  compose_screen_active = true;
  compose_return_screen = lv_scr_act(); // Store current screen to return to

  // Pre-fill the To field with contact's callsign
  lv_textarea_set_text(compose_to_input, contact->callsign.c_str());

  // Focus on message input (skip To field since it's already filled)
  current_focused_input = compose_msg_input;
  lv_keyboard_set_textarea(compose_keyboard, compose_msg_input);

  lv_scr_load_anim(screen_compose, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
}

// Contact long-press - open edit screen
static void contact_item_longpress(lv_event_t *e) {
  Contact *contact = (Contact *)lv_event_get_user_data(e);
  if (!contact)
    return;
  Serial.printf("[LVGL] Contact long-press: %s\n", contact->callsign.c_str());
  contact_longpress_handled = true; // Block the following click event
  show_contact_edit_screen(contact);
}

// Populate contacts list
static void populate_contacts_list(lv_obj_t *list) {
  lv_obj_clean(list);

  std::vector<Contact> contacts = STORAGE_Utils::loadContacts();

  if (contacts.size() == 0) {
    lv_obj_t *empty = lv_label_create(list);
    lv_label_set_text(empty, "No contacts\nTap + to add");
    lv_obj_set_style_text_color(empty, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
  } else {
    for (size_t i = 0; i < contacts.size(); i++) {
      Contact *c = STORAGE_Utils::findContact(contacts[i].callsign);
      String display = contacts[i].callsign;
      if (contacts[i].name.length() > 0) {
        display += " - " + contacts[i].name;
      }
      if (contacts[i].comment.length() > 0) {
        display += "\n" + contacts[i].comment;
      }
      lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_CALL, display.c_str());
      lv_obj_add_event_cb(btn, contact_item_clicked, LV_EVENT_CLICKED, c);
      lv_obj_add_event_cb(btn, contact_item_longpress, LV_EVENT_LONG_PRESSED,
                          c);
    }
  }
}

// Populate frames list (raw LoRa frames log)
static void populate_frames_list(lv_obj_t *list) {
  lv_obj_clean(list);

  const std::vector<String> &frames = STORAGE_Utils::getLastFrames(50);

  if (frames.size() == 0) {
    lv_obj_t *empty = lv_label_create(list);
    lv_label_set_text(empty, "No frames recorded\n(Requires SD card)");
    lv_obj_set_style_text_color(empty, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
  } else {
    // Display frames in reverse order (newest first)
    for (int i = frames.size() - 1; i >= 0; i--) {
      // Create container for each frame
      lv_obj_t *cont = lv_obj_create(list);
      lv_obj_set_width(cont, lv_pct(100));
      lv_obj_set_height(cont, LV_SIZE_CONTENT);
      lv_obj_set_style_bg_color(cont, lv_color_hex(0x0a0a14),
                                0); // Darker background
      lv_obj_set_style_pad_all(cont, 4, 0);
      lv_obj_set_style_border_width(cont, 1, 0);
      lv_obj_set_style_border_color(cont, lv_color_hex(0x333344), 0);
      lv_obj_set_style_border_side(cont, LV_BORDER_SIDE_BOTTOM, 0);
      lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

      // Create label with word wrap
      lv_obj_t *label = lv_label_create(cont);
      lv_label_set_text(label, frames[i].c_str());
      lv_obj_set_width(label, lv_pct(100));
      lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP); // Word wrap
      lv_obj_set_style_text_color(label, lv_color_hex(0x759a9e), 0);
      lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    }
  }
}

// Populate stats display
static void populate_stats(lv_obj_t *cont) {
  LinkStats stats = STORAGE_Utils::getStats();

  // CONDITION DE GARDE : Si le compteur RX n'a pas bougé et n'est pas zéro, on ne fait RIEN
  if (stats.rxCount == last_processed_rx_count && stats.rxCount > 0) {
    return;
  }
  last_processed_rx_count = stats.rxCount;

  // Check available heap - skip if too low to avoid memory issues
  uint32_t freeHeap = ESP.getFreeHeap();
  if (freeHeap < 8000) {
#ifdef DEBUG
    Serial.printf("[LVGL] Stats update skipped - low memory (%u bytes)\n",
                  freeHeap);
#endif
    return; // Don't update if memory is critically low
  }

  const std::vector<DigiStats> &digis = STORAGE_Utils::getDigiStats();
  char buf[128];

  // Link statistics section - Create if not exists, then update
  if (!stats_title_lbl) {
    stats_title_lbl = lv_label_create(cont);
    if (stats_title_lbl) { // Vérifier la création réussie
      lv_label_set_text(stats_title_lbl, "Link Statistics");
      lv_obj_set_style_text_color(stats_title_lbl, lv_color_hex(0x4CAF50), 0);
      lv_obj_set_style_text_font(stats_title_lbl, &lv_font_montserrat_14, 0);
    }

    stats_rx_tx_counts_lbl = lv_label_create(cont);
    if (stats_rx_tx_counts_lbl) { // Vérifier la création réussie
      lv_obj_set_style_text_color(stats_rx_tx_counts_lbl,
                                  lv_color_hex(0x759a9e), 0);
    }

    stats_rssi_stats_lbl = lv_label_create(cont);
    if (stats_rssi_stats_lbl) { // Vérifier la création réussie
      lv_obj_set_style_text_color(stats_rssi_stats_lbl, lv_color_hex(0x759a9e),
                                  0);
    }

    stats_snr_stats_lbl = lv_label_create(cont);
    if (stats_snr_stats_lbl) { // Vérifier la création réussie
      lv_obj_set_style_text_color(stats_snr_stats_lbl, lv_color_hex(0x759a9e),
                                  0);
    }

    // RSSI Chart legend
    stats_rssi_chart_legend_lbl = lv_label_create(cont);
    if (stats_rssi_chart_legend_lbl) { // Vérifier la création réussie
      lv_label_set_text(stats_rssi_chart_legend_lbl, "\nRSSI (dBm)");
      lv_obj_set_style_text_color(stats_rssi_chart_legend_lbl,
                                  lv_color_hex(0x00BFFF), 0);
    }

    // RSSI Chart
    stats_rssi_chart_obj = lv_chart_create(cont);
    if (stats_rssi_chart_obj) { // Vérifier la création réussie
      lv_obj_set_size(stats_rssi_chart_obj, 280, 50);
      lv_chart_set_type(stats_rssi_chart_obj, LV_CHART_TYPE_LINE);
      lv_obj_set_style_bg_color(stats_rssi_chart_obj, lv_color_hex(0x1a1a2e),
                                0);
      lv_obj_set_style_line_color(stats_rssi_chart_obj, lv_color_hex(0x333344),
                                  LV_PART_MAIN);
      lv_chart_set_div_line_count(stats_rssi_chart_obj, 3, 0);
      stats_rssi_chart_ser =
          lv_chart_add_series(stats_rssi_chart_obj, lv_color_hex(0x00BFFF),
                              LV_CHART_AXIS_PRIMARY_Y);
      if (stats_rssi_chart_ser) { // Vérifier la création de la série
        lv_obj_add_flag(stats_rssi_chart_obj,
                        LV_OBJ_FLAG_HIDDEN); // Cacher initialement s'il n'y a
                                             // pas de données
      }
    }
    if (stats_rssi_chart_legend_lbl) { // Vérifier l'étiquette séparément, car
                                       // elle peut être créée même si le
                                       // graphique a échoué
      lv_obj_add_flag(stats_rssi_chart_legend_lbl, LV_OBJ_FLAG_HIDDEN);
    }

    // SNR Chart legend
    stats_snr_chart_legend_lbl = lv_label_create(cont);
    if (stats_snr_chart_legend_lbl) { // Vérifier la création réussie
      lv_label_set_text(stats_snr_chart_legend_lbl, "SNR (dB)");
      lv_obj_set_style_text_color(stats_snr_chart_legend_lbl,
                                  lv_color_hex(0x00FF7F), 0);
    }

    // SNR Chart
    stats_snr_chart_obj = lv_chart_create(cont);
    if (stats_snr_chart_obj) { // Vérifier la création réussie
      lv_obj_set_size(stats_snr_chart_obj, 280, 50);
      lv_chart_set_type(stats_snr_chart_obj, LV_CHART_TYPE_LINE);
      lv_obj_set_style_bg_color(stats_snr_chart_obj, lv_color_hex(0x1a1a2e), 0);
      lv_obj_set_style_line_color(stats_snr_chart_obj, lv_color_hex(0x333344),
                                  LV_PART_MAIN);
      lv_chart_set_div_line_count(stats_snr_chart_obj, 3, 0);
      stats_snr_chart_ser = lv_chart_add_series(
          stats_snr_chart_obj, lv_color_hex(0x00FF7F), LV_CHART_AXIS_PRIMARY_Y);
      if (stats_snr_chart_ser) { // Vérifier la création de la série
        lv_obj_add_flag(stats_snr_chart_obj,
                        LV_OBJ_FLAG_HIDDEN); // Cacher initialement s'il n'y a
                                             // pas de données
      }
    }
    if (stats_snr_chart_legend_lbl) { // Vérifier l'étiquette séparément
      lv_obj_add_flag(stats_snr_chart_legend_lbl, LV_OBJ_FLAG_HIDDEN);
    }

    // Digipeaters section title
    stats_digi_title_lbl = lv_label_create(cont);
    if (stats_digi_title_lbl) { // Vérifier la création réussie
      lv_label_set_text(stats_digi_title_lbl, "\nDigipeaters/IGates");
      lv_obj_set_style_text_color(stats_digi_title_lbl, lv_color_hex(0x4CAF50),
                                  0);
      lv_obj_set_style_text_font(stats_digi_title_lbl, &lv_font_montserrat_14,
                                 0);
    }

    // Container for digipeater list (its children will be cleaned and
    // repopulated)
    stats_digi_list_container = lv_obj_create(cont);
    if (stats_digi_list_container) { // Vérifier la création réussie
      lv_obj_set_size(stats_digi_list_container, lv_pct(100), LV_SIZE_CONTENT);
      lv_obj_set_style_bg_opa(stats_digi_list_container, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_width(stats_digi_list_container, 0, 0);
      lv_obj_set_style_pad_all(stats_digi_list_container, 0, 0);
      lv_obj_set_flex_flow(stats_digi_list_container, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_flex_align(stats_digi_list_container, LV_FLEX_ALIGN_START,
                            LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    }

    stats_no_digi_lbl = lv_label_create(
        stats_digi_list_container); // Créer dans le conteneur des digis (et réutilisé)
    if (stats_no_digi_lbl) {        // Vérifier la création réussie
      lv_label_set_text(stats_no_digi_lbl, "No digipeaters seen yet"); // Texte initial
      lv_obj_set_style_text_color(stats_no_digi_lbl, lv_color_hex(0x888888), 0);
      lv_label_set_long_mode(stats_no_digi_lbl, LV_LABEL_LONG_WRAP); // Permettre le retour à la ligne
      lv_obj_set_width(stats_no_digi_lbl, lv_pct(100)); // Prend toute la largeur
    }
  }

  // Always update content
  // Update RX/TX counts
  if (stats_rx_tx_counts_lbl) { // Vérifier la création réussie
    snprintf(buf, sizeof(buf), "RX: %lu   TX: %lu   ACK: %lu",
             (unsigned long)stats.rxCount, (unsigned long)stats.txCount,
             (unsigned long)stats.ackCount);
    lv_label_set_text(stats_rx_tx_counts_lbl, buf);
  }

  // Update RSSI stats
  if (stats_rssi_stats_lbl) { // Vérifier la création réussie
    if (stats.rxCount > 0) {
      int rssiAvg = stats.rssiTotal / (int)stats.rxCount;
      snprintf(buf, sizeof(buf), "RSSI: %d avg  [%d / %d]", rssiAvg,
               stats.rssiMin, stats.rssiMax);
    } else {
      snprintf(buf, sizeof(buf), "RSSI: -- avg  [-- / --]");
    }
    lv_label_set_text(stats_rssi_stats_lbl, buf);
  }

  // Update SNR stats
  if (stats_snr_stats_lbl) { // Vérifier la création réussie
    if (stats.rxCount > 0) {
      float snrAvg = stats.snrTotal / (float)stats.rxCount;
      snprintf(buf, sizeof(buf), "SNR: %.1f avg  [%.1f / %.1f]", snrAvg,
               stats.snrMin, stats.snrMax);
    } else {
      snprintf(buf, sizeof(buf), "SNR: -- avg  [-- / --]");
    }
    lv_label_set_text(stats_snr_stats_lbl, buf);
  }

  // RSSI/SNR Charts
  const std::vector<int> &rssiHist = STORAGE_Utils::getRssiHistory();
  const std::vector<float> &snrHist = STORAGE_Utils::getSnrHistory();

  if (rssiHist.size() > 1) {
    lv_obj_clear_flag(stats_rssi_chart_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(stats_rssi_chart_legend_lbl, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(stats_snr_chart_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(stats_snr_chart_legend_lbl, LV_OBJ_FLAG_HIDDEN);

    // RSSI Chart update
    lv_chart_set_point_count(stats_rssi_chart_obj, rssiHist.size());
    // Find RSSI range for scaling
    int rssiMin = -120, rssiMax = -40;
    for (int v : rssiHist) {
      if (v < rssiMin)
        rssiMin = v;
      if (v > rssiMax)
        rssiMax = v;
    }
    lv_chart_set_range(stats_rssi_chart_obj, LV_CHART_AXIS_PRIMARY_Y,
                       rssiMin - 5, rssiMax + 5);

    // SAFE UPDATE: Copy values individually (avoid ext_y_array with
    // local/volatile pointers)
    for (size_t i = 0; i < rssiHist.size(); i++) {
      lv_chart_set_value_by_id(stats_rssi_chart_obj, stats_rssi_chart_ser, i,
                               (lv_coord_t)rssiHist[i]);
    }
    lv_chart_refresh(stats_rssi_chart_obj);

    // SNR Chart update
    lv_chart_set_point_count(stats_snr_chart_obj, snrHist.size());
    // Find SNR range for scaling
    float snrMinF = -10, snrMaxF = 15;
    for (float v : snrHist) {
      if (v < snrMinF)
        snrMinF = v;
      if (v > snrMaxF)
        snrMaxF = v;
    }
    lv_chart_set_range(stats_snr_chart_obj, LV_CHART_AXIS_PRIMARY_Y,
                       (int)((snrMinF - 2) * 10), (int)((snrMaxF + 2) * 10));

    // SAFE UPDATE: Copy values with scaling
    for (size_t i = 0; i < snrHist.size(); i++) {
      lv_chart_set_value_by_id(stats_snr_chart_obj, stats_snr_chart_ser, i,
                               (lv_coord_t)(snrHist[i] * 10));
    }
    lv_chart_refresh(stats_snr_chart_obj);

  } else {
    if (stats_rssi_chart_obj) {
      lv_obj_add_flag(stats_rssi_chart_obj, LV_OBJ_FLAG_HIDDEN);
    }
    if (stats_rssi_chart_legend_lbl) {
      lv_obj_add_flag(stats_rssi_chart_legend_lbl, LV_OBJ_FLAG_HIDDEN);
    }
    if (stats_snr_chart_obj) {
      lv_obj_add_flag(stats_snr_chart_obj, LV_OBJ_FLAG_HIDDEN);
    }
    if (stats_snr_chart_legend_lbl) {
      lv_obj_add_flag(stats_snr_chart_legend_lbl, LV_OBJ_FLAG_HIDDEN);
    }
  }

  // Section Digipeaters - toujours mettre à jour le label unique
  if (stats_digi_title_lbl) {
    lv_obj_clear_flag(stats_digi_title_lbl, LV_OBJ_FLAG_HIDDEN);
  }
  // Suppression de lv_obj_clean(stats_digi_list_container); ici, car on réutilise stats_no_digi_lbl

  if (digis.size() == 0) {
    if (stats_no_digi_lbl) {
      lv_obj_clear_flag(stats_no_digi_lbl, LV_OBJ_FLAG_HIDDEN);
      lv_label_set_text(stats_no_digi_lbl, "No digipeaters seen yet");
      lv_obj_set_style_text_color(stats_no_digi_lbl, lv_color_hex(0x888888), 0); // Reset color
    }
  } else {
    if (stats_no_digi_lbl) {
      lv_obj_clear_flag(stats_no_digi_lbl, LV_OBJ_FLAG_HIDDEN); // S'assurer qu'il est visible

      // Utiliser un vecteur de pointeurs pour le tri afin d'éviter les copies de
      // String et la fragmentation mémoire
      std::vector<const DigiStats *> sortedDigiPtrs;
      sortedDigiPtrs.reserve(digis.size());
      for (const auto &d : digis) {
        sortedDigiPtrs.push_back(&d);
      }
      // Trier par compte (décroissant)
      std::sort(sortedDigiPtrs.begin(), sortedDigiPtrs.end(),
                [](const DigiStats *a, const DigiStats *b) {
                  return a->count > b->count;
                });

      // Construire une seule String pour afficher tous les digipeaters
      String fullList = "";
      // Limiter aux 8 meilleurs pour ne pas saturer l'affichage, comme suggéré dans l'analyse
      int showCount = std::min((int)sortedDigiPtrs.size(), 8);
      for (int i = 0; i < showCount; i++) {
        fullList += String(sortedDigiPtrs[i]->callsign.c_str()) + ": " + String((unsigned long)sortedDigiPtrs[i]->count) + "\n";
      }
      lv_label_set_text(stats_no_digi_lbl, fullList.c_str());
      lv_obj_set_style_text_color(stats_no_digi_lbl, lv_color_hex(0x759a9e), 0); // Couleur du texte
    }
  }
}
  // Contact edit screen callbacks
  static void contact_edit_input_focused(lv_event_t * e) {
    lv_obj_t *ta = lv_event_get_target(e);
    contact_current_input = ta;
    if (contact_edit_keyboard) {
      lv_keyboard_set_textarea(contact_edit_keyboard, ta);
    }
  }

  static void contact_edit_keyboard_event(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
      lv_obj_add_flag(contact_edit_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
  }

  static void btn_contact_save_clicked(lv_event_t * e) {
    const char *callsign = lv_textarea_get_text(contact_callsign_input);
    const char *name = lv_textarea_get_text(contact_name_input);
    const char *comment = lv_textarea_get_text(contact_comment_input);

    if (strlen(callsign) == 0) {
      show_message_detail("Callsign required!");
      return;
    }

    Contact contact;
    contact.callsign = String(callsign);
    contact.callsign.toUpperCase();
    contact.name = String(name);
    contact.comment = String(comment);

    bool success;
    if (editing_contact_callsign.length() > 0) {
      // Editing existing contact
      success = STORAGE_Utils::updateContact(editing_contact_callsign, contact);
    } else {
      // Adding new contact
      success = STORAGE_Utils::addContact(contact);
    }

    if (success) {
      Serial.printf("[LVGL] Contact saved: %s\n", contact.callsign.c_str());
      // Go back to messages screen
      lv_scr_load(screen_msg);
      // Refresh contacts list
      if (list_contacts_global) {
        populate_contacts_list(list_contacts_global);
      }
    } else {
      show_message_detail("Failed to save contact\n(duplicate callsign?)");
    }
  }

  static void btn_contact_delete_clicked(lv_event_t * e) {
    if (editing_contact_callsign.length() > 0) {
      bool success = STORAGE_Utils::removeContact(editing_contact_callsign);
      if (success) {
        Serial.printf("[LVGL] Contact deleted: %s\n",
                      editing_contact_callsign.c_str());
        lv_scr_load(screen_msg);
        if (list_contacts_global) {
          populate_contacts_list(list_contacts_global);
        }
      }
    }
  }

  static void btn_contact_cancel_clicked(lv_event_t * e) {
    lv_scr_load(screen_msg);
  }

  static void btn_contact_kb_toggle_clicked(lv_event_t * e) {
    if (lv_obj_has_flag(contact_edit_keyboard, LV_OBJ_FLAG_HIDDEN)) {
      lv_obj_clear_flag(contact_edit_keyboard, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(contact_edit_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
  }

  // Show contact edit screen
  static void show_contact_edit_screen(const Contact *contact) {
    if (contact) {
      editing_contact_callsign = contact->callsign;
    } else {
      editing_contact_callsign = "";
    }

    // Create screen if not exists
    if (!screen_contact_edit) {
      screen_contact_edit = lv_obj_create(NULL);
      lv_obj_set_style_bg_color(screen_contact_edit, lv_color_hex(0x0f0f23), 0);

      // Title bar
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
      lv_obj_add_event_cb(btn_back, btn_contact_cancel_clicked,
                          LV_EVENT_CLICKED, NULL);
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
      lv_obj_add_event_cb(btn_save, btn_contact_save_clicked, LV_EVENT_CLICKED,
                          NULL);
      lv_obj_t *lbl_save = lv_label_create(btn_save);
      lv_label_set_text(lbl_save, LV_SYMBOL_OK);
      lv_obj_center(lbl_save);

      // Delete button
      lv_obj_t *btn_del = lv_btn_create(title_bar);
      lv_obj_set_size(btn_del, 40, 25);
      lv_obj_align(btn_del, LV_ALIGN_RIGHT_MID, -5, 0);
      lv_obj_set_style_bg_color(btn_del, lv_color_hex(0xff4444), 0);
      lv_obj_add_event_cb(btn_del, btn_contact_delete_clicked, LV_EVENT_CLICKED,
                          NULL);
      lv_obj_t *lbl_del = lv_label_create(btn_del);
      lv_label_set_text(lbl_del, LV_SYMBOL_TRASH);
      lv_obj_center(lbl_del);

      // Form container (scrollable for 3 fields)
      lv_obj_t *form = lv_obj_create(screen_contact_edit);
      lv_obj_set_size(form, SCREEN_WIDTH - 10, 160);
      lv_obj_set_pos(form, 5, 40);
      lv_obj_set_scrollbar_mode(form, LV_SCROLLBAR_MODE_AUTO);
      lv_obj_set_style_bg_color(form, lv_color_hex(0x1a1a2e), 0);
      lv_obj_set_style_border_width(form, 0, 0);
      lv_obj_set_style_pad_all(form, 5, 0);
      lv_obj_set_flex_flow(form, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_flex_align(form, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                            LV_FLEX_ALIGN_START);

      // Callsign input
      lv_obj_t *lbl_call = lv_label_create(form);
      lv_label_set_text(lbl_call, "Callsign:");
      lv_obj_set_style_text_color(lbl_call, lv_color_hex(0x0000cc), 0);

      contact_callsign_input = lv_textarea_create(form);
      lv_obj_set_size(contact_callsign_input, lv_pct(100), 30);
      lv_textarea_set_one_line(contact_callsign_input, true);
      lv_textarea_set_placeholder_text(contact_callsign_input, "F4ABC-9");
      lv_obj_add_event_cb(contact_callsign_input, contact_edit_input_focused,
                          LV_EVENT_FOCUSED, NULL);

      // Name input
      lv_obj_t *lbl_name = lv_label_create(form);
      lv_label_set_text(lbl_name, "Name:");
      lv_obj_set_style_text_color(lbl_name, lv_color_hex(0x0000cc), 0);

      contact_name_input = lv_textarea_create(form);
      lv_obj_set_size(contact_name_input, lv_pct(100), 30);
      lv_textarea_set_one_line(contact_name_input, true);
      lv_textarea_set_placeholder_text(contact_name_input, "Jean");
      lv_obj_add_event_cb(contact_name_input, contact_edit_input_focused,
                          LV_EVENT_FOCUSED, NULL);

      // Comment input
      lv_obj_t *lbl_comment = lv_label_create(form);
      lv_label_set_text(lbl_comment, "Note:");
      lv_obj_set_style_text_color(lbl_comment, lv_color_hex(0x0000cc), 0);

      contact_comment_input = lv_textarea_create(form);
      lv_obj_set_size(contact_comment_input, lv_pct(100), 30);
      lv_textarea_set_one_line(contact_comment_input, true);
      lv_textarea_set_placeholder_text(contact_comment_input, "Paris");
      lv_obj_add_event_cb(contact_comment_input, contact_edit_input_focused,
                          LV_EVENT_FOCUSED, NULL);

      // Keyboard toggle button
      lv_obj_t *btn_kb = lv_btn_create(screen_contact_edit);
      lv_obj_set_size(btn_kb, 40, 30);
      lv_obj_set_pos(btn_kb, SCREEN_WIDTH - 50, 165);
      lv_obj_set_style_bg_color(btn_kb, lv_color_hex(0x555555), 0);
      lv_obj_add_event_cb(btn_kb, btn_contact_kb_toggle_clicked,
                          LV_EVENT_CLICKED, NULL);
      lv_obj_t *lbl_kb = lv_label_create(btn_kb);
      lv_label_set_text(lbl_kb, LV_SYMBOL_KEYBOARD);
      lv_obj_center(lbl_kb);

      // Virtual keyboard (hidden by default)
      contact_edit_keyboard = lv_keyboard_create(screen_contact_edit);
      lv_obj_set_size(contact_edit_keyboard, SCREEN_WIDTH, 100);
      lv_obj_align(contact_edit_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
      lv_obj_add_flag(contact_edit_keyboard, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_event_cb(contact_edit_keyboard, contact_edit_keyboard_event,
                          LV_EVENT_ALL, NULL);
    }

    // Fill form with contact data
    if (contact) {
      lv_textarea_set_text(contact_callsign_input, contact->callsign.c_str());
      lv_textarea_set_text(contact_name_input, contact->name.c_str());
      lv_textarea_set_text(contact_comment_input, contact->comment.c_str());
    } else {
      lv_textarea_set_text(contact_callsign_input, "");
      lv_textarea_set_text(contact_name_input, "");
      lv_textarea_set_text(contact_comment_input, "");
    }

    lv_scr_load(screen_contact_edit);

    // Set focus to first input for physical keyboard
    lv_obj_add_state(contact_callsign_input, LV_STATE_FOCUSED);
    contact_current_input = contact_callsign_input;
  }

  // Tab changed callback
  static void msg_tab_changed(lv_event_t * e) {
    lv_obj_t *tabview = lv_event_get_target(e);

    // Verify this is actually the tabview object
    if (!tabview || tabview != msg_tabview) {
      return;
    }

    uint16_t tab_idx = lv_tabview_get_tab_act(tabview);

    // Validate tab index (only 0, 1, 2, 3, 4 are valid)
    if (tab_idx > 4 || tab_idx == 0xFFFF) {
      return; // Invalid index, silently ignore
    }

    // Only process if the tab actually changed
    if (current_msg_type != (int)tab_idx) {
      current_msg_type = (int)tab_idx;
      Serial.printf("[LVGL] Messages tab changed to %d\n", current_msg_type);

      // Lazy load each tab when first selected
      if (current_msg_type == 1 && list_wlnk_global) {
        populate_msg_list(list_wlnk_global, 1);
      }
      if (current_msg_type == 2 && list_contacts_global) {
        populate_contacts_list(list_contacts_global);
      }
      if (current_msg_type == 3 && list_frames_global) {
        populate_frames_list(list_frames_global);
      }
      if (current_msg_type == 4 && cont_stats_global) {
        populate_stats(cont_stats_global);
      }
    }
  }

  // Delete all messages callback
  static void btn_delete_msgs_clicked(lv_event_t * e) {
    Serial.printf("[LVGL] Delete all button pressed, type %d\n",
                  current_msg_type);

    // Don't delete contacts, frames or stats with this button
    if (current_msg_type >= 2) {
      return;
    }

    const char *msg = (current_msg_type == 0) ? "Delete all APRS messages?"
                                              : "Delete all Winlink mails?";
    show_delete_confirmation(msg, -1); // -1 means delete all
  }

  // Compose screen callbacks
  static void compose_keyboard_event(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
      lv_obj_add_flag(compose_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
  }

  static void compose_input_focused(lv_event_t * e) {
    lv_obj_t *ta = lv_event_get_target(e);
    current_focused_input = ta;
    lv_keyboard_set_textarea(compose_keyboard, ta);
    // Don't show keyboard automatically - user can toggle with button
  }

  static void btn_send_msg_clicked(lv_event_t * e) {
    const char *to = lv_textarea_get_text(compose_to_input);
    const char *msg = lv_textarea_get_text(compose_msg_input);

    if (strlen(to) > 0 && strlen(msg) > 0) {
      Serial.printf("[LVGL] Sending message to %s: %s\n", to, msg);
      MSG_Utils::addToOutputBuffer(1, String(to), String(msg));

      // Save sent message to aprsMessages.txt (same format as received
      // messages) Format: "TO_CALLSIGN,>message" (> prefix indicates sent
      // message)
      if (xSemaphoreTakeRecursive(spiMutex, portMAX_DELAY) == pdTRUE) {
        File msgFile =
            STORAGE_Utils::openFile("/aprsMessages.txt", FILE_APPEND);
        if (msgFile) {
          String sentMsg = String(to) + ",>" + String(msg);
          msgFile.println(sentMsg);
          msgFile.close();
          MSG_Utils::loadNumMessages();
        }
        xSemaphoreGiveRecursive(spiMutex);
      }

      // Save to conversation file
      MSG_Utils::saveToConversation(String(to), String(msg), true);

      // Show confirmation popup
      LVGL_UI::showTxPacket(msg);

      // Clear inputs and go back
      lv_textarea_set_text(compose_to_input, "");
      lv_textarea_set_text(compose_msg_input, "");
      compose_screen_active = false;

      // Return to the screen we came from
      if (compose_return_screen && lv_obj_is_valid(compose_return_screen)) {
        // If returning to conversation screen, refresh messages
        if (compose_return_screen == screen_conversation) {
          refresh_conversation_messages();
        }
        lv_scr_load_anim(compose_return_screen, LV_SCR_LOAD_ANIM_MOVE_RIGHT,
                         100, 0, false);
      } else {
        lv_scr_load_anim(screen_main, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0,
                         false);
      }
    }
  }

  static void btn_compose_back_clicked(lv_event_t * e) {
    compose_screen_active = false;
    // Return to the screen we came from (map, messages, or dashboard)
    if (compose_return_screen && lv_obj_is_valid(compose_return_screen)) {
      lv_scr_load_anim(compose_return_screen, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100,
                       0, false);
    } else {
      lv_scr_load_anim(screen_main, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
    }
  }

  static void create_compose_screen() {
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
    lv_obj_add_event_cb(btn_back, btn_compose_back_clicked, LV_EVENT_CLICKED,
                        NULL);
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
    lv_obj_add_event_cb(compose_to_input, compose_input_focused,
                        LV_EVENT_FOCUSED, NULL);

    // "Msg:" label and input
    lv_obj_t *lbl_msg = lv_label_create(screen_compose);
    lv_label_set_text(lbl_msg, "Msg:");
    lv_obj_set_pos(lbl_msg, 10, 80);
    lv_obj_set_style_text_color(lbl_msg, lv_color_hex(0xffffff), 0);

    compose_msg_input = lv_textarea_create(screen_compose);
    lv_obj_set_size(compose_msg_input, SCREEN_WIDTH - 20, 50);
    lv_obj_set_pos(compose_msg_input, 10, 95);
    lv_textarea_set_placeholder_text(compose_msg_input, "Your message...");
    lv_obj_add_event_cb(compose_msg_input, compose_input_focused,
                        LV_EVENT_FOCUSED, NULL);

    // Virtual Keyboard (hidden by default - physical keyboard is primary)
    compose_keyboard = lv_keyboard_create(screen_compose);
    lv_obj_set_size(compose_keyboard, SCREEN_WIDTH, 90);
    lv_obj_align(compose_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(compose_keyboard, compose_to_input);
    lv_obj_add_event_cb(compose_keyboard, compose_keyboard_event, LV_EVENT_ALL,
                        NULL);
    lv_obj_add_flag(compose_keyboard, LV_OBJ_FLAG_HIDDEN); // Hidden by default

    // Toggle keyboard button (bottom right)
    lv_obj_t *btn_kbd = lv_btn_create(screen_compose);
    lv_obj_set_size(btn_kbd, 40, 30);
    lv_obj_align(btn_kbd, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_set_style_bg_color(btn_kbd, lv_color_hex(0x444466), 0);
    lv_obj_add_event_cb(
        btn_kbd,
        [](lv_event_t *e) {
          if (lv_obj_has_flag(compose_keyboard, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_clear_flag(compose_keyboard, LV_OBJ_FLAG_HIDDEN);
          } else {
            lv_obj_add_flag(compose_keyboard, LV_OBJ_FLAG_HIDDEN);
          }
        },
        LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_kbd = lv_label_create(btn_kbd);
    lv_label_set_text(lbl_kbd, LV_SYMBOL_KEYBOARD);
    lv_obj_center(lbl_kbd);

    Serial.println("[LVGL] Compose screen created");
  }

  // Caps Lock and Symbol Lock state for physical keyboard
  static bool capsLockActive = false;
  static bool symbolLockActive = false;
  static uint32_t lastShiftTime = 0;
  static uint32_t lastSymbolTime = 0;
  static const uint32_t DOUBLE_TAP_MS = 400; // Double tap window

  // T-Deck keyboard special key codes (adjust if needed)
  // These may vary depending on firmware - check serial output
  static const char KEY_SHIFT = 0x01;  // Shift key alone
  static const char KEY_SYMBOL = 0x02; // Symbol key alone ($)

  // Symbol mapping for T-Deck keyboard (number row -> symbols)
  static char getSymbolChar(char key) {
    switch (key) {
    case '1':
      return '!';
    case '2':
      return '@';
    case '3':
      return '#';
    case '4':
      return '$';
    case '5':
      return '%';
    case '6':
      return '^';
    case '7':
      return '&';
    case '8':
      return '*';
    case '9':
      return '(';
    case '0':
      return ')';
    case 'q':
      return '#';
    case 'w':
      return '1';
    case 'e':
      return '2';
    case 'r':
      return '3';
    case 't':
      return '(';
    case 'y':
      return ')';
    case 'u':
      return '_';
    case 'i':
      return '-';
    case 'o':
      return '+';
    case 'p':
      return '@';
    case 'a':
      return '*';
    case 's':
      return '4';
    case 'd':
      return '5';
    case 'f':
      return '6';
    case 'g':
      return '/';
    case 'h':
      return ':';
    case 'j':
      return ';';
    case 'k':
      return '\'';
    case 'l':
      return '"';
    case 'z':
      return '?';
    case 'x':
      return '7';
    case 'c':
      return '8';
    case 'v':
      return '9';
    case 'b':
      return '!';
    case 'n':
      return ',';
    case 'm':
      return '.';
    default:
      return key;
    }
  }

  // Handle physical keyboard input for compose screen and contact edit
  void handleComposeKeyboard(char key) {
    // Update activity time for eco mode
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
        Serial.printf(
            "[LVGL] Screen woken up by keyboard, CPU boosted to %d MHz (map)\n",
            getCpuFrequencyMhz());
      } else {
        Serial.println("[LVGL] Screen woken up by keyboard");
      }
    }

    // Determine which input to use
    lv_obj_t *target_input = nullptr;

    if (compose_screen_active && current_focused_input) {
      target_input = current_focused_input;
    } else if (lv_scr_act() == screen_contact_edit && contact_current_input) {
      target_input = contact_current_input;
    }

    if (!target_input)
      return;

    // Debug: print key code
    Serial.printf("[KB] Key: %d (0x%02X) '%c'\n", key, key,
                  (key >= 32 && key < 127) ? key : '?');

    // Check for Shift key double-tap (Caps Lock toggle)
    if (key == KEY_SHIFT) {
      uint32_t now = millis();
      if (now - lastShiftTime < DOUBLE_TAP_MS) {
        capsLockActive = !capsLockActive;
        symbolLockActive = false; // Disable symbol lock when enabling caps
        Serial.printf("[KB] Caps Lock: %s\n", capsLockActive ? "ON" : "OFF");
        // Show popup indicator
        static lv_obj_t *lock_popup = nullptr;
        if (lock_popup && lv_obj_is_valid(lock_popup)) {
          lv_obj_del(lock_popup);
        }
        lock_popup = lv_label_create(lv_scr_act());
        lv_label_set_text(lock_popup, capsLockActive ? "MAJ" : "maj");
        lv_obj_set_style_bg_color(lock_popup,
                                  capsLockActive ? lv_color_hex(0x00aa00)
                                                 : lv_color_hex(0x666666),
                                  0);
        lv_obj_set_style_bg_opa(lock_popup, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(lock_popup, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_pad_all(lock_popup, 8, 0);
        lv_obj_set_style_radius(lock_popup, 5, 0);
        lv_obj_align(lock_popup, LV_ALIGN_TOP_MID, 0, 40);
        // Auto-delete after 1.5 sec
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, lock_popup);
        lv_anim_set_time(&a, 1500);
        lv_anim_set_deleted_cb(&a, [](lv_anim_t *anim) {
          lv_obj_t *obj = (lv_obj_t *)anim->var;
          if (obj && lv_obj_is_valid(obj))
            lv_obj_del(obj);
        });
        lv_anim_start(&a);
      }
      lastShiftTime = now;
      return;
    }

    // Check for Symbol key double-tap (Symbol Lock toggle)
    if (key == KEY_SYMBOL) {
      uint32_t now = millis();
      if (now - lastSymbolTime < DOUBLE_TAP_MS) {
        symbolLockActive = !symbolLockActive;
        capsLockActive = false; // Disable caps lock when enabling symbol
        Serial.printf("[KB] Symbol Lock: %s\n",
                      symbolLockActive ? "ON" : "OFF");
        // Show popup indicator
        static lv_obj_t *sym_popup = nullptr;
        if (sym_popup && lv_obj_is_valid(sym_popup)) {
          lv_obj_del(sym_popup);
        }
        sym_popup = lv_label_create(lv_scr_act());
        lv_label_set_text(sym_popup, symbolLockActive ? "SYM" : "sym");
        lv_obj_set_style_bg_color(sym_popup,
                                  symbolLockActive ? lv_color_hex(0x0088ff)
                                                   : lv_color_hex(0x666666),
                                  0);
        lv_obj_set_style_bg_opa(sym_popup, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(sym_popup, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_pad_all(sym_popup, 8, 0);
        lv_obj_set_style_radius(sym_popup, 5, 0);
        lv_obj_align(sym_popup, LV_ALIGN_TOP_MID, 0, 40);
        // Auto-delete after 1.5 sec
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, sym_popup);
        lv_anim_set_time(&a, 1500);
        lv_anim_set_deleted_cb(&a, [](lv_anim_t *anim) {
          lv_obj_t *obj = (lv_obj_t *)anim->var;
          if (obj && lv_obj_is_valid(obj))
            lv_obj_del(obj);
        });
        lv_anim_start(&a);
      }
      lastSymbolTime = now;
      return;
    }

    if (key == 0x08 || key == 0x7F) { // Backspace
      lv_textarea_del_char(target_input);
    } else if (key == 0x0D || key == 0x0A) { // Enter
      // Handle Enter differently for compose vs contact edit
      if (compose_screen_active) {
        // Compose screen: switch between inputs or send
        if (current_focused_input == compose_to_input) {
          lv_obj_clear_state(compose_to_input, LV_STATE_FOCUSED);
          lv_obj_add_state(compose_msg_input, LV_STATE_FOCUSED);
          current_focused_input = compose_msg_input;
        } else {
          // Send message
          const char *to = lv_textarea_get_text(compose_to_input);
          const char *msg = lv_textarea_get_text(compose_msg_input);
          if (strlen(to) > 0 && strlen(msg) > 0) {
            MSG_Utils::addToOutputBuffer(1, String(to), String(msg));

            // Save sent message to aprsMessages.txt (same format as received
            // messages)
            File msgFile =
                STORAGE_Utils::openFile("/aprsMessages.txt", FILE_APPEND);
            if (msgFile) {
              String sentMsg = String(to) + ",>" + String(msg);
              msgFile.println(sentMsg);
              msgFile.close();
              MSG_Utils::loadNumMessages();
            }

            // Save to conversation file
            MSG_Utils::saveToConversation(String(to), String(msg), true);

            lv_textarea_set_text(compose_to_input, "");
            lv_textarea_set_text(compose_msg_input, "");
            compose_screen_active = false;

            // Return to the screen we came from
            if (compose_return_screen &&
                lv_obj_is_valid(compose_return_screen)) {
              if (compose_return_screen == screen_conversation) {
                refresh_conversation_messages();
              }
              lv_scr_load_anim(compose_return_screen,
                               LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
            } else {
              lv_scr_load_anim(screen_main, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0,
                               false);
            }
          }
        }
      } else if (lv_scr_act() == screen_contact_edit) {
        // Contact edit screen: switch between fields
        if (contact_current_input == contact_callsign_input) {
          lv_obj_clear_state(contact_callsign_input, LV_STATE_FOCUSED);
          lv_obj_add_state(contact_name_input, LV_STATE_FOCUSED);
          contact_current_input = contact_name_input;
        } else if (contact_current_input == contact_name_input) {
          lv_obj_clear_state(contact_name_input, LV_STATE_FOCUSED);
          lv_obj_add_state(contact_comment_input, LV_STATE_FOCUSED);
          contact_current_input = contact_comment_input;
        }
        // On last field, Enter does nothing (user uses Save button)
      }
    } else if (key >= 32 && key < 127) { // Printable chars
      char outputKey = key;

      // Apply Symbol Lock transformation
      if (symbolLockActive && key >= 'a' && key <= 'z') {
        outputKey = getSymbolChar(key);
      }
      // Apply Caps Lock transformation (only for letters)
      else if (capsLockActive && key >= 'a' && key <= 'z') {
        outputKey = key - 32; // Convert to uppercase
      }

      lv_textarea_add_char(target_input, outputKey);
    }
  }

  static void btn_compose_clicked(lv_event_t * e) {
    Serial.printf("[LVGL] Compose button clicked, current_msg_type=%d\n",
                  current_msg_type);
    if (current_msg_type == 2) {
      // Contacts tab - open add contact screen
      Serial.println("[LVGL] Opening contact edit screen");
      show_contact_edit_screen(nullptr);
    } else {
      // Messages tab - open compose message screen
      create_compose_screen();
      compose_screen_active = true;
      compose_return_screen = lv_scr_act(); // Store current screen to return to
      current_focused_input = compose_to_input;
      lv_scr_load_anim(screen_compose, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0,
                       false);
    }
  }

  // Create the messages screen
  static void create_msg_screen() {
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
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    // Compose button
    lv_obj_t *btn_compose = lv_btn_create(title_bar);
    lv_obj_set_size(btn_compose, 40, 25);
    lv_obj_align(btn_compose, LV_ALIGN_RIGHT_MID, -50, 0);
    lv_obj_set_style_bg_color(btn_compose, lv_color_hex(0x00aa55), 0);
    lv_obj_add_event_cb(btn_compose, btn_compose_clicked, LV_EVENT_CLICKED,
                        NULL);
    lv_obj_t *lbl_compose = lv_label_create(btn_compose);
    lv_label_set_text(lbl_compose, LV_SYMBOL_EDIT);
    lv_obj_center(lbl_compose);

    // Delete button
    lv_obj_t *btn_delete = lv_btn_create(title_bar);
    lv_obj_set_size(btn_delete, 40, 25);
    lv_obj_align(btn_delete, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_bg_color(btn_delete, lv_color_hex(0xff4444), 0);
    lv_obj_add_event_cb(btn_delete, btn_delete_msgs_clicked, LV_EVENT_CLICKED,
                        NULL);
    lv_obj_t *lbl_delete = lv_label_create(btn_delete);
    lv_label_set_text(lbl_delete, LV_SYMBOL_TRASH);
    lv_obj_center(lbl_delete);

    // Tabview for APRS / Winlink
    msg_tabview = lv_tabview_create(screen_msg, LV_DIR_TOP, 30);
    if (!msg_tabview) { // Vérifier la création réussie
      Serial.println("[LVGL] ERROR: Failed to create msg_tabview!");
      return; // Éviter le crash
    }
    lv_obj_set_size(msg_tabview, SCREEN_WIDTH, SCREEN_HEIGHT - 35);
    lv_obj_set_pos(msg_tabview, 0, 35);
    lv_obj_set_style_bg_color(msg_tabview, lv_color_hex(0x0f0f23), 0);
    lv_obj_add_event_cb(msg_tabview, msg_tab_changed, LV_EVENT_VALUE_CHANGED,
                        NULL);

    // Get tab bar and set equal width for all tabs
    lv_obj_t *tab_bar = lv_tabview_get_tab_btns(msg_tabview);
    if (tab_bar) { // Vérifier la création réussie
      lv_obj_set_style_pad_column(tab_bar, 2, 0); // Small gap between tabs
    }

    // APRS Tab
    lv_obj_t *tab_aprs = lv_tabview_add_tab(msg_tabview, "APRS");
    if (tab_aprs) { // Vérifier la création réussie
      lv_obj_set_style_bg_color(tab_aprs, lv_color_hex(0x0f0f23), 0);
      lv_obj_set_style_pad_all(tab_aprs, 5, 0);

      list_aprs_global = lv_list_create(tab_aprs);
      if (list_aprs_global) { // Vérifier la création réussie
        lv_obj_set_size(list_aprs_global, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_color(list_aprs_global, lv_color_hex(0x0f0f23), 0);
        lv_obj_set_style_border_width(list_aprs_global, 0, 0);
        populate_msg_list(list_aprs_global, 0);
      }
    }

    // Winlink Tab
    lv_obj_t *tab_wlnk = lv_tabview_add_tab(msg_tabview, "Winlink");
    if (tab_wlnk) { // Vérifier la création réussie
      lv_obj_set_style_bg_color(tab_wlnk, lv_color_hex(0x0f0f23), 0);
      lv_obj_set_style_pad_all(tab_wlnk, 5, 0);

      list_wlnk_global = lv_list_create(tab_wlnk);
      if (list_wlnk_global) { // Vérifier la création réussie
        lv_obj_set_size(list_wlnk_global, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_color(list_wlnk_global, lv_color_hex(0x0f0f23), 0);
        lv_obj_set_style_border_width(list_wlnk_global, 0, 0);
        // Don't populate here - lazy load when tab is selected
      }
    }

    // Contacts Tab
    lv_obj_t *tab_contacts = lv_tabview_add_tab(msg_tabview, "Contacts");
    if (tab_contacts) { // Vérifier la création réussie
      lv_obj_set_style_bg_color(tab_contacts, lv_color_hex(0x0f0f23), 0);
      lv_obj_set_style_pad_all(tab_contacts, 5, 0);

      list_contacts_global = lv_list_create(tab_contacts);
      if (list_contacts_global) { // Vérifier la création réussie
        lv_obj_set_size(list_contacts_global, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_color(list_contacts_global, lv_color_hex(0x0f0f23),
                                  0);
        lv_obj_set_style_border_width(list_contacts_global, 0, 0);
        // Don't populate here - lazy load when tab is selected
      }
    }

    // Frames Tab (raw LoRa frames log)
    lv_obj_t *tab_frames = lv_tabview_add_tab(msg_tabview, "Frames");
    if (tab_frames) { // Vérifier la création réussie
      lv_obj_set_style_bg_color(tab_frames, lv_color_hex(0x0f0f23), 0);
      lv_obj_set_style_pad_all(tab_frames, 5, 0);

      list_frames_global = lv_list_create(tab_frames);
      if (list_frames_global) { // Vérifier la création réussie
        lv_obj_set_size(list_frames_global, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_color(list_frames_global, lv_color_hex(0x0f0f23),
                                  0);
        lv_obj_set_style_border_width(list_frames_global, 0, 0);
        // Don't populate here - lazy load when tab is selected
      }
    }

    // Stats Tab (link statistics)
    lv_obj_t *tab_stats = lv_tabview_add_tab(msg_tabview, "Stats");
    if (tab_stats) { // Vérifier la création réussie
      lv_obj_set_style_bg_color(tab_stats, lv_color_hex(0x0f0f23), 0);
      lv_obj_set_style_pad_all(tab_stats, 10, 0);

      cont_stats_global = lv_obj_create(tab_stats);
      if (cont_stats_global) { // Vérifier la création réussie
        lv_obj_set_size(cont_stats_global, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_color(cont_stats_global, lv_color_hex(0x0f0f23), 0);
        lv_obj_set_style_border_width(cont_stats_global, 0, 0);
        lv_obj_set_flex_flow(cont_stats_global, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(cont_stats_global, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_row(cont_stats_global, 4, 0);
        // Don't populate here - lazy load when tab is selected
      }
    }

    Serial.println("[LVGL] Messages screen created with tabs");
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

    // Create the UI
    create_dashboard();

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
        updateCallsign(last_callsign.c_str());
        // Update APRS symbol when beacon changes (overlay + symbol)
        String fullSymbol = currentBeacon->overlay + currentBeacon->symbol;
        drawAPRSSymbol(fullSymbol.c_str());
      }

      // Update GPS data
      if (gps.location.isValid()) {
        updateGPS(gps.location.lat(), gps.location.lng(), gps.altitude.meters(),
                  gps.speed.kmph(), gps.satellites.value(), gps.hdop.hdop());
      }

      // Update date/time from GPS
      if (gps.time.isValid() && gps.date.isValid()) {
        updateTime(gps.date.day(), gps.date.month(), gps.date.year(),
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
        updateBattery(percent, voltage);
      }

      // Update WiFi status
      updateWiFi(WiFiConnected, WiFiConnected ? WiFi.RSSI() : 0);

      // Update Bluetooth status
      if (label_bluetooth) {
        if (!bluetoothActive) {
          lv_label_set_text(label_bluetooth, "BT: Disabled");
          lv_obj_set_style_text_color(label_bluetooth, lv_color_hex(0x666666),
                                      0);
        } else if (BLE_Utils::isSleeping()) {
          lv_label_set_text(label_bluetooth, "BT: Eco Sleep");
          lv_obj_set_style_text_color(label_bluetooth, lv_color_hex(0x666666),
                                      0);
        } else if (bluetoothConnected) {
          String addr = BLE_Utils::getConnectedDeviceAddress();
          if (addr.length() > 0) {
            String btText = "BT: > " + addr;
            lv_label_set_text(label_bluetooth, btText.c_str());
          } else {
            lv_label_set_text(label_bluetooth, "BT: Connected");
          }
          lv_obj_set_style_text_color(label_bluetooth, lv_color_hex(0xc792ea),
                                      0);
        } else {
          lv_label_set_text(label_bluetooth, "BT: Waiting...");
          lv_obj_set_style_text_color(label_bluetooth, lv_color_hex(0xffa500),
                                      0);
        }
      }

      // Update LoRa (last received packet)
      if (lastReceivedPacket.sender.length() > 0) {
        updateLoRa(lastReceivedPacket.sender.c_str(), lastReceivedPacket.rssi);
      }
      
     // Update Stats tab if currently active (Index 4)
    if (screen_msg && lv_scr_act() == screen_msg && msg_tabview) {
        if (lv_tabview_get_tab_act(msg_tabview) == 4 && cont_stats_global) {
            populate_stats(cont_stats_global);
        }
      }
    }
  }

  void updateGPS(double lat, double lng, double alt, double speed, int sats,
                 double hdop) {
    if (label_gps) {
      char buf[128];
      const char *locator = Utils::getMaidenheadLocator(lat, lng, 8);

      // Determine HDOP quality indicator (same as original CA2RXU code)
      const char *hdopState = "";
      if (hdop > 5.0) {
        hdopState = "X"; // Bad precision
      } else if (hdop > 2.0 && hdop < 5.0) {
        hdopState = "-"; // Medium precision
      } else if (hdop <= 2.0) {
        hdopState = "+"; // Good precision
      }

      snprintf(buf, sizeof(buf),
               "GPS: %d%s sat  Loc: %s\nLat: %.4f  Lon: %.4f\nAlt: %.0f m  "
               "Spd: %.0f km/h",
               sats, hdopState, locator, lat, lng, alt, speed);
      lv_label_set_text(label_gps, buf);
    }
  }

  void updateBattery(int percent, float voltage) {
    if (label_battery) {
      char buf[32];
      snprintf(buf, sizeof(buf), "Bat: %.2f V (%d%%)", voltage, percent);
      lv_label_set_text(label_battery, buf);

      // Change color based on level (red/coral base, green when good)
      if (percent > 50) {
        lv_obj_set_style_text_color(label_battery, lv_color_hex(0x006600),
                                    0); // Green
      } else if (percent > 20) {
        lv_obj_set_style_text_color(label_battery, lv_color_hex(0xffa500),
                                    0); // Orange
      } else {
        lv_obj_set_style_text_color(label_battery, lv_color_hex(0xff6b6b),
                                    0); // Red/coral
      }
    }
  }

  void updateLoRa(const char *lastRx, int rssi) {
    if (label_lora) {
      char buf[128];
      float freq = Config.loraTypes[loraIndex].frequency / 1000000.0;
      int rate = Config.loraTypes[loraIndex].dataRate;
      snprintf(buf, sizeof(buf), "LoRa: %.3f MHz  %d bps\nLast RX: %s (%ddBm)",
               freq, rate, lastRx, rssi);
      lv_label_set_text(label_lora, buf);
    }
  }

  void refreshLoRaInfo() {
    if (label_lora) {
      char buf[128];
      float freq = Config.loraTypes[loraIndex].frequency / 1000000.0;
      int rate = Config.loraTypes[loraIndex].dataRate;
      snprintf(buf, sizeof(buf), "LoRa: %.3f MHz  %d bps\nLast RX: ---", freq,
               rate);
      lv_label_set_text(label_lora, buf);
    }
  }

  void updateWiFi(bool connected, int rssi) {
    if (label_wifi) {
      if (WiFiUserDisabled) {
        lv_label_set_text(label_wifi, "WiFi: Disabled");
        lv_obj_set_style_text_color(label_wifi, lv_color_hex(0xff6b6b),
                                    0); // Red
      } else if (connected) {
        char buf[48];
        String ip = WiFi.localIP().toString();
        snprintf(buf, sizeof(buf), "WiFi: %s (%d dBm)", ip.c_str(), rssi);
        lv_label_set_text(label_wifi, buf);
        lv_obj_set_style_text_color(label_wifi, lv_color_hex(0x759a9e), 0);
      } else if (WiFiEcoMode) {
        lv_label_set_text(label_wifi, "WiFi: Eco (sleep)");
        lv_obj_set_style_text_color(label_wifi, lv_color_hex(0xffa500),
                                    0); // Orange
      } else {
        lv_label_set_text(label_wifi, "WiFi: ---");
        lv_obj_set_style_text_color(label_wifi, lv_color_hex(0x759a9e), 0);
      }
    }
  }

  // RX Message popup - delegated to UIPopups module
  void showMessage(const char *from, const char *message) {
    UIPopups::showMessage(from, message);
  }

  void updateCallsign(const char *callsign) {
    if (label_callsign) {
      lv_label_set_text(label_callsign, callsign);
    }
  }

  void updateTime(int day, int month, int year, int hour, int minute,
                  int second) {
    if (label_time) {
      char buf[32];
      snprintf(buf, sizeof(buf), "%02d/%02d/%04d %02d:%02d:%02d UTC", day,
               month, year, hour, minute, second);
      lv_label_set_text(label_time, buf);
    }
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

  void handleComposeKeyboard(char key) { ::handleComposeKeyboard(key); }

  void showBootWebConfig() {
    UISettings::showBootWebConfig();
  }

  // Return to main dashboard screen
  void return_to_dashboard() {
    if (screen_main) {
      lv_scr_load_anim(screen_main, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
    }
  }

  // Refresh frames list if visible (called when new frame received)
  void refreshFramesList() {
    // Only refresh if on messages screen and Frames tab is active
    if (lv_scr_act() == screen_msg && current_msg_type == 3 &&
        list_frames_global) {
      populate_frames_list(list_frames_global);
      // Scroll to top to show newest frame
      lv_obj_scroll_to_y(list_frames_global, 0, LV_ANIM_ON);
    }
  }

  }

#endif // USE_LVGL_UI
