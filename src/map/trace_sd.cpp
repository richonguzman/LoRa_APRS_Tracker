/* trace_sd.cpp — Binary trace persistence on SD card
 * Session-only: file is wiped at boot, rebuilt during the trip.
 * Fixed filename — no date dependency, no midnight rollover issues.
 * PSRAM cache mirrors the file; readViewport() scans the cache (microseconds, no SD).
 */

#ifdef USE_LVGL_UI

#include "trace_sd.h"
#include <SD.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static const char* TAG = "TraceSD";

extern SemaphoreHandle_t spiMutex;

static constexpr const char* TRACE_DIR  = "/LoRa_Tracker/trace";
static constexpr const char* TRACE_PATH = "/LoRa_Tracker/trace/trace_session.bin";

static bool initialized = false;

// PSRAM cache — dynamically grown
static TraceRecord* cache = nullptr;
static int cacheCount    = 0;
static int cacheCapacity = 0;
static constexpr int CACHE_GROW_STEP = 512;  // Grow by 512 records (6 KB) at a time
static constexpr int CACHE_MAX       = 32768; // ~384 KB max — well within PSRAM

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

// Clear session file and reset PSRAM cache
static void clearTraceSD() {
    if (spiMutex == NULL || xSemaphoreTakeRecursive(spiMutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        return;
    }

    if (SD.exists(TRACE_PATH)) {
        SD.remove(TRACE_PATH);
        ESP_LOGI(TAG, "Deleted previous session trace: %s", TRACE_PATH);
    }

    File file = SD.open(TRACE_PATH, FILE_WRITE);
    if (file) {
        file.close();
    }

    xSemaphoreGiveRecursive(spiMutex);

    cacheCount = 0;
    if (cache) {
        heap_caps_free(cache);
        cache = nullptr;
    }
    cacheCapacity = 0;
}

namespace TraceSD {

    void init() {
        if (initialized) return;

        if (spiMutex == NULL || xSemaphoreTakeRecursive(spiMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            return;
        }
        if (!SD.exists(TRACE_DIR)) {
            SD.mkdir(TRACE_DIR);
        }
        xSemaphoreGiveRecursive(spiMutex);

        // Fresh session: clear leftover file from previous boot.
        clearTraceSD();

        initialized = true;
        ESP_LOGI(TAG, "Trace SD initialized: %s", TRACE_PATH);
    }

    void appendPoint(float lat, float lon, uint32_t time_ms) {
        if (!initialized) return;

        TraceRecord rec = { lat, lon, time_ms };

        // 1. Append to PSRAM cache (instant)
        if (ensureCacheSpace()) {
            cache[cacheCount++] = rec;
        }

        // 2. Append to SD (persistent)
        if (spiMutex == NULL || xSemaphoreTakeRecursive(spiMutex, pdMS_TO_TICKS(200)) != pdTRUE) {
            return;
        }
        File file = SD.open(TRACE_PATH, FILE_APPEND);
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
