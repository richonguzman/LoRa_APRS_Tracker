// gps_math.cpp — Standalone Haversine distance and bearing calculations
// Ported from IceNav-v3 gpsMath.cpp (Jordi Gauchía), without LUT dependency.
#include "gps_math.h"

float calcDist(float lat1, float lon1, float lat2, float lon2) {
    float lat1_rad = DEG2RAD(lat1);
    float lon1_rad = DEG2RAD(lon1);
    float lat2_rad = DEG2RAD(lat2);
    float lon2_rad = DEG2RAD(lon2);
    float dlat = lat2_rad - lat1_rad;
    float dlon = lon2_rad - lon1_rad;

    float a = sinf(dlat * 0.5f) * sinf(dlat * 0.5f) +
              cosf(lat1_rad) * cosf(lat2_rad) *
              sinf(dlon * 0.5f) * sinf(dlon * 0.5f);

    float c = 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
    return EARTH_RADIUS * c;
}

float calcCourse(float lat1, float lon1, float lat2, float lon2) {
    float lat1_rad = DEG2RAD(lat1);
    float lat2_rad = DEG2RAD(lat2);
    float dlon_rad = DEG2RAD(lon2 - lon1);

    float y = sinf(dlon_rad) * cosf(lat2_rad);
    float x = cosf(lat1_rad) * sinf(lat2_rad) -
              sinf(lat1_rad) * cosf(lat2_rad) * cosf(dlon_rad);

    float course = atan2f(y, x) * (180.0f / M_PI);
    if (course < 0.0f)
        course += 360.0f;

    return course;
}
