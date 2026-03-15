// test/mock_arduino.h
#ifndef MOCK_ARDUINO_H
#define MOCK_ARDUINO_H

#include <cstdint>

// Mock des fonctions Arduino utilisées dans map_gps_filter.cpp
namespace MockArduino {
    extern unsigned long current_millis;  // Déclaration SEULEMENT (extern, sans =0)

    // Fonction mockée pour millis()
    inline unsigned long millis() {
        return current_millis;  // Plus d'incrément auto, setter manuel
    }

    // Reset pour les tests
    inline void reset_millis() {
        current_millis = 0;
    }

    // No-op pour d'autres fonctions potentielles (ex. delay)
    inline void delay(unsigned long) {}
}

#endif // MOCK_ARDUINO_H
