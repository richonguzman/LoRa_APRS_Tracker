/* GNSS UART — NMEA parser + fix filter */

#ifndef GPS_UART_H
#define GPS_UART_H

#include "uart_proto.h"
#include <stdbool.h>

/* Filtre GNSS */
typedef struct {
    uint16_t min_hdop;          /* HDOP x100 seuil max (ex: 500 = 5.0) */
    uint8_t  min_sats;          /* Nombre minimum de satellites */
    uint8_t  min_fix_type;      /* 2 = 2D, 3 = 3D */
    uint16_t static_speed;      /* cm/s en dessous = stationnaire */
} gps_filter_cfg_t;

/* Initialise UART1 + lance la tache de parsing NMEA */
void gps_uart_init(const gps_filter_cfg_t *filter_cfg);

/* Derniere position filtrée (thread-safe, copie atomique) */
bool gps_get_last_fix(gps_fix_t *out);

/* Nombre de fix bruts / filtrés depuis le boot */
uint32_t gps_get_raw_count(void);
uint32_t gps_get_filtered_count(void);

#endif /* GPS_UART_H */
