/* Protocol UART — communication with S3 via binary protocol */

#include "proto_uart.h"
#include "pinout.h"

#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include <string.h>

static const char *TAG = "proto";

static proto_parser_t     parser;
static proto_rx_handler_t rx_handler = NULL;
static SemaphoreHandle_t  tx_mutex   = NULL;

/* Callback du parser — appele dans la tache RX */
static void on_frame_parsed(uint8_t type, const uint8_t *payload, uint16_t length)
{
    if (rx_handler) {
        rx_handler(type, payload, length);
    }
}

/* Tache RX — lit le ring buffer UART par blocs */
static void proto_rx_task(void *arg)
{
    uint8_t buf[128];

    for (;;) {
        int len = uart_read_bytes(PROTO_UART_NUM, buf, sizeof(buf), pdMS_TO_TICKS(20));
        if (len > 0) {
            for (int i = 0; i < len; i++) {
                proto_parser_feed(&parser, buf[i], on_frame_parsed);
            }
        }
    }
}

void proto_uart_init(void)
{
    uart_config_t uart_cfg = {
        .baud_rate  = PROTO_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(PROTO_UART_NUM, PROTO_RX_BUF, 256, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(PROTO_UART_NUM, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(PROTO_UART_NUM, PROTO_TX_PIN, PROTO_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    proto_parser_init(&parser);

    tx_mutex = xSemaphoreCreateMutex();

    xTaskCreate(proto_rx_task, "proto_rx", 3072, NULL, 5, NULL);

    ESP_LOGI(TAG, "UART%d init: %d baud, TX=%d RX=%d, RX buf=%d",
             PROTO_UART_NUM, PROTO_BAUD, PROTO_TX_PIN, PROTO_RX_PIN, PROTO_RX_BUF);
}

void proto_set_rx_handler(proto_rx_handler_t handler)
{
    rx_handler = handler;
}

void proto_send(uint8_t type, const uint8_t *payload, uint16_t length)
{
    if (!tx_mutex) return;  /* proto UART not initialized */

    uint8_t frame[PROTO_MAX_FRAME];
    size_t frame_len = proto_build_frame(frame, type, payload, length);

    xSemaphoreTake(tx_mutex, portMAX_DELAY);
    uart_write_bytes(PROTO_UART_NUM, frame, frame_len);
    xSemaphoreGive(tx_mutex);
}

/* --- Helpers types --- */

void proto_send_gps_fix(const gps_fix_t *fix)
{
    proto_send(MSG_GPS_FIX, (const uint8_t *)fix, sizeof(gps_fix_t));
}

void proto_send_lora_rx(const uint8_t *aprs_pkt, uint16_t pkt_len,
                        int16_t rssi, int8_t snr, uint8_t cr, uint16_t freq_err)
{
    /* lora_rx_t header (8 bytes) + pkt data */
    uint8_t buf[8 + 512];
    if (pkt_len > 512 - 8) pkt_len = 512 - 8;

    lora_rx_t *hdr = (lora_rx_t *)buf;
    hdr->rssi     = rssi;
    hdr->snr      = snr;
    hdr->cr       = cr;
    hdr->pkt_len  = pkt_len;
    hdr->freq_err = freq_err;
    memcpy(hdr->data, aprs_pkt, pkt_len);

    proto_send(MSG_LORA_RX, buf, 8 + pkt_len);
}

void proto_send_tx_ack(uint8_t status, uint16_t irq_flags)
{
    lora_tx_ack_t ack = {
        .status    = status,
        .reserved  = 0,
        .irq_flags = irq_flags,
    };
    proto_send(MSG_LORA_TX_ACK, (const uint8_t *)&ack, sizeof(ack));
}

void proto_send_status(const status_t *st)
{
    proto_send(MSG_STATUS, (const uint8_t *)st, sizeof(status_t));
}

void proto_send_error(uint16_t code, uint16_t detail)
{
    error_t err = { .code = code, .detail = detail };
    proto_send(MSG_ERROR, (const uint8_t *)&err, sizeof(err));
}
