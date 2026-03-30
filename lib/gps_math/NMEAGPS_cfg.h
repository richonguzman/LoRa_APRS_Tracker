#pragma once

// NeoGPS parser configuration for LoRa APRS Tracker
// Based on IceNav-v3, simplified: no satellite info, no talker ID parsing.

#include "GPSfix_cfg.h"

// Sentence parsing — only what we need
#define NMEAGPS_PARSE_GGA
//#define NMEAGPS_PARSE_GLL
// GSA enabled: required for PDOP/VDOP extraction (strict 3D mode)
#define NMEAGPS_PARSE_GSA
//#define NMEAGPS_PARSE_GSV
//#define NMEAGPS_PARSE_GST
#define NMEAGPS_PARSE_RMC
//#define NMEAGPS_PARSE_VTG
//#define NMEAGPS_PARSE_ZDA

// Last sentence in each GPS update interval
// L76K sentence order: RMC → GGA (GGA comes after RMC)
// Must be GGA so HDOP is available when the fix is dispatched
#define LAST_SENTENCE_IN_INTERVAL NMEAGPS::NMEA_GGA

// Coherent fix: all data from same cycle
#define NMEAGPS_COHERENT

// Explicit merging: GGA + RMC merged safely into one fix
#define NMEAGPS_EXPLICIT_MERGING
//#define NMEAGPS_IMPLICIT_MERGING

#ifdef NMEAGPS_IMPLICIT_MERGING
    #define NMEAGPS_MERGING NMEAGPS::IMPLICIT_MERGING
    #define NMEAGPS_INIT_FIX(m)
    #define NMEAGPS_INVALIDATE(m) m_fix.valid.m = false
#else
    #ifdef NMEAGPS_EXPLICIT_MERGING
        #define NMEAGPS_MERGING NMEAGPS::EXPLICIT_MERGING
    #else
        #define NMEAGPS_MERGING NMEAGPS::NO_MERGING
        #define NMEAGPS_NO_MERGING
    #endif
    #define NMEAGPS_INIT_FIX(m) m.valid.init()
    #define NMEAGPS_INVALIDATE(m)
#endif

#if ( defined(NMEAGPS_NO_MERGING) + \
    defined(NMEAGPS_IMPLICIT_MERGING) + \
    defined(NMEAGPS_EXPLICIT_MERGING) )  > 1
  #error Only one MERGING technique should be enabled in NMEAGPS_cfg.h!
#endif

// Fix buffer size (1 = minimal, sufficient for polling)
#define NMEAGPS_FIX_MAX 1

#if defined(NMEAGPS_EXPLICIT_MERGING) && (NMEAGPS_FIX_MAX == 0)
    #error You must define FIX_MAX >= 1 to allow EXPLICIT merging in NMEAGPS_cfg.h
#endif

#define NMEAGPS_KEEP_NEWEST_FIXES true

// Polling mode (not interrupt)
//#define NMEAGPS_INTERRUPT_PROCESSING
#ifdef  NMEAGPS_INTERRUPT_PROCESSING
    #define NMEAGPS_PROCESSING_STYLE NMEAGPS::PS_INTERRUPT
#else
    #define NMEAGPS_PROCESSING_STYLE NMEAGPS::PS_POLLING
#endif

// No talker ID — we don't need to distinguish GP/GL/GN
//#define NMEAGPS_SAVE_TALKER_ID
//#define NMEAGPS_PARSE_TALKER_ID

// No proprietary sentences
//#define NMEAGPS_PARSE_PROPRIETARY

// Enable satellite array tracking (required by NeoGPS GSA parser bug)
#define NMEAGPS_PARSE_SATELLITES
#define NMEAGPS_MAX_SATELLITES 24
//#define NMEAGPS_PARSE_SATELLITE_INFO

// Statistics (for charsProcessed equivalent)
#define NMEAGPS_STATS

// No derived types needed (no talker ID parsing)
//#define NMEAGPS_DERIVED_TYPES
#ifdef NMEAGPS_DERIVED_TYPES
    #define NMEAGPS_VIRTUAL virtual
#else
    #define NMEAGPS_VIRTUAL
#endif

#ifdef NMEAGPS_PARSE_TALKER_ID
    #ifndef NMEAGPS_DERIVED_TYPES
        #error You must define NMEAGPS_DERIVED_TYPES to parse Talker IDs!
    #endif
#endif

#define NMEAGPS_VALIDATE_CHARS false
#define NMEAGPS_VALIDATE_FIELDS false

#define NMEAGPS_COMMA_NEEDED

#define NMEAGPS_RECOGNIZE_ALL

#define NMEAGPS_PARSING_SCRATCHPAD

//#define NMEAGPS_TIMESTAMP_FROM_INTERVAL
//#define NMEAGPS_TIMESTAMP_FROM_PPS
