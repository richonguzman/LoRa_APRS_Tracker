#pragma once

// NeoGPS core configuration for LoRa APRS Tracker
// Copied from IceNav-v3 (unchanged — platform-independent).

#include <Arduino.h>

#ifdef __AVR__
    #define NEOGPS_PACKED_DATA
#endif

#ifdef NEOGPS_PACKED_DATA
    #define NEOGPS_BF(b) :b
    #define NEOGPS_PACKED __attribute__((packed))
#else
    #define NEOGPS_PACKED
    #define NEOGPS_BF(b)
#endif

#if (                                              \
      (ARDUINO < 10606)                          | \
     ((10700  <= ARDUINO) & (ARDUINO <= 10799 )) | \
     ((107000 <= ARDUINO) & (ARDUINO <= 107999))   \
    )                                              \
        &                                          \
    !defined(ESP8266)
    #define CONST_CLASS_DATA static const
#else
    #define CONST_CLASS_DATA static constexpr
#endif

#if defined(ARDUINO_SAMD_MKRZERO) | \
    defined(ARDUINO_SAMD_ZERO)    | \
    defined(ARDUINO_SAM_DUE)      | \
    defined(ARDUINO_ARCH_ARC32)   | \
    defined(__TC27XX__)           | \
    (defined(TEENSYDUINO) && (TEENSYDUINO < 139))
    #undef pgm_read_ptr
    #define pgm_read_ptr(addr) (*(const void **)(addr))
#endif
