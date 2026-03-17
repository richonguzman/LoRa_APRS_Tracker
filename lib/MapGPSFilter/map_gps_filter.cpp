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
    ownPositionLat = 0.0f;
    ownPositionLon = 0.0f;
    ownPositionValid = false;
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
    uint32_t now = MILLIS();

    // Initialisation si c'est la première position valide
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
        if (speedKmph > MAX_SPEED_KMPH) { // Vitesse impossible (>150km/h) -> saut GPS
            ESP_LOGW(TAG, "GPS Jump: %.1fm in %.1fs (%.1f km/h)", distMeters, dtSeconds, speedKmph);
            return; // On rejette purement et simplement ce point. Rien ne bouge.
        }
    }
    lastValidTime = now;

    // 2. Filtre anti-gigue à l'arrêt (Jitter Filter)
    if (gps.speed.isValid() && gps.speed.kmph() < MIN_SPEED_KMPH) {
         return; // A l'arrêt, on ne met rien à jour pour figer la carte et l'icone
    }

    // 3. Lissage fluide (Interpolation / Low-pass filter)
    float alpha = 0.5f; // Valeur de base (moitié position courante / moitié nouvelle position)
    if (gps.hdop.isValid()) {
        float hdop = fmax(1.0f, gps.hdop.hdop());
        alpha = fmax(0.1f, 1.0f / hdop); // HDOP 1 = alpha 1.0 (direct). HDOP 5 = alpha 0.2 (très lissé).
    }

    ownPositionLat += alpha * (lat - ownPositionLat);
    ownPositionLon += alpha * (lon - ownPositionLon);
}

void MapGPSFilter::addOwnTracePoint() {
    float lat, lon;
    if (!getUiPosition(&lat, &lon)) return; // No valid GPS position yet

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
    int bufferIndex = (ownTraceHead - ownTraceCount + index + TRACE_MAX_POINTS) % TRACE_MAX_POINTS;
    return ownTrace[bufferIndex];
}