#ifndef MAP_RENDER_H
#define MAP_RENDER_H

/* Map rendering functions — extracted from ui_map_manager.cpp (Étape 3)
 * Handles sprite double-buffering, station overlay, traces, and canvas updates.
 */

#ifdef USE_LVGL_UI

namespace MapRender {

    // Copy back→front sprite (byte-swap for raster, memcpy for NAV)
    // Caller MUST hold MapEngine::renderLock.
    void copyBackToFront();

    // Apply rendered viewport: copy sprite to front, update canvas position & UI labels.
    // Call after render completes (NAV async done) or after GPS pan.
    void applyRenderedViewport();

    // Lightweight refresh: restore back→front, redraw stations/traces only.
    // ~10-50ms vs 500-3000ms for full re-render.
    void refreshStationOverlay();

    // Reset hit zones (call before redrawing stations)
    void cleanup_station_buttons();

    // Draw all stations + own position on canvas
    void update_station_objects();

    // Draw received station traces (blue lines)
    void draw_station_traces();

    // Draw own GPS trace (purple line)
    void draw_own_trace();

} // namespace MapRender

#endif // USE_LVGL_UI
#endif // MAP_RENDER_H
