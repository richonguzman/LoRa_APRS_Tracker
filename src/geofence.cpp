#include "geofence.h"
//#include <TinyGPS++.h>
#include "display.h"
#include "power_utils.h"

bool geofence_pause = false;

void applyGeofence(TinyGPSPlus &gps, Beacon* currentBeacon) {
    if (!gps.location.isValid()) {
        return;
    }

    // calculate distance
    double distance = TinyGPSPlus::distanceBetween(
        gps.location.lat(), 
        gps.location.lng(), 
        currentBeacon->geofence_latitude, 
        currentBeacon->geofence_longitude
    );

    if (distance < (double)currentBeacon->geofence_radius) {
        
        if (currentBeacon->geofence_mode == "inactive") {
            geofence_pause = false;
            return;
        }

        if (currentBeacon->geofence_mode == "pause") {
            geofence_pause = true;
            displayShow("GEOFENCE","", "TX pausing","Radius: " + String(double(currentBeacon->geofence_radius),0)+" m","Distance: " + String(distance,0) + " m","", 100);
            delay(2000);
            return;
        }

        if (currentBeacon->geofence_mode == "poweroff") {
            geofence_pause = true;
            displayShow("GEOFENCE", "", "powering off", 100);
            delay(2000);
            POWER_Utils::shutdown();
        }
    }
    else
    {
           geofence_pause = false;
     }
    return;
}
