/* APRS packet builder
 *
 * Builds standard APRS position reports for LoRa transmission.
 * Format: AX.25 header (3 bytes) + TNC2 text
 *
 * Position format (uncompressed): !DDMM.MMN/DDDMM.MME>CCC/SSS comment
 *   DD = degrees, MM.MM = minutes, N/S E/W
 *   CCC = course, SSS = speed (knots)
 */

#include "aprs_packet.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* AX.25 header for LoRa APRS: control=0x3C, PID=0xFF, protocol=0x01 */
#define AX25_HEADER_0  0x3C
#define AX25_HEADER_1  0xFF
#define AX25_HEADER_2  0x01

/* Convert lat (x1e7) to APRS format: DDMM.MMN */
static int format_lat(char *buf, int32_t lat_e7)
{
    char ns = (lat_e7 >= 0) ? 'N' : 'S';
    if (lat_e7 < 0) lat_e7 = -lat_e7;

    uint32_t deg = (uint32_t)(lat_e7 / 10000000);
    uint32_t rem = (uint32_t)(lat_e7 % 10000000);
    /* Convert remainder to minutes: rem / 1e7 * 60 = rem * 60 / 1e7 */
    uint32_t min_x100 = (uint32_t)((uint64_t)rem * 6000 / 10000000);

    return sprintf(buf, "%02lu%02lu.%02lu%c",
                   (unsigned long)deg, (unsigned long)(min_x100 / 100), (unsigned long)(min_x100 % 100), ns);
}

/* Convert lon (x1e7) to APRS format: DDDMM.MME */
static int format_lon(char *buf, int32_t lon_e7)
{
    char ew = (lon_e7 >= 0) ? 'E' : 'W';
    if (lon_e7 < 0) lon_e7 = -lon_e7;

    uint32_t deg = (uint32_t)(lon_e7 / 10000000);
    uint32_t rem = (uint32_t)(lon_e7 % 10000000);
    uint32_t min_x100 = (uint32_t)((uint64_t)rem * 6000 / 10000000);

    return sprintf(buf, "%03lu%02lu.%02lu%c",
                   (unsigned long)deg, (unsigned long)(min_x100 / 100), (unsigned long)(min_x100 % 100), ew);
}

int aprs_build_position(uint8_t *buf, int buf_size,
                        const aprs_config_t *cfg,
                        const gps_fix_t *fix)
{
    if (buf_size < 10) return 0;

    /* AX.25 header */
    buf[0] = AX25_HEADER_0;
    buf[1] = AX25_HEADER_1;
    buf[2] = AX25_HEADER_2;

    /* TNC2 format: SRC>DST,PATH:!lat/lon>CSE/SPD comment */
    char *p = (char *)buf + 3;
    int remaining = buf_size - 3;

    /* Source */
    int n;
    if (cfg->ssid > 0) {
        n = snprintf(p, remaining, "%s-%u", cfg->callsign, cfg->ssid);
    } else {
        n = snprintf(p, remaining, "%s", cfg->callsign);
    }
    p += n; remaining -= n;

    /* Destination */
    const char *dest = cfg->dest[0] ? cfg->dest : "APLT00";
    n = snprintf(p, remaining, ">%s", dest);
    p += n; remaining -= n;

    /* Path */
    if (cfg->path[0]) {
        n = snprintf(p, remaining, ",%s", cfg->path);
        p += n; remaining -= n;
    }

    /* Data type identifier: ! = position without timestamp */
    n = snprintf(p, remaining, ":!");
    p += n; remaining -= n;

    /* Latitude */
    char lat_str[12];
    format_lat(lat_str, fix->lat);
    n = snprintf(p, remaining, "%s", lat_str);
    p += n; remaining -= n;

    /* Symbol table */
    n = snprintf(p, remaining, "%c", cfg->symbol_table ? cfg->symbol_table : '/');
    p += n; remaining -= n;

    /* Longitude */
    char lon_str[12];
    format_lon(lon_str, fix->lon);
    n = snprintf(p, remaining, "%s", lon_str);
    p += n; remaining -= n;

    /* Symbol */
    n = snprintf(p, remaining, "%c", cfg->symbol ? cfg->symbol : '>');
    p += n; remaining -= n;

    /* Course/Speed (if moving) */
    if (fix->flags & 0x04) {  /* speed valid */
        uint16_t course = fix->heading / 100;  /* heading x100 -> degrees */
        /* speed: cm/s -> knots (1 knot = 51.4444 cm/s) */
        uint16_t speed_kn = (uint16_t)((float)fix->speed / 51.4444f);
        n = snprintf(p, remaining, "%03u/%03u", course, speed_kn);
        p += n; remaining -= n;
    }

    /* Altitude (if available, in APRS /A= format, feet) */
    if (fix->alt != 0) {
        int32_t alt_ft = (int32_t)((float)fix->alt / 304.8f);  /* mm -> feet */
        n = snprintf(p, remaining, "/A=%06d", (int)alt_ft);
        p += n; remaining -= n;
    }

    /* Comment */
    if (cfg->comment[0]) {
        n = snprintf(p, remaining, " %s", cfg->comment);
        p += n; remaining -= n;
    }

    return (int)(p - (char *)buf);
}

int aprs_build_status(uint8_t *buf, int buf_size,
                      const aprs_config_t *cfg,
                      const char *status_text)
{
    if (buf_size < 10) return 0;

    buf[0] = AX25_HEADER_0;
    buf[1] = AX25_HEADER_1;
    buf[2] = AX25_HEADER_2;

    char *p = (char *)buf + 3;
    int remaining = buf_size - 3;

    int n;
    if (cfg->ssid > 0) {
        n = snprintf(p, remaining, "%s-%u", cfg->callsign, cfg->ssid);
    } else {
        n = snprintf(p, remaining, "%s", cfg->callsign);
    }
    p += n; remaining -= n;

    const char *dest = cfg->dest[0] ? cfg->dest : "APLT00";
    n = snprintf(p, remaining, ">%s", dest);
    p += n; remaining -= n;

    if (cfg->path[0]) {
        n = snprintf(p, remaining, ",%s", cfg->path);
        p += n; remaining -= n;
    }

    /* Status message type: > */
    n = snprintf(p, remaining, ":>%s", status_text);
    p += n; remaining -= n;

    return (int)(p - (char *)buf);
}
