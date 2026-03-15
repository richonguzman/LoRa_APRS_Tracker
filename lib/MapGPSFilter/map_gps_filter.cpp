// map_gps_filter.cpp
#include "map_gps_filter.h"
#ifdef UNIT_TEST
#include "mock_arduino.h"  // For native tests
#define MILLIS() MockArduino::millis()
#include <cstdio>  // For printf in mock logs
#define ESP_LOGW(TAG, fmt, ...) printf("WARN: " fmt "\n", ##__VA_ARGS__)
#define ESP_LOGD(TAG, fmt, ...) printf("DEBUG: " fmt "\n", ##__VA_ARGS__)
#else
#include <esp_log.h> // Keep standard log for firmware
#define MILLIS() millis()
#endif
#include <cmath>

static const char* TAG = "MapGPSFilter";

MapGPSFilter::MapGPSFilter() {
    reset();
}

void MapGPSFilter::reset() {
    iconGpsLat = 0.0f;
    iconGpsLon = 0.0f;
    iconGpsValid = false;
    filteredOwnLat = 0.0f;
    filteredOwnLon = 0.0f;
    filteredOwnValid = false;
    iconCentroidLat = 0.0f;
    iconCentroidLon = 0.0f;
    iconCentroidCount = 0;
    lastValidTime = 0;
    ownTraceCount = 0;
    ownTraceHead = 0;
    memset(ownTrace, 0, sizeof(ownTrace));
}

void MapGPSFilter::updateFilteredOwnPosition(TinyGPSPlus& gps) {
    ESP_LOGD(TAG, "Update called: location.isValid=%d, sats.value=%d, hdop.value=%.1f", gps.location.isValid(), gps.satellites.value(), gps.hdop.hdop());
    // Basic sanity check: need a valid location and a realistic number of satellites.
    if (!gps.location.isValid() || !gps.satellites.isValid() || gps.satellites.value() < 3 || gps.satellites.value() > 90) {
        return;
    }

    float lat = gps.location.lat();
    float lon = gps.location.lng();
    int sats = gps.satellites.value();

    // Level 1: icon display (≥3 sats = 2D fix minimum)
    if (sats >= 3) {
        iconGpsLat = lat;
        iconGpsLon = lon;
        iconGpsValid = true;
    }

    // Level 2: filtered position for trace + recentering (≥6 sats)
    if (sats < 6) return;

    uint32_t now = MILLIS();

    // First valid filtered position
    if (!filteredOwnValid) {
        filteredOwnLat = lat;
        filteredOwnLon = lon;
        filteredOwnValid = true;
        iconCentroidLat = lat;
        iconCentroidLon = lon;
        iconCentroidCount = 1;
        lastValidTime = now;
        return;
    }

    // 1. JUMP FILTER (Supersonic Spike Rejection > 150 km/h)
    double distMeters = TinyGPSPlus::distanceBetween(filteredOwnLat, filteredOwnLon, lat, lon);
    double dtSeconds = (now - lastValidTime) / 1000.0;
    if (dtSeconds > 0.0 && dtSeconds < 120.0) { 
        double speedKmph = (distMeters / dtSeconds) * 3.6;
        if (speedKmph > MAX_SPEED_KMPH) {
            ESP_LOGW(TAG, "GPS Jump: %.1fm in %.1fs (%.1f km/h)", distMeters, dtSeconds, speedKmph);
            return; // Reject aberrant jump
        }
    }
    lastValidTime = now;

    // 2. JITTER FILTER (Doppler Speed Stop < 1.5 km/h)
    if (gps.speed.isValid() && gps.speed.kmph() < MIN_SPEED_KMPH) return;

    // Update running centroid with every GPS reading
    float alpha = (iconCentroidCount < 10) ? 1.0f / (iconCentroidCount + 1) : 0.1f;
    iconCentroidLat += alpha * (lat - iconCentroidLat);
    iconCentroidLon += alpha * (lon - iconCentroidLon);
    iconCentroidCount++;

    // Threshold: 15m min, +5m per HDOP unit
    float hdop = gps.hdop.isValid() ? gps.hdop.hdop() : 2.0f;
    float thresholdM = fmax(MIN_THRESHOLD_M, hdop * HDOP_FACTOR);
    float thresholdLat = thresholdM / 111320.0f;
    float thresholdLon = thresholdM / (111320.0f * cosf(lat * M_PI / 180.0f));

    // Compare the *current smoothed centroid* against the *last published filteredOwn position*.
    // Only update filteredOwn if the centroid has moved significantly beyond the threshold.
    // This ensures filteredOwn is stable for trace/centering, and only "snaps" when truly needed.
    if (fabs(iconCentroidLat - filteredOwnLat) > thresholdLat || 
        fabs(iconCentroidLon - filteredOwnLon) > thresholdLon) {
        ESP_LOGD(TAG, "Filtered GPS snapped: Centroid (%.6f,%.6f) -> Filtered (%.6f,%.6f) - DeltaLat:%.6f, DeltaLon:%.6f",
                      iconCentroidLat, iconCentroidLon, filteredOwnLat, filteredOwnLon,
                      fabs(iconCentroidLat - filteredOwnLat), fabs(iconCentroidLon - filteredOwnLon));
        filteredOwnLat = iconCentroidLat; // filteredOwn "snaps" to the current smoothed centroid
        filteredOwnLon = iconCentroidLon;
        // IMPORTANT: Do NOT reset iconCentroidLat/Lon here. It's a continuous average.
        // Resetting it would re-introduce the jumping.
    }
    // If the centroid is within threshold, filteredOwn remains unchanged, ensuring stability.
}

