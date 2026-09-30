#ifndef GEOFENCE_H
#define GEOFENCE_H
#include <Arduino.h>
#include <TinyGPS++.h>
#include "configuration.h"

extern bool geofence_pause;

enum GeofenceMode {
    GEOFENCE_INACTIVE = 0,      // Mode: inactive (normal operation)
    GEOFENCE_PAUSE = 1,         // Mode: pause (no transmission)
    GEOFENCE_POWEROFF = 2       // Mode: power off
};

void applyGeofence(TinyGPSPlus &gps, Beacon* currentBeacon);

#endif
