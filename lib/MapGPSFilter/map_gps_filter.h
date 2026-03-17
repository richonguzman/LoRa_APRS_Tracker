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
 * Encapsulates GPS position filtering with a single, stable source of truth.
 * 
 * Features:
 * - Supersonic jump rejection (>150 km/h spikes)
 * - Doppler-based jitter filter (<1.5 km/h stationary)
 * - Smooth interpolation for fluid movement
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
    
    // Get best available position for UI (Single Source of Truth)
    bool getUiPosition(float* lat, float* lon) const;

    // Own Position getters (The single source of truth)
    float getOwnLat() const { return ownPositionLat; }
    float getOwnLon() const { return ownPositionLon; }
    bool isOwnPositionValid() const { return ownPositionValid; }
    
    // Trace history access
    const TracePoint* getOwnTrace() const { return ownTrace; }
    int getOwnTraceCount() const { return ownTraceCount; }
    int getOwnTraceHead() const { return ownTraceHead; }
    const TracePoint& getOwnTracePoint(int index) const; // For unit tests

    // Clear all trace points (for reset/clear functionality)
    void clearTrace();

    // Diagnostic getters (updated each call to updateFilteredOwnPosition)
    float getLastDeltaMeters() const { return lastDeltaMeters; }
    float getLastAlpha()       const { return lastAlpha; }

    private:
        float lastDeltaMeters; // Distance in meters between raw GPS and filtered position
        float lastAlpha;       // Alpha value used for smoothing
        // Own filtered position (The single source of truth for UI, trace, and recentering)
        float ownPositionLat;
        float ownPositionLon;
        bool ownPositionValid;
        uint32_t lastValidTime; // For speed calculation

        // Own GPS trace (circular buffer)
        TracePoint ownTrace[TRACE_MAX_POINTS];
        uint8_t ownTraceCount;
        uint8_t ownTraceHead;

        // Constants (matching original thresholds)
        static constexpr double MAX_SPEED_KMPH = 150.0;  // Supersonic rejection
        static constexpr double MIN_SPEED_KMPH = 1.5;    // Jitter filter
        static constexpr float TRACE_MIN_DISTANCE = 0.000027f; // ~3 meters
};

#endif // MAP_GPS_FILTER_H