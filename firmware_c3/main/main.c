/* LoRa APRS Tracker — ESP32-C3 firmware
 *
 * Manages: GNSS, LoRa radio (SX1262), SmartBeaconing.
 * Communicates with ESP32-S3 (UI) via binary UART protocol.
 *
 * Task architecture:
 *   - gps_rx    (prio 6): UART1 NMEA parser + filter
 *   - proto_rx  (prio 5): UART0 protocol parser (S3 commands)
 *   - main_loop (prio 4): SmartBeaconing + GPS fix forwarding + LoRa RX poll + status
 */

#include "gps_uart.h"
#include "proto_uart.h"
#include "smartbeacon.h"
#include "lora_radio.h"
#include "aprs_packet.h"
#include "pinout.h"
#include "uart_proto.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "main";

/* --- Global state --- */

static config_t      current_config;
static bool          config_received = false;
static gps_fix_t     last_sent_fix;
static bool          smartbeacon_active = true;
static bool          lora_ready = false;
static bool          proto_ready = false;   /* proto UART init'd */

/* APRS identity — defaults, updated by CONFIG from S3 */
static aprs_config_t aprs_cfg = {
    .callsign    = "N0CALL",
    .ssid        = 7,
    .symbol_table = '/',
    .symbol      = '>',
    .comment     = "",
    .dest        = "APLT00",
    .path        = "WIDE1-1",
};

/* Status counters */
static uint16_t rx_count    = 0;
static uint16_t tx_count    = 0;
static uint16_t error_count = 0;

/* --- LoRa RX callback (called from main_loop via lora_check_rx) --- */

static void on_lora_rx(const uint8_t *data, uint16_t len,
                       int16_t rssi, int8_t snr, uint16_t freq_err)
{
    ESP_LOGI(TAG, "LoRa RX: %u bytes, RSSI=%.1f SNR=%.1f",
             len, rssi / 10.0f, snr / 4.0f);

    /* Forward to S3 */
    proto_send_lora_rx(data, len, rssi, snr, 5, freq_err);
    rx_count++;
}

/* --- Handler for messages from S3 --- */

static void on_s3_message(uint8_t type, const uint8_t *payload, uint16_t length)
{
    switch (type) {

    case MSG_LORA_TX_REQ: {
        if (length < sizeof(lora_tx_req_t)) break;
        const lora_tx_req_t *req = (const lora_tx_req_t *)payload;
        ESP_LOGI(TAG, "TX_REQ from S3: %u bytes", req->pkt_len);

        uint8_t tx_status = TX_OK;
        uint16_t irq = 0;

        if (lora_ready) {
            int ret = lora_transmit(req->data, req->pkt_len);
            if (ret != 0) {
                tx_status = TX_SPI_ERROR;
                error_count++;
            }
            /* Re-arm RX after TX */
            lora_start_rx(on_lora_rx);
        } else {
            tx_status = TX_SPI_ERROR;
        }

        proto_send_tx_ack(tx_status, irq);
        tx_count++;
        break;
    }

    case MSG_CONFIG: {
        if (length < sizeof(config_t)) break;
        memcpy(&current_config, payload, sizeof(config_t));
        config_received = true;

        /* Update SmartBeaconing config */
        smartbeacon_cfg_t sb = {
            .slow_rate      = current_config.sb_slow_rate,
            .fast_rate      = current_config.sb_fast_rate,
            .min_speed      = current_config.sb_min_speed,
            .max_speed      = current_config.sb_max_speed,
            .min_turn_angle = 280,  /* 28.0 degrees — hardcoded for now */
            .turn_slope     = 240,
        };
        smartbeacon_update_cfg(&sb);
        smartbeacon_active = current_config.sb_active;

        /* Reconfigure radio if already init'd */
        if (lora_ready) {
            lora_config_t lc = {
                .freq     = current_config.freq,
                .tx_power = current_config.tx_power,
                .sf       = current_config.sf,
                .bw       = current_config.bw,
                .cr       = current_config.cr,
                .preamble = current_config.preamble,
            };
            lora_reconfigure(&lc);
            lora_start_rx(on_lora_rx);
        }

        ESP_LOGI(TAG, "CONFIG: freq=%lu txp=%d sf=%u bw=%u sb=%s",
                 (unsigned long)current_config.freq,
                 current_config.tx_power,
                 current_config.sf,
                 current_config.bw,
                 smartbeacon_active ? "ON" : "OFF");

        proto_send(MSG_CONFIG_ACK, NULL, 0);
        break;
    }

    default:
        ESP_LOGW(TAG, "Unknown msg type 0x%02X (%u bytes)", type, length);
        break;
    }
}

/* --- Send periodic status heartbeat --- */

