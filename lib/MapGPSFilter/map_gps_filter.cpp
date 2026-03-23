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
    _mutex = xSemaphoreCreateMutex();
    reset();
}

void MapGPSFilter::reset() {
    ownPositionLat = 0.0;
    ownPositionLon = 0.0;
    ownPositionValid = false;
    lastValidTime = 0;
    lastRawLat = 0.0;
    lastRawLon = 0.0;
    ownTraceCount = 0;
    ownTraceHead = 0;
    memset(ownTrace, 0, sizeof(ownTrace));
    lastDeltaMeters = 0.0f;
    lastAlpha = 0.0f;
}

void MapGPSFilter::updateFilteredOwnPosition(const gps_fix& fix) {
    float hdopVal = fix.valid.hdop ? (float)fix.hdop / 1000.0f : 99.0f;
    ESP_LOGV(TAG, "Update called: location.isValid=%d, sats=%d, hdop=%.1f",
             fix.valid.location, fix.satellites, hdopVal);

    // Basic sanity check: need a valid location.
    if (!fix.valid.location || !fix.valid.satellites || fix.satellites > 90) {
        return;
    }

    double lat = fix.latitude();
    double lon = fix.longitude();
    uint32_t now = MILLIS();

    // 6 sats or fewer: allow initial fix only, then freeze position until 7+
    if (fix.satellites <= 6) {
        if (!ownPositionValid) {
            if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                ownPositionLat = lat;
                ownPositionLon = lon;
                ownPositionValid = true;
                xSemaphoreGive(_mutex);
            }
            lastRawLat = lat;
            lastRawLon = lon;
        }
        lastValidTime = now;  // Prevent dtSeconds from growing during low-sat periods
        return;
    }

    // Bug #4 fix: skip if same raw position as last call (no new NMEA sentence)
    if (ownPositionValid && lat == lastRawLat && lon == lastRawLon) {
        return;
    }
    lastRawLat = lat;
    lastRawLon = lon;

    // Initialization if this is the first valid position (7+ sats)
    if (!ownPositionValid) {
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            ownPositionLat = lat;
            ownPositionLon = lon;
            ownPositionValid = true;
            xSemaphoreGive(_mutex);
        }
        lastValidTime = now;
        return;
    }

    // 1. Filtre anti-saut spatial absolu (Supersonic Spike Rejection)
    double distMeters = calcDist(ownPositionLat, ownPositionLon, lat, lon);
    double dtSeconds = (now - lastValidTime) / 1000.0;
    if (dtSeconds > 0.0) {
        if (dtSeconds < 120.0) {
            double speedKmph = (distMeters / dtSeconds) * 3.6;
            if (speedKmph > MAX_SPEED_KMPH) {
                ESP_LOGW(TAG, "GPS Jump rejected: %.1fm in %.1fs (%.1f km/h)",
                         distMeters, dtSeconds, speedKmph);
                return;
            }
        } else {
            // Long gap (>2 min): accept only if distance is reasonable (<5 km)
            if (distMeters > 5000.0) {
                ESP_LOGW(TAG, "GPS Teleport rejected after long gap: %.1fm in %.1fs",
                         distMeters, dtSeconds);
                return;
            }
        }
    }
    lastValidTime = now;

    // 2. Poor signal freeze: HDOP > 4 -> unconditional freeze
    //    Speed is unreliable when HDOP is bad (indoor multipath)
    if (fix.valid.hdop && hdopVal > 3.0f) {
        ESP_LOGD(TAG, "Poor signal freeze: HDOP=%.1f", hdopVal);
        return;
    }

    // 2b. Fallback: no HDOP available and few satellites -> freeze
    if (!fix.valid.hdop && fix.satellites < 6) {
        ESP_LOGD(TAG, "No HDOP and few sats (%d) - freeze", fix.satellites);
        return;
    }

    // 2c. Filtre anti-gigue a l'arret (Jitter Filter)
    float speedKph = fix.valid.speed ? fix.speed_kph() : 0.0f;
    if (fix.valid.speed && speedKph < MIN_SPEED_KMPH) {
        return; // When stationary, do not update to freeze map and icon
    }

    // 2d. Cross-check: GPS reports low speed but position barely changed → Doppler jitter
    if (fix.valid.speed && speedKph < 4.0f && distMeters < 10.0) {
        return;
    }

    // 3. Smooth Interpolation (Low-pass filter)
    // At high speed (>=30 km/h), GPS track is reliable: use direct assignment (alpha=1).
    // At low/medium speed, cap alpha to smooth out GPS jitter.
    float currentAlpha = 0.5f;
    if (fix.valid.speed && speedKph >= 30.0f) {
        currentAlpha = 1.0f; // Haute vitesse : on fait confiance au GPS, pas de lag
    } else if (fix.valid.hdop) {
        float hdop = fmax(1.0f, hdopVal);
        currentAlpha = fmax(0.05f, 1.0f / hdop); // Base de confiance sur la géométrie des satellites

        // Amortissement progressif basé sur la vitesse (marche / arrêt)
        if (speedKph < 2.0f) {
            // Quasi à l'arrêt ou très lent : on filtre énormément (bruit de positionnement)
            currentAlpha = fmin(currentAlpha, 0.1f); 
        } else if (speedKph < 15.0f) {
            // Vitesse de marche/course/vélo lent (2 à 15 km/h)
            // On map linéairement ou empiriquement. À 5km/h, on aura un alpha doux.
            currentAlpha = fmin(currentAlpha, 0.25f);
        }
    }

    // Diagnostics BEFORE update: distance between raw GPS and current filtered position
    lastDeltaMeters = calcDist(lat, lon, ownPositionLat, ownPositionLon);
    lastAlpha = currentAlpha;

    // Apply low-pass filter (double precision avoids catastrophic cancellation)
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        ownPositionLat += currentAlpha * (lat - ownPositionLat);
        ownPositionLon += currentAlpha * (lon - ownPositionLon);
        xSemaphoreGive(_mutex);
    }

    // Log CSV on SD: gps_lat,gps_lon,speed_kmh,hdop,alpha,filt_lat,filt_lon,delta_m