void MapGPSFilter::addOwnTracePoint() {
    if (!filteredOwnValid) return; // No valid smoothed position yet

    float lat = filteredOwnLat;
    float lon = filteredOwnLon;

    // Ensure we only add a new point if we moved enough from the last trace point.
    // This prevents the buffer from filling up with identical points during standing updates.
    if (ownTraceCount > 0) {
        int lastIdx = (ownTraceHead - 1 + TRACE_MAX_POINTS) % TRACE_MAX_POINTS;
        float lastLat = ownTrace[lastIdx].lat;
        float lastLon = ownTrace[lastIdx].lon;
        
        // Threshold: 0.0001 degrees is roughly 11 meters
        if (fabs(lat - lastLat) < TRACE_MIN_DISTANCE && fabs(lon - lastLon) < TRACE_MIN_DISTANCE) {
            return; // Hasn't moved enough from the last recorded trace point
        }
    }

    // Add point to circular buffer
    ownTrace[ownTraceHead].lat = lat;
    ownTrace[ownTraceHead].lon = lon;

    // Update circular buffer indices
    ownTraceHead = (ownTraceHead + 1) % TRACE_MAX_POINTS;
    if (ownTraceCount < TRACE_MAX_POINTS) {
        ownTraceCount++;
    }
}

bool MapGPSFilter::getUiPosition(float* lat, float* lon) const {
    if (filteredOwnValid) {
        *lat = filteredOwnLat;
        *lon = filteredOwnLon;
        return true;
    } else if (iconGpsValid) {
        *lat = iconGpsLat;
        *lon = iconGpsLon;
        return true;
    }
    return false;
}

void MapGPSFilter::clearTrace() {
    ownTraceCount = 0;
    ownTraceHead = 0;
    memset(ownTrace, 0, sizeof(ownTrace));
}

// For unit tests only
const TracePoint& MapGPSFilter::getOwnTracePoint(int index) const {
    int bufferIndex = (ownTraceHead - ownTraceCount + index + TRACE_MAX_POINTS) % TRACE_MAX_POINTS;
    return ownTrace[bufferIndex];
}