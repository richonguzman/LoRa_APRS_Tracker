/* trace_sd.cpp — Binary trace persistence on SD card */

#ifdef USE_LVGL_UI

#include "trace_sd.h"
#include <SD.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <NMEAGPS.h>

static const char* TAG = "TraceSD";

extern SemaphoreHandle_t spiMutex;
extern gps_fix gpsFix;

static char currentFilePath[64] = "";
static bool initialized = false;

// Build today's filename from GPS date
static void updateFilePath() {
    if (gpsFix.valid.date) {
        snprintf(currentFilePath, sizeof(currentFilePath),
                 "/LoRa_Tracker/trace/trace_%04d%02d%02d.bin",
                 2000 + gpsFix.dateTime.year, gpsFix.dateTime.month, gpsFix.dateTime.date);
    } else {
        // Fallback: use a generic name until GPS date is available
        strncpy(currentFilePath, "/LoRa_Tracker/trace/trace_nodate.bin", sizeof(currentFilePath));
    }
}

namespace TraceSD {

    void init() {
        if (spiMutex == NULL || xSemaphoreTakeRecursive(spiMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            return;
        }
        if (!SD.exists("/LoRa_Tracker/trace")) {
            SD.mkdir("/LoRa_Tracker/trace");
        }
        xSemaphoreGiveRecursive(spiMutex);

        updateFilePath();
        initialized = true;
        ESP_LOGI(TAG, "Trace SD initialized: %s", currentFilePath);
    }

    void appendPoint(float lat, float lon, uint32_t time_ms) {
        if (!initialized) return;

        // Update filename if GPS date became available or day changed
        updateFilePath();

        if (spiMutex == NULL || xSemaphoreTakeRecursive(spiMutex, pdMS_TO_TICKS(200)) != pdTRUE) {
            return;  // Skip this point rather than block rendering
        }

        File file = SD.open(currentFilePath, FILE_APPEND);
        if (file) {
            TraceRecord rec = { lat, lon, time_ms };
            file.write((const uint8_t*)&rec, sizeof(rec));
            file.close();
        }
        xSemaphoreGiveRecursive(spiMutex);
    }

    int readViewport(float minLat, float maxLat, float minLon, float maxLon,
                     TraceRecord* outBuf, int maxPoints) {
        if (!initialized || maxPoints <= 0) return 0;

        updateFilePath();

        if (spiMutex == NULL || xSemaphoreTakeRecursive(spiMutex, pdMS_TO_TICKS(500)) != pdTRUE) {
            return 0;
        }

        File file = SD.open(currentFilePath, FILE_READ);
        if (!file) {
            xSemaphoreGiveRecursive(spiMutex);
            return 0;
        }

        int count = 0;
        TraceRecord rec;
        while (file.available() >= (int)sizeof(rec) && count < maxPoints) {
            if (file.read((uint8_t*)&rec, sizeof(rec)) != sizeof(rec)) break;

            // Bounding box filter
            if (rec.lat >= minLat && rec.lat <= maxLat &&
                rec.lon >= minLon && rec.lon <= maxLon) {
                outBuf[count++] = rec;
            }
        }

        file.close();
        xSemaphoreGiveRecursive(spiMutex);

        return count;
    }

    int getTodayPointCount() {
        if (!initialized) return 0;

        updateFilePath();

        if (spiMutex == NULL || xSemaphoreTakeRecursive(spiMutex, pdMS_TO_TICKS(200)) != pdTRUE) {
            return 0;
        }

        File file = SD.open(currentFilePath, FILE_READ);
        int count = 0;
        if (file) {
            count = file.size() / sizeof(TraceRecord);
            file.close();
        }
        xSemaphoreGiveRecursive(spiMutex);
        return count;
    }

}  // namespace TraceSD

#endif // USE_LVGL_UI
