/* APRS packet builder — constructs AX.25 UI frames for LoRa APRS */

#ifndef APRS_PACKET_H
#define APRS_PACKET_H

#include "uart_proto.h"
#include <stdint.h>

/* APRS config */
typedef struct {
    char callsign[10];     /* e.g. "F4XXX" */
    uint8_t ssid;          /* 0-15 */
    char symbol_table;     /* '/' or '\\' */
    char symbol;           /* e.g. '>' for car, '[' for jogger */
    char comment[40];      /* Beacon comment */
    char dest[10];         /* Destination callsign (default "APLT00") */
    char path[20];         /* Digipeater path (e.g. "WIDE1-1") */
} aprs_config_t;

/* Build an APRS position packet from GPS fix.
 * Returns total length written to buf (including AX.25 header 0x3C 0xFF 0x01).
 * buf must be at least 256 bytes. */
int aprs_build_position(uint8_t *buf, int buf_size,
                        const aprs_config_t *cfg,
                        const gps_fix_t *fix);

/* Build a raw APRS status/message packet.
 * Returns total length. */
int aprs_build_status(uint8_t *buf, int buf_size,
                      const aprs_config_t *cfg,
                      const char *status_text);

#endif /* APRS_PACKET_H */