static void send_status(uint8_t gps_fix_type, uint8_t gps_sats)
{
    status_t st = {
        .gps_fix     = gps_fix_type,
        .gps_sats    = gps_sats,
        .lora_state  = lora_get_state(),
        .flags       = 0,
        .rx_count    = rx_count,
        .tx_count    = tx_count,
        .error_count = error_count,
        .uptime      = (uint16_t)(esp_timer_get_time() / 60000000ULL),
    };
    proto_send_status(&st);

    /* Stack high water mark — runtime monitoring */
    ESP_LOGD(TAG, "Stack HWM: main=%u gps=%u proto=%u, free heap=%lu",
             (unsigned)uxTaskGetStackHighWaterMark(NULL),
             (unsigned)uxTaskGetStackHighWaterMark(xTaskGetHandle("gps_rx")),
             (unsigned)uxTaskGetStackHighWaterMark(xTaskGetHandle("proto_rx")),
             (unsigned long)esp_get_free_heap_size());
}

/* --- Main loop task --- */

#define STATUS_INTERVAL_MS  5000
#define FIX_FORWARD_MS      1000

static void main_loop_task(void *arg)
{
    int64_t last_status_us  = 0;
    int64_t last_fix_fwd_us = 0;
    gps_fix_t fix;
    memset(&fix, 0, sizeof(fix));

    ESP_LOGI(TAG, "Main loop started");

    for (;;) {
        int64_t now = esp_timer_get_time();

        /* --- LoRa RX poll --- */
        if (lora_ready) {
            lora_check_rx();
        }

        /* --- GPS fix handling --- */
        if (gps_get_last_fix(&fix)) {

            /* Forward to S3 (rate-limited) */
            if ((now - last_fix_fwd_us) >= (FIX_FORWARD_MS * 1000LL)) {
                proto_send_gps_fix(&fix);
                last_fix_fwd_us = now;
            }

            /* SmartBeaconing decision */
            if (smartbeacon_active && smartbeacon_should_tx(&fix)) {
                ESP_LOGI(TAG, "SmartBeacon TX: lat=%ld lon=%ld spd=%u hdg=%u",
                         (long)fix.lat, (long)fix.lon, fix.speed, fix.heading);

                /* Build APRS position packet */
                uint8_t pkt[256];
                int pkt_len = aprs_build_position(pkt, sizeof(pkt), &aprs_cfg, &fix);

                if (pkt_len > 0 && lora_ready) {
                    int ret = lora_transmit(pkt, (uint16_t)pkt_len);
                    if (ret == 0) {
                        proto_send_tx_ack(TX_OK, 0);
                    } else {
                        proto_send_tx_ack(TX_SPI_ERROR, 0);
                        error_count++;
                    }
                    /* Re-arm RX */
                    lora_start_rx(on_lora_rx);
                }

                smartbeacon_tx_done();
                last_sent_fix = fix;
                tx_count++;
            }
        }

        /* --- Status heartbeat --- */
        if ((now - last_status_us) >= (STATUS_INTERVAL_MS * 1000LL)) {
            send_status(fix.fix_type, fix.sats);
            last_status_us = now;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* --- Entry point --- */

void app_main(void)
{
    /* Wait for CP2102N UART bridge to stabilise after ROM bootloader reinit */
    esp_rom_delay_us(50000);

    ESP_LOGI(TAG, "LoRa APRS C3 starting...");

    /* Default SmartBeaconing config */
    smartbeacon_cfg_t sb_default = {
        .slow_rate      = 180,  /* 30 minutes */
        .fast_rate      = 60,   /* 60 seconds */
        .min_speed      = 5,    /* km/h */
        .max_speed      = 90,   /* km/h */
        .min_turn_angle = 280,  /* 28.0 degrees */
        .turn_slope     = 240,
    };
    smartbeacon_init(&sb_default);

    /* Default GPS filter */
    gps_filter_cfg_t gps_filter = {
        .min_hdop     = 500,    /* 5.0 */
        .min_sats     = 4,
        .min_fix_type = 2,
        .static_speed = 50,     /* 0.5 m/s */
    };
    gps_uart_init(&gps_filter);

    // Protocol UART (toward S3) — DISABLED for debug (shared CP2102N)
    // proto_uart_init();
    // proto_set_rx_handler(on_s3_message);

    /* LoRa radio init — default 433.775 MHz APRS */
    lora_config_t lora_cfg = {
        .freq     = 433775000,
        .tx_power = 22,
        .sf       = 12,
        .bw       = 7,     /* 125 kHz */
        .cr       = 5,
        .preamble = 8,
    };
    int lora_ret = lora_init(&lora_cfg);
    if (lora_ret == 0) {
        lora_ready = true;
        esp_rom_delay_us(100000); /* let UART settle before RX */
        lora_start_rx(on_lora_rx);
        ESP_LOGI(TAG, "LoRa radio ready, RX started");
    } else {
        ESP_LOGE(TAG, "LoRa init failed (%d) — running without radio", lora_ret);
        // proto_send_error(ERR_LORA_INIT, (uint16_t)lora_ret);  // proto disabled
    }

    /* Main loop */
    xTaskCreate(main_loop_task, "main_loop", 6144, NULL, 4, NULL);

    ESP_LOGI(TAG, "All tasks started");
}
