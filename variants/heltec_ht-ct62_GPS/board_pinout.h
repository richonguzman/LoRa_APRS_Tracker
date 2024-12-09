#ifndef BOARD_PINOUT_H_
#define BOARD_PINOUT_H_

    //  LoRa Radio
    #define HAS_SX1262
    #define RADIO_SCLK_PIN      10
    #define RADIO_MISO_PIN      6
    #define RADIO_MOSI_PIN      7
    #define RADIO_CS_PIN        8
    #define RADIO_RST_PIN       5
    #define RADIO_DIO1_PIN      3
    #define RADIO_BUSY_PIN      4

    //  Display
    #undef  OLED_SDA
    #undef  OLED_SCL
    #undef  OLED_RST

    #define OLED_SDA            18
    #define OLED_SCL            19
    #define OLED_RST            -1

    //  GPS
    #define GPS_RX              0
    #define GPS_TX              1

#endif