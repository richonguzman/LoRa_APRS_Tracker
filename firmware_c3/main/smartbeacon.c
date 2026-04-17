/* SmartBeaconing — autonomous TX decision logic
 *
 * Standard APRS SmartBeaconing algorithm:
 * - Fast rate when moving fast
 * - Slow rate when stationary or slow
 * - Corner pegging: TX on significant heading change
 */

#include "smartbeacon.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <stdlib.h>
#include <math.h>

static const char *TAG = "sbeacon";

static smartbeacon_cfg_t cfg;

static int64_t  last_tx_time = 0;   /* microseconds */
static uint16_t last_heading = 0;
static bool     has_last_heading = false;

void smartbeacon_init(const smartbeacon_cfg_t *c)
{
    cfg = *c;
    last_tx_time    = 0;
    has_last_heading = false;

    ESP_LOGI(TAG, "init: slow=%us fast=%us speed=%u-%u km/h turn=%u.%u deg",
             cfg.slow_rate * 10, cfg.fast_rate,
             cfg.min_speed, cfg.max_speed,
             cfg.min_turn_angle / 10, cfg.min_turn_angle % 10);
}

void smartbeacon_update_cfg(const smartbeacon_cfg_t *c)
{
    cfg = *c;
    ESP_LOGI(TAG, "config updated");
}

bool smartbeacon_should_tx(const gps_fix_t *fix)
{
    int64_t now = esp_timer_get_time();

    /* First fix ever: TX immediately */
    if (last_tx_time == 0) {
        return true;
    }

    int64_t elapsed_us = now - last_tx_time;
    int elapsed_s = (int)(elapsed_us / 1000000);

    /* Speed in km/h (fix->speed is cm/s) */
    float speed_kmh = (float)fix->speed * 0.036f;

    /* Slow rate: stationary or below min_speed */
    uint32_t slow_rate_s = (uint32_t)cfg.slow_rate * 10;
    if (speed_kmh < (float)cfg.min_speed) {
        return elapsed_s >= (int)slow_rate_s;
    }

    /* Fast rate: proportional to speed */
    uint32_t rate_s;
    if (speed_kmh >= (float)cfg.max_speed) {
        rate_s = cfg.fast_rate;
    } else {
        /* Linear interpolation between slow_rate and fast_rate */
        float ratio = (speed_kmh - (float)cfg.min_speed) /
                      (float)(cfg.max_speed - cfg.min_speed);
        rate_s = (uint32_t)(slow_rate_s - ratio * (slow_rate_s - cfg.fast_rate));
    }

    if (elapsed_s >= (int)rate_s) {
        return true;
    }

    /* Corner pegging: heading change */
    if (has_last_heading && speed_kmh >= (float)cfg.min_speed) {
        int16_t heading_deg10 = (int16_t)(fix->heading / 10); /* heading is x100 -> x10 */
        int16_t last_deg10    = (int16_t)(last_heading / 10);
        int16_t diff = heading_deg10 - last_deg10;

        /* Normalize to -1800..+1800 */
        if (diff > 1800) diff -= 3600;
        if (diff < -1800) diff += 3600;
        if (diff < 0) diff = -diff;

        /* Dynamic threshold: min_turn + turn_slope / speed */
        uint16_t threshold = cfg.min_turn_angle;
        if (speed_kmh > 0.1f) {
            threshold += (uint16_t)((float)cfg.turn_slope / speed_kmh);
        }

        if ((uint16_t)diff >= threshold) {
            return true;
        }
    }

    return false;
}

void smartbeacon_tx_done(void)
{
    last_tx_time = esp_timer_get_time();
    /* Note: last_heading is updated from the fix that triggered the TX.
     * The caller should call this after getting the fix. */
}
