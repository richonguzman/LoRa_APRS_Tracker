/* Protocol UART — communication with S3 */

#ifndef PROTO_UART_H
#define PROTO_UART_H

#include "uart_proto.h"
#include <stdint.h>

/* Initialise UART0 (remapped to GPIO 20/21) + lance la tache RX */
void proto_uart_init(void);

/* Envoie une trame vers le S3 (thread-safe) */
void proto_send(uint8_t type, const uint8_t *payload, uint16_t length);

/* Helpers typés */
void proto_send_gps_fix(const gps_fix_t *fix);
void proto_send_lora_rx(const uint8_t *aprs_pkt, uint16_t pkt_len,
                        int16_t rssi, int8_t snr, uint8_t cr, uint16_t freq_err);
void proto_send_tx_ack(uint8_t status, uint16_t irq_flags);
void proto_send_status(const status_t *st);
void proto_send_error(uint16_t code, uint16_t detail);

/* Callback enregistré pour les messages entrants (S3 -> C3) */
typedef void (*proto_rx_handler_t)(uint8_t type, const uint8_t *payload, uint16_t length);
void proto_set_rx_handler(proto_rx_handler_t handler);

#endif /* PROTO_UART_H */
