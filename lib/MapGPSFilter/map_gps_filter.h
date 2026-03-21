// map_gps_filter.h
#ifndef MAP_GPS_FILTER_H
#define MAP_GPS_FILTER_H

#include <cstdint>
#ifdef UNIT_TEST
#include "mock_gpsfix.h"
#else
#include <GPSfix.h>
#endif
#include "gps_math.h"
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
#define pdMS_TO_TICKS(x) (x)
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
    void updateFilteredOwnPosition(const gps_fix& fix);

    // Add current position to trace history (distance-based recording)
    void addOwnTracePoint();

    // Get best available position for UI (Single Source of Truth)
    bool getUiPosition(double* lat, double* lon) const;

    // Own Position getters (The single source of truth)
    double getOwnLat() const { return ownPositionLat; }
    double getOwnLon() const { return ownPositionLon; }
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

    static constexpr int OWN_TRACE_MAX_POINTS = 500;

private:
    // Compact trace using Douglas-Peucker when buffer is full
    void compactTrace();

    // Mutex for cross-core access (defense in depth)
    mutable SemaphoreHandle_t _mutex;

    float lastDeltaMeters; // Distance in meters between raw GPS and filtered position
    float lastAlpha;       // Alpha value used for smoothing

    // Own filtered position — double precision to avoid catastrophic cancellation
    double ownPositionLat;
    double ownPositionLon;
    bool ownPositionValid;
    uint32_t lastValidTime;    // For speed calculation

    // Duplicate detection: last raw GPS position processed
    double lastRawLat;
    double lastRawLon;

    // Own GPS trace (circular buffer)
    TracePoint ownTrace[OWN_TRACE_MAX_POINTS];
    uint16_t ownTraceCount;
    uint16_t ownTraceHead;

    // Constants
    static constexpr double MAX_SPEED_KMPH = 150.0;  // Spike rejection
    static constexpr double MIN_SPEED_KMPH = 3.0;    // Jitter filter (walking Doppler noise)
    static constexpr float TRACE_MIN_DISTANCE = 0.000027f; // ~3 meters
};

#endif // MAP_GPS_FILTER_H
