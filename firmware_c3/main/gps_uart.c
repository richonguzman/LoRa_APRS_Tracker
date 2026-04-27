/* GNSS UART — NMEA parser + fix filter
 *
 * Parse GGA + RMC sentences, apply quality filter, expose last valid fix.
 * Runs on UART1. Thread-safe access via spinlock.
 */

#include "gps_uart.h"
#include "pinout.h"

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

static const char *TAG = "gps";

/* --- State --- */

static gps_filter_cfg_t filter_cfg;
static gps_fix_t        last_fix;
static portMUX_TYPE     fix_lock = portMUX_INITIALIZER_UNLOCKED;
static bool             fix_valid = false;
static uint32_t         raw_count = 0;
static uint32_t         filtered_count = 0;

/* Intermediate parse state (GGA + RMC combined) */
static struct {
    /* From GGA */
    int32_t  lat;           /* x 1e7 */
    int32_t  lon;           /* x 1e7 */
    int32_t  alt;           /* mm */
    uint8_t  fix_type;      /* 0, 1 (2D), 2 (3D DGPS) — remapped to our 0/2/3 */
    uint8_t  sats;
    uint16_t hdop;          /* x 100 */
    bool     gga_valid;
    /* From RMC */
    uint16_t speed;         /* cm/s */
    uint16_t heading;       /* x 100 */
    uint32_t timestamp;     /* UTC epoch seconds (date+time) */
    uint8_t  rmc_status;    /* 'A' or 'V' */
    bool     rmc_valid;
} parse;

/* NMEA line buffer */
#define NMEA_MAX_LEN 120
static char  nmea_buf[NMEA_MAX_LEN + 1];
static int   nmea_idx = 0;

/* --- NMEA parsing helpers --- */

/* Split NMEA fields by comma. Returns number of fields. */
static int nmea_split(char *line, char *fields[], int max_fields)
{
    int n = 0;
    fields[n++] = line;
    while (*line && n < max_fields) {
        if (*line == ',') {
            *line = '\0';
            fields[n++] = line + 1;
        }
        line++;
    }
    return n;
}

/* Parse NMEA lat/lon: "4807.038" + "N" -> degrees x 1e7 */
static int32_t parse_coord(const char *val, const char *dir)
{
    if (!val[0] || !dir[0]) return 0;

    /* Find decimal point */
    const char *dot = strchr(val, '.');
    if (!dot) return 0;

    /* Degrees: everything before the last 2 integer digits before dot */
    int int_len = (int)(dot - val);
    int deg_len = int_len - 2;
    if (deg_len < 1 || deg_len > 3) return 0;

    char deg_str[4] = {0};
    memcpy(deg_str, val, deg_len);
    int degrees = atoi(deg_str);

    /* Minutes: last 2 integer digits + fractional */
    double minutes = atof(val + deg_len);

    int32_t result = (int32_t)((degrees + minutes / 60.0) * 1e7);

    if (dir[0] == 'S' || dir[0] == 'W') result = -result;

    return result;
}

/* Parse time "hhmmss.ss" -> partial epoch (seconds within day) */
static uint32_t parse_time_sod(const char *val)
{
    if (strlen(val) < 6) return 0;
    int h = (val[0] - '0') * 10 + (val[1] - '0');
    int m = (val[2] - '0') * 10 + (val[3] - '0');
    int s = (val[4] - '0') * 10 + (val[5] - '0');
    return (uint32_t)(h * 3600 + m * 60 + s);
}

/* Parse date "ddmmyy" + time -> Unix epoch */
static uint32_t parse_datetime(const char *date, const char *time_str)
{
    if (strlen(date) < 6) return 0;
    int day   = (date[0] - '0') * 10 + (date[1] - '0');
    int month = (date[2] - '0') * 10 + (date[3] - '0');
    int year  = (date[4] - '0') * 10 + (date[5] - '0') + 2000;

    /* Simplified days-since-epoch (good enough for 2000-2099) */
    /* Using a basic algorithm, not worrying about leap second precision */
    int y = year;
    int m = month;
    if (m <= 2) { y--; m += 12; }
    int32_t days = 365L * y + y / 4 - y / 100 + y / 400 + (153 * (m - 3) + 2) / 5 + day - 719469;

    return (uint32_t)(days * 86400UL + parse_time_sod(time_str));
}

