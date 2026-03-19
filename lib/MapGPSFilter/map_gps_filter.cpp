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
#include "sd_logger.h"

static const char* TAG = "MapGPSFilter";

MapGPSFilter::MapGPSFilter() {
    reset();
}

void MapGPSFilter::reset() {
    ownPositionLat = 0.0f;
    ownPositionLon = 0.0f;
    ownPositionValid = false;
    lastValidTime = 0;
    ownTraceCount = 0;
    ownTraceHead = 0;
    memset(ownTrace, 0, sizeof(ownTrace));
    lastDeltaMeters = 0.0f;
    lastAlpha = 0.0f;
}

void MapGPSFilter::updateFilteredOwnPosition(TinyGPSPlus& gps) {
    ESP_LOGD(TAG, "Update called: location.isValid=%d, sats.value=%d, hdop.value=%.1f", gps.location.isValid(), gps.satellites.value(), gps.hdop.hdop());
    // Basic sanity check: need a valid location and a realistic number of satellites.
    if (!gps.location.isValid() || !gps.satellites.isValid() || gps.satellites.value() < 3 || gps.satellites.value() > 90) {
        return;
    }

    float lat = gps.location.lat();
    float lon = gps.location.lng();
    uint32_t now = MILLIS();

    // Initialization if this is the first valid position
    if (!ownPositionValid) {
        ownPositionLat = lat;
        ownPositionLon = lon;
        ownPositionValid = true;
        lastValidTime = now;
        return;
    }

    // 1. Filtre anti-saut spatial absolu (Supersonic Spike Rejection)
    double distMeters = TinyGPSPlus::distanceBetween(ownPositionLat, ownPositionLon, lat, lon);
    double dtSeconds = (now - lastValidTime) / 1000.0;
    if (dtSeconds > 0.0 && dtSeconds < 120.0) { 
        double speedKmph = (distMeters / dtSeconds) * 3.6;
        if (speedKmph > MAX_SPEED_KMPH) {
            ESP_LOGW(TAG, "GPS Jump rejected: %.1fm in %.1fs (%.1f km/h)", distMeters, dtSeconds, speedKmph);
            return;
        }
    }
    lastValidTime = now;

    // 2. Filtre anti-gigue à l'arrêt (Jitter Filter)
    if (gps.speed.isValid() && gps.speed.kmph() < MIN_SPEED_KMPH) {
        return; // When stationary, do not update to freeze map and icon
    }

    // 2b. Poor signal freeze: HDOP > 8 and speed < 5 km/h → freeze
    // Prevents small jumps (<150 km/h) caused by multipath/few satellites indoors.
    // Does not trigger when moving (speed >= 5 km/h) or with good signal (HDOP <= 8).
    if (gps.hdop.isValid() && gps.hdop.hdop() > 8.0f) {
        float spd = gps.speed.isValid() ? gps.speed.kmph() : 0.0f;
        if (spd < 5.0f) {
            ESP_LOGD(TAG, "Poor signal freeze: HDOP=%.1f speed=%.1f km/h", gps.hdop.hdop(), spd);
            return;
        }
    }

    // 3. Smooth Interpolation (Low-pass filter)
    // At high speed (>=30 km/h), GPS track is reliable: use direct assignment (alpha=1).
    // At low speed, HDOP-adaptive smoothing suppresses jitter.
    float currentAlpha = 0.5f;
    if (gps.speed.isValid() && gps.speed.kmph() >= 30.0f) {
        currentAlpha = 1.0f; // Direct assignment: no lag at car speed
    } else if (gps.hdop.isValid()) {
        float hdop = fmax(1.0f, gps.hdop.hdop());
        currentAlpha = fmax(0.1f, 1.0f / hdop); // HDOP 1 = alpha 1.0. HDOP 5 = alpha 0.2.
    }

    ownPositionLat += currentAlpha * (lat - ownPositionLat);
    ownPositionLon += currentAlpha * (lon - ownPositionLon);

    // Diagnostics: distance entre GPS brut et position filtrée
    lastDeltaMeters = (float)TinyGPSPlus::distanceBetween(lat, lon, ownPositionLat, ownPositionLon);
    lastAlpha = currentAlpha;

    // Log CSV sur SD: gps_lat,gps_lon,speed_kmh,hdop,alpha,filt_lat,filt_lon,delta_m
    char logLine[256];
    snprintf(logLine, sizeof(logLine), "%.6f,%.6f,%.2f,%.1f,%.2f,%.6f,%.6f,%.1f",
             lat, lon, gps.speed.kmph(),
             gps.hdop.isValid() ? gps.hdop.hdop() : 0.0f,
             currentAlpha, ownPositionLat, ownPositionLon, lastDeltaMeters);
    SD_Logger::log(SD_Logger::INFO, "GPS_DEBUG", logLine);
}

