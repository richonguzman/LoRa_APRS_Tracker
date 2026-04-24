/* trace_sd.h — Binary trace persistence on SD card
 * Format: fixed 12-byte records (float lat, float lon, uint32_t time_ms)
 * Session-only file: /LoRa_Tracker/trace/trace_session.bin (wiped at each boot)
 */

#ifndef TRACE_SD_H
#define TRACE_SD_H

#ifdef USE_LVGL_UI

#include <cstdint>

// On-disk record — packed, no padding (3 × 4 bytes = 12 bytes)
#pragma pack(push, 1)
struct TraceRecord {
    float    lat;
    float    lon;
    uint32_t time_ms;  // millis() timestamp
};
#pragma pack(pop)

static_assert(sizeof(TraceRecord) == 12, "TraceRecord must be 12 bytes");

namespace TraceSD {

    // Initialize (call after SD mounted). Creates directory if needed.
    void init();

    // Append one point to today's trace file
    void appendPoint(float lat, float lon, uint32_t time_ms);

    // Read points from SD that fall within a lat/lon bounding box.
    // Fills outBuf (caller-provided), returns number of points loaded.
    // Optionally fills outIndices with the original cache index for continuity checking.
    int readViewport(float minLat, float maxLat, float minLon, float maxLon,
                     TraceRecord* outBuf, int maxPoints, int* outIndices = nullptr);

    // Get total number of records in today's file
    int getTodayPointCount();

}  // namespace TraceSD

#endif // USE_LVGL_UI
#endif // TRACE_SD_H
