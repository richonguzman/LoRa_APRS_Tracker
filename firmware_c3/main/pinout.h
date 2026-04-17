/* GPIO allocation — ESP32-C3 custom PCB */

#ifndef PINOUT_H
#define PINOUT_H

/* GNSS UART (UART1) */
#define GPS_UART_NUM    UART_NUM_1
#define GPS_TX_PIN      0
#define GPS_RX_PIN      1
#define GPS_PPS_PIN     2
#define GPS_BAUD        9600

/* LoRa SPI (SPI2) */
#define LORA_SCK_PIN    4
#define LORA_MISO_PIN   5
#define LORA_MOSI_PIN   6
#define LORA_CS_PIN     7
#define LORA_RST_PIN    3
#define LORA_BUSY_PIN   8
#define LORA_DIO1_PIN   10

/* Protocol UART toward S3 (UART0 remapped — debug via USB JTAG) */
#define PROTO_UART_NUM  UART_NUM_0
#define PROTO_TX_PIN    21
#define PROTO_RX_PIN    20
#define PROTO_BAUD      460800
#define PROTO_RX_BUF    4096

/* Spare: GPIO 9, 11, 18, 19 */

#endif /* PINOUT_H */
