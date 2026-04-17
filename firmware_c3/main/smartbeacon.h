/* SmartBeaconing — logique de decision TX autonome */

#ifndef SMARTBEACON_H
#define SMARTBEACON_H

#include "uart_proto.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t  slow_rate;     /* Secondes / 10  (ex: 180 = 30 min) */
    uint8_t  fast_rate;     /* Secondes       (ex: 60) */
    uint8_t  min_speed;     /* km/h           (ex: 5) */
    uint8_t  max_speed;     /* km/h           (ex: 90) */
    uint16_t min_turn_angle;/* Degres x10     (ex: 280 = 28.0 deg) */
    uint16_t turn_slope;    /* Slope          (ex: 240) */
} smartbeacon_cfg_t;

/* Initialise la config SmartBeaconing */
void smartbeacon_init(const smartbeacon_cfg_t *cfg);

/* Met a jour la config (MSG_CONFIG depuis S3) */
void smartbeacon_update_cfg(const smartbeacon_cfg_t *cfg);

/* Evalue si on doit transmettre maintenant.
 * Retourne true si TX requis. Appeler apres chaque GPS fix filtre. */
bool smartbeacon_should_tx(const gps_fix_t *fix);

/* A appeler apres un TX reussi pour reset les timers */
void smartbeacon_tx_done(void);

#endif /* SMARTBEACON_H */
