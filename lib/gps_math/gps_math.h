// gps_math.h — Standalone Haversine distance and bearing calculations
// Ported from IceNav-v3 gpsMath.hpp (Jordi Gauchía), without LUT dependency.
#pragma once

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define EARTH_RADIUS 6378137.0f  // meters

static inline float DEG2RAD(float deg) { return deg * (M_PI / 180.0f); }
static inline float RAD2DEG(float rad) { return rad * (180.0f / M_PI); }

// Haversine distance in meters between two points (degrees)
float calcDist(float lat1, float lon1, float lat2, float lon2);

// Initial bearing (degrees 0-360) from point 1 to point 2 (degrees)
float calcCourse(float lat1, float lon1, float lat2, float lon2);
