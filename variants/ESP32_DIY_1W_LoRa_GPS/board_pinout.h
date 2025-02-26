#ifndef BOARD_PINOUT_H_
#define BOARD_PINOUT_H_

    //  LoRa Radio
    #define HAS_SX1268
    #define HAS_1W_LORA
    #define RADIO_SCLK_PIN      18
    #define RADIO_MISO_PIN      19
    #define RADIO_MOSI_PIN      23
    #define RADIO_CS_PIN        5
    #define RADIO_RST_PIN       27
    #define RADIO_DIO1_PIN      12
    #define RADIO_BUSY_PIN      14
    #define RADIO_RXEN          32
    #define RADIO_TXEN          25

    //  Display
    #undef  OLED_SDA
    #undef  OLED_SCL
    #undef  OLED_RST

    #define OLED_SDA            21
    #define OLED_SCL            22
    #define OLED_RST            -1

    //  GPS
    #define GPS_RX              17
    #define GPS_TX              16

    //  OTHER
    #define BUTTON_PIN          15
    #define BATTERY_PIN         35

    #define HAS_BT_CLASSIC

#endif