void MapGPSFilter::addOwnTracePoint() {
    float lat, lon;
    if (!getUiPosition(&lat, &lon)) return; // No valid GPS position yet

    // Ensure we only add a new point if we moved enough from the last trace point.
    // This prevents the buffer from filling up with identical points during standing updates.
    if (ownTraceCount > 0) {
        int lastIdx = (ownTraceHead - 1 + OWN_TRACE_MAX_POINTS) % OWN_TRACE_MAX_POINTS;
        float lastLat = ownTrace[lastIdx].lat;
        float lastLon = ownTrace[lastIdx].lon;
        
        // Threshold: 0.0001 degrees is roughly 11 meters
        if (fabs(lat - lastLat) < TRACE_MIN_DISTANCE && fabs(lon - lastLon) < TRACE_MIN_DISTANCE) {
            return; // Hasn't moved enough from the last recorded trace point
        }
    }

    // Compact when buffer is full: simplify first half to free space
    if (ownTraceCount >= OWN_TRACE_MAX_POINTS) {
        compactTrace();
    }

    // Add point to circular buffer
    ownTrace[ownTraceHead].lat = lat;
    ownTrace[ownTraceHead].lon = lon;

    // Update circular buffer indices
    ownTraceHead = (ownTraceHead + 1) % OWN_TRACE_MAX_POINTS;
    if (ownTraceCount < OWN_TRACE_MAX_POINTS) {
        ownTraceCount++;
    }
}

void MapGPSFilter::compactTrace() {
    // Linearize circular buffer into a temporary array
    TracePoint linear[OWN_TRACE_MAX_POINTS];
    int startIdx = (ownTraceHead - ownTraceCount + OWN_TRACE_MAX_POINTS) % OWN_TRACE_MAX_POINTS;
    for (int i = 0; i < ownTraceCount; i++) {
        linear[i] = ownTrace[(startIdx + i) % OWN_TRACE_MAX_POINTS];
    }

    // Douglas-Peucker on the first half: simplify older points
    int halfCount = ownTraceCount / 2;
    bool keep[OWN_TRACE_MAX_POINTS];
    memset(keep, 0, sizeof(keep));
    keep[0] = true;              // Always keep first point (trip start)
    keep[halfCount - 1] = true;  // Always keep boundary point

    // Epsilon ~0.0002 degrees (~22m) preserves route shape
    STATION_Utils::douglasPeuckerSimplify(linear, 0, halfCount - 1, keep, 0.0002f);

    // Rebuild: kept points from first half + all points from second half
    int writeIdx = 0;
    for (int i = 0; i < halfCount; i++) {
        if (keep[i]) {
            ownTrace[writeIdx++] = linear[i];
        }
    }
    for (int i = halfCount; i < ownTraceCount; i++) {
        ownTrace[writeIdx++] = linear[i];
    }

    ownTraceCount = writeIdx;
    ownTraceHead = writeIdx;

    ESP_LOGD(TAG, "Trace compacted: %d -> %d points", OWN_TRACE_MAX_POINTS, writeIdx);
}

bool MapGPSFilter::getUiPosition(float* lat, float* lon) const {
    if (ownPositionValid) {
        *lat = ownPositionLat;
        *lon = ownPositionLon;
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
    int bufferIndex = (ownTraceHead - ownTraceCount + index + OWN_TRACE_MAX_POINTS) % OWN_TRACE_MAX_POINTS;
    return ownTrace[bufferIndex];
}