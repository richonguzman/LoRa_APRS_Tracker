#ifndef MAP_INPUT_H
#define MAP_INPUT_H

/* Map input handlers — extracted from ui_map_manager.cpp (Étape 4)
 * Touch pan, zoom, recenter, back, GPX recording.
 */

#ifdef USE_LVGL_UI

#include <lvgl.h>

namespace MapInput {

    // Touch pan handler (attach to map_canvas LV_EVENT_ALL)
    void map_touch_event_cb(lv_event_t* e);

    // Button handlers
    void btn_map_zoomin_clicked(lv_event_t* e);
    void btn_map_zoomout_clicked(lv_event_t* e);
    void btn_map_recenter_clicked(lv_event_t* e);
    void btn_map_back_clicked(lv_event_t* e);
    void btn_gpx_rec_clicked(lv_event_t* e);

    // Update GPX record button visual state
    void updateGpxRecButton();

    // Pan map by dx/dy pixels (also called from timer_cb for inertia)
    void scrollMap(int16_t dx, int16_t dy);

} // namespace MapInput

#endif // USE_LVGL_UI
#endif // MAP_INPUT_H
