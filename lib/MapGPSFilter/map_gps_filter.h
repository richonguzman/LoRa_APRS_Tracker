// map_gps_filter.h
#ifndef MAP_GPS_FILTER_H
#define MAP_GPS_FILTER_H

#include <cstdint>
#include "TinyGPS++.h"
#include "station_utils.h" // For TRACE_MAX_POINTS and TracePoint

#ifdef UNIT_TEST
// Mock FreeRTOS for native tests
typedef void* SemaphoreHandle_t;
#define xSemaphoreCreateMutex() nullptr
#define vSemaphoreDelete(x)
#define xSemaphoreTake(x, y) true
#define xSemaphoreGive(x)
#define portMAX_DELAY 0
#define pdTRUE true
#else
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

/**
 * MapGPSFilter - KISS module for GPS filtering and trace management
 * 
 * Encapsulates GPS position filtering with two quality levels:
 * - Icon GPS: ≥3 satellites (basic 2D fix for display)
 * - Filtered GPS: ≥6 satellites (good 3D fix for trace/centering)
 * 
 * Features:
 * - Supersonic jump rejection (>150 km/h spikes)
 * - Doppler-based jitter filter (<1.5 km/h stationary)
 * - Centroid-based smoothing with HDOP-adaptive thresholds
 * - Circular buffer trace history with distance-based recording
 */
class MapGPSFilter {
public:
    MapGPSFilter();
    void reset(); // Used by unit tests

    // Update filtered position from GPS data (includes all filtering logic)
    void updateFilteredOwnPosition(TinyGPSPlus& gps);
    
    // Add current position to trace history (distance-based recording)
    void addOwnTracePoint();
    
    // Get best available position for UI (filtered if available, icon otherwise)
    bool getUiPosition(float* lat, float* lon) const;
    
    // Icon GPS getters (≥3 sats)
    float getIconGpsLat() const { return iconGpsLat; }
    float getIconGpsLon() const { return iconGpsLon; }
    bool isIconGpsValid() const { return iconGpsValid; }
    
    // Filtered GPS getters (≥6 sats)
    float getFilteredOwnLat() const { return filteredOwnLat; }
    float getFilteredOwnLon() const { return filteredOwnLon; }
    bool isFilteredOwnValid() const { return filteredOwnValid; }
    
    // Trace history access
    const TracePoint* getOwnTrace() const { return ownTrace; }
    int getOwnTraceCount() const { return ownTraceCount; }
    int getOwnTraceHead() const { return ownTraceHead; }
    const TracePoint& getOwnTracePoint(int index) const; // For unit tests

    // Clear all trace points (for reset/clear functionality)
    void clearTrace();

private:
    // Icon GPS position (≥3 satellites, basic 2D fix)
    float iconGpsLat;
    float iconGpsLon;
    bool iconGpsValid;
    
    // Filtered position (≥6 satellites, good 3D fix)
    float filteredOwnLat;
    float filteredOwnLon;
    bool filteredOwnValid;
    
    // Centroid for smoothing (running average)
    float iconCentroidLat;
    float iconCentroidLon;
    uint32_t iconCentroidCount;
    uint32_t lastValidTime; // For speed calculation
    
    // Own GPS trace (circular buffer)
    TracePoint ownTrace[TRACE_MAX_POINTS];
    uint8_t ownTraceCount;
    uint8_t ownTraceHead;
    
    // Constants (matching original thresholds)
    static constexpr float MIN_THRESHOLD_M = 15.0f;
    static constexpr float HDOP_FACTOR = 5.0f;
    static constexpr double MAX_SPEED_KMPH = 150.0;  // Supersonic rejection
    static constexpr double MIN_SPEED_KMPH = 1.5;    // Jitter filter
    static constexpr float TRACE_MIN_DISTANCE = 0.000027f; // ~3 meters
};

#endif // MAP_GPS_FILTER_H