/* Parse $GPGGA / $GNGGA */
static void parse_gga(char *fields[], int nfields)
{
    if (nfields < 15) return;

    /* Quality: 0=invalid, 1=GPS, 2=DGPS, 6=estimated */
    int quality = atoi(fields[6]);
    if (quality == 0) {
        parse.gga_valid = false;
        return;
    }

    parse.lat      = parse_coord(fields[2], fields[3]);
    parse.lon      = parse_coord(fields[4], fields[5]);
    parse.sats     = (uint8_t)atoi(fields[7]);
    parse.hdop     = (uint16_t)(atof(fields[8]) * 100);
    parse.alt      = (int32_t)(atof(fields[9]) * 1000); /* meters -> mm */
    parse.fix_type = (quality >= 2) ? 3 : 2;             /* DGPS=3D, GPS=2D */
    parse.gga_valid = true;
}

/* Parse $GPRMC / $GNRMC */
static void parse_rmc(char *fields[], int nfields)
{
    if (nfields < 12) return;

    parse.rmc_status = fields[2][0];
    if (parse.rmc_status != 'A') {
        parse.rmc_valid = false;
        return;
    }

    /* Speed: knots -> cm/s  (1 knot = 51.4444 cm/s) */
    double knots = atof(fields[7]);
    parse.speed = (uint16_t)(knots * 51.4444);

    /* Heading */
    parse.heading = (uint16_t)(atof(fields[8]) * 100);

    /* Timestamp */
    parse.timestamp = parse_datetime(fields[9], fields[1]);

    parse.rmc_valid = true;
}

/* Verify NMEA checksum: *XX at end */
static bool nmea_checksum_ok(const char *line, int len)
{
    /* Find '*' */
    int star_pos = -1;
    for (int i = len - 1; i >= 0; i--) {
        if (line[i] == '*') { star_pos = i; break; }
    }
    if (star_pos < 1 || star_pos + 2 >= len) return false;

    /* XOR all bytes between '$' and '*' */
    uint8_t calc = 0;
    for (int i = 1; i < star_pos; i++) {
        calc ^= (uint8_t)line[i];
    }

    /* Parse expected */
    char hex[3] = { line[star_pos + 1], line[star_pos + 2], 0 };
    uint8_t expected = (uint8_t)strtoul(hex, NULL, 16);

    return calc == expected;
}

/* Process a complete NMEA line */
static void process_nmea_line(char *line, int len)
{
    if (len < 10 || line[0] != '$') return;
    if (!nmea_checksum_ok(line, len)) return;

    /* Strip checksum for field parsing */
    for (int i = 0; i < len; i++) {
        if (line[i] == '*') { line[i] = '\0'; break; }
    }

    char *fields[20];
    int nfields = nmea_split(line, fields, 20);

    if (strcmp(fields[0], "$GPGGA") == 0 || strcmp(fields[0], "$GNGGA") == 0) {
        parse_gga(fields, nfields);
    } else if (strcmp(fields[0], "$GPRMC") == 0 || strcmp(fields[0], "$GNRMC") == 0) {
        parse_rmc(fields, nfields);
    } else {
        return; /* Ignore other sentences */
    }

    /* When we have both GGA + RMC valid, combine into a fix */
    if (!parse.gga_valid || !parse.rmc_valid) return;

    raw_count++;

    /* --- Apply filter --- */
    if (parse.fix_type < filter_cfg.min_fix_type) return;
    if (parse.sats < filter_cfg.min_sats) return;
    if (parse.hdop > filter_cfg.min_hdop && filter_cfg.min_hdop > 0) return;

    /* Static speed filter: below threshold, zero out speed */
    uint16_t speed = parse.speed;
    uint8_t flags = 0x01; /* Bit 0: fix valid */
    if (parse.fix_type == 3) flags |= 0x02; /* DGPS */

    if (speed <= filter_cfg.static_speed) {
        speed = 0;
    } else {
        flags |= 0x04; /* speed valid */
    }

    /* Build filtered fix */
    gps_fix_t fix = {
        .lat       = parse.lat,
        .lon       = parse.lon,
        .alt       = parse.alt,
        .speed     = speed,
        .heading   = parse.heading,
        .sats      = parse.sats,
        .fix_type  = parse.fix_type,
        .hdop      = parse.hdop,
        .timestamp = parse.timestamp,
        .flags     = flags,
        .reserved  = 0,
    };

    portENTER_CRITICAL(&fix_lock);
    last_fix  = fix;
    fix_valid = true;
    portEXIT_CRITICAL(&fix_lock);

    /* Log every 10th fix to avoid spam */
    if (filtered_count % 10 == 1) {
        ESP_LOGI(TAG, "Fix #%d: lat=%.6f lon=%.6f alt=%d sats=%d hdop=%.1f",
                 (int)filtered_count, fix.lat / 1e7, fix.lon / 1e7,
                 (int)fix.alt, (int)fix.sats, fix.hdop / 10.0f);
    }

    filtered_count++;

    /* Reset parse state for next cycle */
    parse.gga_valid = false;
    parse.rmc_valid = false;
}

