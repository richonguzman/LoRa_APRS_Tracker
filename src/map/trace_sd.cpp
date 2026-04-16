/* trace_sd.cpp — Binary trace persistence on SD card
 * PSRAM cache: SD is read once at init, then only appended to.
 * readViewport() scans the PSRAM cache (microseconds, no SD access).
 */

#ifdef USE_LVGL_UI

#include "trace_sd.h"
#include <SD.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <NMEAGPS.h>

static const char* TAG = "TraceSD";

extern SemaphoreHandle_t spiMutex;
extern gps_fix gpsFix;

static char currentFilePath[64] = "";
static bool initialized = false;

// PSRAM cache — dynamically grown
static TraceRecord* cache = nullptr;
static int cacheCount    = 0;
static int cacheCapacity = 0;
static constexpr int CACHE_GROW_STEP = 512;  // Grow by 512 records (6 KB) at a time
static constexpr int CACHE_MAX       = 32768; // ~384 KB max — well within PSRAM

// Build today's filename from GPS date
static void updateFilePath() {
    if (gpsFix.valid.date) {
        snprintf(currentFilePath, sizeof(currentFilePath),
                 "/LoRa_Tracker/trace/trace_%04d%02d%02d.bin",
                 2000 + gpsFix.dateTime.year, gpsFix.dateTime.month, gpsFix.dateTime.date);
    } else {
        strncpy(currentFilePath, "/LoRa_Tracker/trace/trace_nodate.bin", sizeof(currentFilePath));
    }
}

// Grow cache in PSRAM if needed. Returns true if space is available.
static bool ensureCacheSpace() {
    if (cacheCount < cacheCapacity) return true;
    if (cacheCount >= CACHE_MAX) return false;

    int newCap = cacheCapacity + CACHE_GROW_STEP;
    if (newCap > CACHE_MAX) newCap = CACHE_MAX;

    TraceRecord* newBuf = (TraceRecord*)heap_caps_realloc(cache, newCap * sizeof(TraceRecord), MALLOC_CAP_SPIRAM);
    if (!newBuf) {
        ESP_LOGW(TAG, "PSRAM realloc failed at %d records", newCap);
        return false;
    }
    cache = newBuf;
    cacheCapacity = newCap;
    return true;
}

// Load existing SD file into PSRAM cache
static void loadFromSD() {
    if (spiMutex == NULL || xSemaphoreTakeRecursive(spiMutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        return;
    }

    File file = SD.open(currentFilePath, FILE_READ);
    if (!file) {
        xSemaphoreGiveRecursive(spiMutex);
        ESP_LOGI(TAG, "No existing trace file, starting fresh");
        return;
    }

    int fileRecords = file.size() / sizeof(TraceRecord);
    ESP_LOGI(TAG, "Loading %d records from SD (%d bytes)", fileRecords, (int)file.size());

    // Read in chunks to avoid huge stack buffers
    TraceRecord rec;
    while (file.available() >= (int)sizeof(rec)) {
        if (file.read((uint8_t*)&rec, sizeof(rec)) != sizeof(rec)) break;
        if (!ensureCacheSpace()) {
            ESP_LOGW(TAG, "Cache full at %d records, truncating SD load", cacheCount);
            break;
        }
        cache[cacheCount++] = rec;
    }

    file.close();
    xSemaphoreGiveRecursive(spiMutex);

    ESP_LOGI(TAG, "Loaded %d records into PSRAM cache (%d KB)",
             cacheCount, (int)(cacheCount * sizeof(TraceRecord) / 1024));
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

        // Reset cache
        cacheCount = 0;

        // Load existing trace from SD into PSRAM
        loadFromSD();

        initialized = true;
        ESP_LOGI(TAG, "Trace SD initialized: %s (%d cached)", currentFilePath, cacheCount);
    }

    void appendPoint(float lat, float lon, uint32_t time_ms) {
        if (!initialized) return;

        updateFilePath();

        TraceRecord rec = { lat, lon, time_ms };

        // 1. Append to PSRAM cache (instant)
        if (ensureCacheSpace()) {
            cache[cacheCount++] = rec;
        }

        // 2. Append to SD (persistent)
        if (spiMutex == NULL || xSemaphoreTakeRecursive(spiMutex, pdMS_TO_TICKS(200)) != pdTRUE) {
            return;
        }
        File file = SD.open(currentFilePath, FILE_APPEND);
        if (file) {
            file.write((const uint8_t*)&rec, sizeof(rec));
            file.close();
        }
        xSemaphoreGiveRecursive(spiMutex);
    }

    int readViewport(float minLat, float maxLat, float minLon, float maxLon,
                     TraceRecord* outBuf, int maxPoints, int* outIndices) {
        if (!initialized || !cache || maxPoints <= 0) return 0;

        // Scan PSRAM cache — no SD access, no mutex
        int count = 0;
        for (int i = 0; i < cacheCount && count < maxPoints; ++i) {
            if (cache[i].lat >= minLat && cache[i].lat <= maxLat &&
                cache[i].lon >= minLon && cache[i].lon <= maxLon) {
                if (outIndices) outIndices[count] = i;
                outBuf[count++] = cache[i];
            }
        }
        return count;
    }

    int getTodayPointCount() {
        return cacheCount;
    }

}  // namespace TraceSD

#endif // USE_LVGL_UI
