// mock_gpsfix.h — Minimal gps_fix mock for native unit tests (no Arduino deps)
#pragma once
#include <cstdint>
#include <cmath>

class gps_fix {
public:
    struct valid_t {
        bool location   = false;
        bool satellites = false;
        bool hdop       = false;
        bool speed      = false;
        bool heading    = false;
        bool altitude   = false;
        bool time       = false;
        bool date       = false;
        void init() { location = satellites = hdop = speed = heading = altitude = time = date = false; }
    } valid;

    uint8_t satellites = 0;
    uint16_t hdop = 0;  // x1000

    struct { uint16_t whole = 0; uint16_t frac = 0; } spd;
    struct { int16_t whole = 0; } alt;
    struct { float value = 0; } hdg;

    // Internal storage: lat/lon in 1e-7 degrees (int32_t)
    void latitudeL(int32_t v) { _lat = v; }
    void longitudeL(int32_t v) { _lon = v; }
    int32_t latitudeL() const { return _lat; }
    int32_t longitudeL() const { return _lon; }

    double latitude() const { return _lat * 1e-7; }
    double longitude() const { return _lon * 1e-7; }

    float speed_kph() const { return spd.whole / 100.0f; }
    float heading() const { return hdg.value; }

private:
    int32_t _lat = 0;
    int32_t _lon = 0;
};
