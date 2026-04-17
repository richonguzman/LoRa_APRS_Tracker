/* Protocole UART S3 <-> C3 — LoRa APRS Tracker
 *
 * Header commun partagé entre ESP32-S3 (Arduino) et ESP32-C3 (ESP-IDF).
 * Protocole binaire full-duplex, CRC-16/CCITT.
 */

#ifndef UART_PROTO_H
#define UART_PROTO_H

#include <stdint.h>
#include <stddef.h>

#define PROTO_SYNC          0xAA
#define PROTO_MAX_PAYLOAD   512
#define PROTO_HEADER_SIZE   4       // SYNC + TYPE + LEN(2)
#define PROTO_CRC_SIZE      2
#define PROTO_MAX_FRAME     (PROTO_HEADER_SIZE + PROTO_MAX_PAYLOAD + PROTO_CRC_SIZE)

// --- Message types C3 -> S3 ---
#define MSG_GPS_FIX         0x01
#define MSG_LORA_RX         0x02
#define MSG_LORA_TX_ACK     0x11
#define MSG_STATUS          0x30
#define MSG_ERROR           0x3F

// --- Message types S3 -> C3 ---
#define MSG_LORA_TX_REQ     0x10
#define MSG_CONFIG          0x20
#define MSG_CONFIG_ACK      0x21

// --- TX ACK status codes ---
#define TX_OK               0x00
#define TX_TIMEOUT          0x01
#define TX_SPI_ERROR        0x02
#define TX_BUSY             0x03

// --- Error codes ---
#define ERR_LORA_INIT       0x0001
#define ERR_LORA_SPI        0x0002
#define ERR_LORA_XOSC       0x0003
#define ERR_GPS_NO_FIX      0x0010
#define ERR_GPS_UART        0x0011
#define ERR_PROTO_CRC       0x0020
#define ERR_PROTO_OVERFLOW  0x0021

// --- Payload structures (packed, little-endian) ---

typedef struct __attribute__((packed)) {
    int32_t  lat;           // Latitude x 1e7
    int32_t  lon;           // Longitude x 1e7
    int32_t  alt;           // Altitude mm
    uint16_t speed;         // Vitesse cm/s
    uint16_t heading;       // Cap x 100
    uint8_t  sats;
    uint8_t  fix_type;      // 0=no fix, 2=2D, 3=3D
    uint16_t hdop;          // HDOP x 100
    uint32_t timestamp;     // UTC epoch seconds
    uint8_t  flags;         // Bit 0: fix valid, Bit 1: DGPS, Bit 2: speed valid
    uint8_t  reserved;
} gps_fix_t;                // 30 bytes

typedef struct __attribute__((packed)) {
    int16_t  rssi;          // RSSI dBm x 10
    int8_t   snr;           // SNR dB x 4
    uint8_t  cr;            // Coding rate (5-8)
    uint16_t pkt_len;       // APRS packet length
    uint16_t freq_err;      // Frequency error Hz
    uint8_t  data[];        // Raw APRS packet (pkt_len bytes)
} lora_rx_t;                // 8 + pkt_len bytes

typedef struct __attribute__((packed)) {
    uint16_t pkt_len;
    uint8_t  data[];        // Raw APRS packet
} lora_tx_req_t;            // 2 + pkt_len bytes

typedef struct __attribute__((packed)) {
    uint8_t  status;        // TX_OK, TX_TIMEOUT, TX_SPI_ERROR, TX_BUSY
    uint8_t  reserved;
    uint16_t irq_flags;     // SX1262 IRQ register
} lora_tx_ack_t;            // 4 bytes

typedef struct __attribute__((packed)) {
    uint32_t freq;          // Hz
    int8_t   tx_power;      // dBm (-9 to +22)
    uint8_t  sf;            // Spreading Factor (7-12)
    uint8_t  bw;            // Bandwidth index (7=125k, 8=250k, 9=500k)
    uint8_t  cr;            // Coding Rate (5-8)
    uint16_t preamble;      // Preamble symbols
    uint8_t  sb_active;     // SmartBeaconing on/off
    uint8_t  sb_slow_rate;  // Slow rate (seconds / 10)
    uint8_t  sb_fast_rate;  // Fast rate (seconds)
    uint8_t  sb_min_speed;  // km/h
    uint8_t  sb_max_speed;  // km/h
} config_t;                 // 16 bytes

typedef struct __attribute__((packed)) {
    uint8_t  gps_fix;       // 0=no fix, 2=2D, 3=3D
    uint8_t  gps_sats;
    uint8_t  lora_state;    // 0=idle, 1=RX, 2=TX, 3=CAD, 4=sleep
    uint8_t  flags;         // Bit 0: RX pending, Bit 1: TX in progress
    uint16_t rx_count;
    uint16_t tx_count;
    uint16_t error_count;
    uint16_t uptime;        // Minutes (max ~45 days)
} status_t;                 // 12 bytes

typedef struct __attribute__((packed)) {
    uint16_t code;
    uint16_t detail;
} error_t;                  // 4 bytes

// --- Parser ---

typedef enum {
    PARSE_IDLE,
    PARSE_TYPE,
    PARSE_LEN_LO,
    PARSE_LEN_HI,
    PARSE_PAYLOAD,
    PARSE_CRC_LO,
    PARSE_CRC_HI
} parse_state_t;

typedef struct {
    parse_state_t state;
    uint8_t  type;
    uint16_t length;
    uint16_t payload_idx;
    uint8_t  payload[PROTO_MAX_PAYLOAD];
    uint8_t  crc_lo;
    uint16_t crc_error_count;
    uint16_t rx_frame_count;
} proto_parser_t;

typedef void (*proto_callback_t)(uint8_t type, const uint8_t *payload, uint16_t length);

// --- API ---

uint16_t proto_crc16(const uint8_t *data, size_t len);
void     proto_parser_init(proto_parser_t *p);
void     proto_parser_feed(proto_parser_t *p, uint8_t byte, proto_callback_t cb);
size_t   proto_build_frame(uint8_t *buf, uint8_t type, const uint8_t *payload, uint16_t length);

#endif // UART_PROTO_H
