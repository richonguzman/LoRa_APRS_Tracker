// Dummy Arduino.h for native tests
#ifndef ARDUINO_H
#define ARDUINO_H

#include <stdint.h>
#include <string.h>
#include <math.h>
#include <string>
#include <chrono>

unsigned long millis();

typedef uint8_t byte;

#ifndef TWO_PI
#define TWO_PI 6.283185307179586476925286766559
#endif

inline double radians(double deg) { return deg * 0.017453292519943295769236907684886; }
inline double degrees(double rad) { return rad * 57.295779513082320876798154814105; }
inline double sq(double x) { return x * x; }

// Minimal Arduino String mock for station_utils.h
class String {
public:
    std::string s;
    String() {}
    String(const char* c) : s(c ? c : "") {}
    String(const std::string& str) : s(str) {}
    const char* c_str() const { return s.c_str(); }
    bool operator==(const String& rhs) const { return s == rhs.s; }
};

#endif