/* GPS UART RX task */
static void gps_rx_task(void *arg)
{
    uint8_t buf[128];

    for (;;) {
        int len = uart_read_bytes(GPS_UART_NUM, buf, sizeof(buf), pdMS_TO_TICKS(50));
        if (len <= 0) continue;

        for (int i = 0; i < len; i++) {
            char c = (char)buf[i];

            if (c == '$') {
                /* Start of sentence */
                nmea_idx = 0;
            }

            if (nmea_idx < NMEA_MAX_LEN) {
                nmea_buf[nmea_idx++] = c;
            }

            if (c == '\n' && nmea_idx > 1) {
                nmea_buf[nmea_idx] = '\0';
                /* Strip trailing \r\n */
                while (nmea_idx > 0 && (nmea_buf[nmea_idx - 1] == '\r' || nmea_buf[nmea_idx - 1] == '\n')) {
                    nmea_buf[--nmea_idx] = '\0';
                }
                process_nmea_line(nmea_buf, nmea_idx);
                raw_count++;
                if (raw_count == 1) {
                    ESP_LOGI(TAG, "First NMEA: %s", nmea_buf);
                }
                nmea_idx = 0;
            }
        }
    }
}

/* --- Public API --- */

void gps_uart_init(const gps_filter_cfg_t *cfg)
{
    if (cfg) {
        filter_cfg = *cfg;
    } else {
        /* Defaults */
        filter_cfg.min_hdop     = 500;   /* 5.0 */
        filter_cfg.min_sats     = 4;
        filter_cfg.min_fix_type = 2;
        filter_cfg.static_speed = 50;    /* 0.5 m/s = ~1.8 km/h */
    }

    memset(&parse, 0, sizeof(parse));

    uart_config_t uart_cfg = {
        .baud_rate  = GPS_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(GPS_UART_NUM, 1024, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(GPS_UART_NUM, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(GPS_UART_NUM, GPS_TX_PIN, GPS_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    xTaskCreate(gps_rx_task, "gps_rx", 3072, NULL, 6, NULL);

    ESP_LOGI(TAG, "UART%d init: %d baud, TX=%d RX=%d, filter: hdop<%u sats>=%u fix>=%u",
             GPS_UART_NUM, GPS_BAUD, GPS_TX_PIN, GPS_RX_PIN,
             filter_cfg.min_hdop, filter_cfg.min_sats, filter_cfg.min_fix_type);
}

bool gps_get_last_fix(gps_fix_t *out)
{
    portENTER_CRITICAL(&fix_lock);
    bool valid = fix_valid;
    if (valid) *out = last_fix;
    portEXIT_CRITICAL(&fix_lock);
    return valid;
}

uint32_t gps_get_raw_count(void)      { return raw_count; }
uint32_t gps_get_filtered_count(void) { return filtered_count; }
