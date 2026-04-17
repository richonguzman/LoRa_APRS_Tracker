/* LoRa radio abstraction — C API over RadioLib SX1262 */

#ifndef LORA_RADIO_H
#define LORA_RADIO_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Radio config (matches protocol config_t fields) */
typedef struct {
    uint32_t freq;          /* Hz */
    int8_t   tx_power;      /* dBm (-9 to +22) */
    uint8_t  sf;            /* Spreading Factor (7-12) */
    uint8_t  bw;            /* Bandwidth index (7=125k, 8=250k, 9=500k) */
    uint8_t  cr;            /* Coding Rate (5-8) */
    uint16_t preamble;      /* Preamble symbols */
} lora_config_t;

/* Callback for received packets */
typedef void (*lora_rx_cb_t)(const uint8_t *data, uint16_t len,
                             int16_t rssi, int8_t snr, uint16_t freq_err);

/* Initialize SPI + SX1262 with given config. Returns 0 on success. */
int lora_init(const lora_config_t *cfg);

/* Reconfigure radio (frequency, power, etc.) without full re-init */
int lora_reconfigure(const lora_config_t *cfg);

/* Transmit a raw packet (blocking). Returns 0 on success. */
int lora_transmit(const uint8_t *data, uint16_t len);

/* Start continuous RX. Received packets delivered via callback. */
void lora_start_rx(lora_rx_cb_t cb);

/* Check if a packet has been received (called from main loop).
 * Returns true if a packet was processed. */
bool lora_check_rx(void);

/* Get current radio state: 0=idle, 1=RX, 2=TX */
uint8_t lora_get_state(void);

#ifdef __cplusplus
}
#endif

#endif /* LORA_RADIO_H */
