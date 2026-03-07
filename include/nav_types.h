#ifndef NAV_TYPES_H
#define NAV_TYPES_H

#include <cstdint>
#include <cstdlib>
#include <new>
#include "esp_heap_caps.h"

// Allocator that places vectors in PSRAM to preserve scarce DRAM
// for WiFi, BLE, LoRa, LVGL, and OS stacks.
// Uses MALLOC_CAP_SPIRAM with fallback to default heap.
// Throws std::bad_alloc on failure (C++ standard requirement) so that
// std::vector::reserve/push_back leave the vector in a valid state.
template <typename T>
struct PSRAMAllocator {
    using value_type = T;

    PSRAMAllocator() noexcept = default;
    template <typename U>
    PSRAMAllocator(const PSRAMAllocator<U>&) noexcept {}

    T* allocate(std::size_t n) {
        void* p = heap_caps_malloc(n * sizeof(T), MALLOC_CAP_SPIRAM);
        if (!p) p = malloc(n * sizeof(T)); // fallback to default heap
        if (!p) throw std::bad_alloc();
        return static_cast<T*>(p);
    }
    void deallocate(T* p, std::size_t) noexcept {
        heap_caps_free(p);
    }

    template <typename U>
    bool operator==(const PSRAMAllocator<U>&) const noexcept { return true; }
    template <typename U>
    bool operator!=(const PSRAMAllocator<U>&) const noexcept { return false; }
};

namespace UIMapManager {

// Global Tile Header (22 bytes)
struct NavTileHeader {
    char magic[4];          // 'NAV1'
    uint16_t featureCount;
    int32_t minLon;
    int32_t minLat;
    int32_t maxLon;
    int32_t maxLat;
} __attribute__((packed));

// Feature Header (13 bytes packed)
// Followed by: payload of payloadSize bytes
// Lines/Polygons: coords encoded as Delta+ZigZag+VarInt
// Polygons: ring data (uint16 ringCount + ringCount × uint16 ringEnds) at end of payload
// Points: 1 coord encoded as ZigZag+VarInt (delta from 0)
// Text (type 4): int16 px, int16 py, uint8 textLen, text bytes (NOT VarInt)
struct NavFeatureHeader {
    uint8_t geomType;       // 1=Point, 2=Line, 3=Polygon, 4=Text
    uint16_t colorRgb565;
    uint8_t zoomPriority;   // High nibble = minZoom, low nibble = priority
    uint8_t widthPixels;
    uint8_t bbox[4];        // x1, y1, x2, y2 (tile-relative, /16)
    uint16_t coordCount;
    uint16_t payloadSize;   // Total payload size in bytes (replaces padding)
} __attribute__((packed));

// Edge structure for Active Edge List (AEL) algorithm
struct Edge {
    int32_t yMax;
    int32_t xVal;           // 16.16 fixed-point
    int32_t slope;          // 16.16 fixed-point
    int nextInBucket;
    int nextActive;
};

// NPK2 pack file header (25 bytes)
// Pack format: header + Y-table + index (sorted y then x) + NAV1 blobs
struct Npk2Header {
    char     magic[4];        // "NPK2"
    uint8_t  zoom;
    uint32_t tile_count;
    uint32_t y_min;
    uint32_t y_max;
    uint32_t ytable_offset;   // = 25
    uint32_t index_offset;    // = 25 + y_span × 8
} __attribute__((packed));

// NPK2 Y-table entry (8 bytes per row Y)
struct Npk2YEntry {
    uint32_t idx_start;       // first index entry for this row
    uint32_t idx_count;       // number of index entries for this row
} __attribute__((packed));

// NPK2 index entry (16 bytes)
struct Npk2IndexEntry {
    uint32_t x;
    uint32_t y;
    uint32_t offset;          // from start of file
    uint32_t size;            // NAV1 blob size
} __attribute__((packed));

} // namespace UIMapManager
#endif
