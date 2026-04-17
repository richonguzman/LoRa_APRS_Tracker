/* Protocole UART S3 <-> C3 — LoRa APRS Tracker
 *
 * Parser + frame builder. Code C pur, pas de dépendance Arduino/ESP-IDF.
 * Partagé tel quel entre S3 et C3.
 */

#include "uart_proto.h"

uint16_t proto_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
    }
    return crc;
}

void proto_parser_init(proto_parser_t *p) {
    p->state           = PARSE_IDLE;
    p->crc_error_count = 0;
    p->rx_frame_count  = 0;
}

void proto_parser_feed(proto_parser_t *p, uint8_t byte, proto_callback_t cb) {
    switch (p->state) {

    case PARSE_IDLE:
        if (byte == PROTO_SYNC)
            p->state = PARSE_TYPE;
        break;

    case PARSE_TYPE:
        p->type  = byte;
        p->state = PARSE_LEN_LO;
        break;

    case PARSE_LEN_LO:
        p->length = byte;
        p->state  = PARSE_LEN_HI;
        break;

    case PARSE_LEN_HI:
        p->length |= (uint16_t)byte << 8;
        if (p->length > PROTO_MAX_PAYLOAD) {
            p->state = PARSE_IDLE;          // Frame too large, resync
        } else if (p->length == 0) {
            p->payload_idx = 0;
            p->state = PARSE_CRC_LO;       // No payload, skip to CRC
        } else {
            p->payload_idx = 0;
            p->state = PARSE_PAYLOAD;
        }
        break;

    case PARSE_PAYLOAD:
        p->payload[p->payload_idx++] = byte;
        if (p->payload_idx >= p->length)
            p->state = PARSE_CRC_LO;
        break;

    case PARSE_CRC_LO:
        p->crc_lo = byte;
        p->state  = PARSE_CRC_HI;
        break;

    case PARSE_CRC_HI: {
        uint16_t rx_crc = p->crc_lo | ((uint16_t)byte << 8);

        // CRC over TYPE + LENGTH(2B) + PAYLOAD
        uint8_t hdr[3] = {
            p->type,
            (uint8_t)(p->length),
            (uint8_t)(p->length >> 8)
        };
        uint16_t calc_crc = proto_crc16(hdr, 3);

        // Continue CRC over payload bytes
        for (uint16_t i = 0; i < p->length; i++) {
            calc_crc ^= (uint16_t)p->payload[i] << 8;
            for (int j = 0; j < 8; j++)
                calc_crc = (calc_crc & 0x8000)
                    ? (calc_crc << 1) ^ 0x1021 : calc_crc << 1;
        }

        if (rx_crc == calc_crc) {
            p->rx_frame_count++;
            if (cb)
                cb(p->type, p->payload, p->length);
        } else {
            p->crc_error_count++;
        }
        p->state = PARSE_IDLE;
        break;
    }
    }
}

size_t proto_build_frame(uint8_t *buf, uint8_t type,
                         const uint8_t *payload, uint16_t length)
{
    buf[0] = PROTO_SYNC;
    buf[1] = type;
    buf[2] = (uint8_t)(length);
    buf[3] = (uint8_t)(length >> 8);

    if (length > 0 && payload) {
        for (uint16_t i = 0; i < length; i++)
            buf[4 + i] = payload[i];
    }

    // CRC over TYPE + LENGTH + PAYLOAD (bytes 1..3+length)
    uint16_t crc = proto_crc16(&buf[1], 3 + length);
    buf[4 + length]     = (uint8_t)(crc);
    buf[4 + length + 1] = (uint8_t)(crc >> 8);

    return 4 + length + 2;
}