#ifndef UNIT_TEST
    char logLine[256];
    snprintf(logLine, sizeof(logLine), "%.8f,%.8f,%.2f,%.1f,%.2f,%.8f,%.8f,%.1f",
             lat, lon, speedKph, hdopVal,
             currentAlpha, ownPositionLat, ownPositionLon, lastDeltaMeters);
    SD_Logger::log(SD_Logger::INFO, "GPS_DEBUG", logLine);
#endif
}

void MapGPSFilter::addOwnTracePoint() {
    double lat, lon;
    if (!getUiPosition(&lat, &lon)) return; // No valid GPS position yet

    // Ensure we only add a new point if we moved enough from the last trace point.
    if (ownTraceCount > 0) {
        int lastIdx = (ownTraceHead - 1 + OWN_TRACE_MAX_POINTS) % OWN_TRACE_MAX_POINTS;
        float lastLat = ownTrace[lastIdx].lat;
        float lastLon = ownTrace[lastIdx].lon;

        if (fabs(lat - lastLat) < TRACE_MIN_DISTANCE && fabs(lon - lastLon) < TRACE_MIN_DISTANCE) {
            return; // Hasn't moved enough from the last recorded trace point
        }
    }

    // Compact when buffer is full: simplify first half to free space
    if (ownTraceCount >= OWN_TRACE_MAX_POINTS) {
        compactTrace();
    }

    // Add point to circular buffer (TracePoint uses float — acceptable for trace)
    ownTrace[ownTraceHead].lat = (float)lat;
    ownTrace[ownTraceHead].lon = (float)lon;

    // Update circular buffer indices
    ownTraceHead = (ownTraceHead + 1) % OWN_TRACE_MAX_POINTS;
    if (ownTraceCount < OWN_TRACE_MAX_POINTS) {
        ownTraceCount++;
    }
}

void MapGPSFilter::compactTrace() {
    // Linearize circular buffer into a temporary array (static to avoid 6KB stack usage)
    static TracePoint linear[OWN_TRACE_MAX_POINTS];
    int startIdx = (ownTraceHead - ownTraceCount + OWN_TRACE_MAX_POINTS) % OWN_TRACE_MAX_POINTS;
    for (int i = 0; i < ownTraceCount; i++) {
        linear[i] = ownTrace[(startIdx + i) % OWN_TRACE_MAX_POINTS];
    }

    // Douglas-Peucker on the first half: simplify older points
    int halfCount = ownTraceCount / 2;
    static bool keep[OWN_TRACE_MAX_POINTS];
    memset(keep, 0, sizeof(keep));
    keep[0] = true;              // Always keep first point (trip start)
    keep[halfCount - 1] = true;  // Always keep boundary point

    // Epsilon ~0.00003 degrees (~3m) preserves tight route shape (walking/hiking)
    #ifndef UNIT_TEST
    STATION_Utils::douglasPeuckerSimplify(linear, 0, halfCount - 1, keep, 0.00003f);
    #else
    // Mock simplification for unit tests: keep all points
    for (int i = 0; i < halfCount; i++) {
        keep[i] = true;
    }
#endif

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
    // Ensure head stays within circular buffer bounds (0..OWN_TRACE_MAX_POINTS-1)
    ownTraceHead = writeIdx % OWN_TRACE_MAX_POINTS;

    ESP_LOGD(TAG, "Trace compacted: %d -> %d points", OWN_TRACE_MAX_POINTS, writeIdx);
}

bool MapGPSFilter::getUiPosition(double* lat, double* lon) const {
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) != pdTRUE) return false;
    bool valid = ownPositionValid;
    if (valid) {
        *lat = ownPositionLat;
        *lon = ownPositionLon;
    }
    xSemaphoreGive(_mutex);
    return valid;
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
