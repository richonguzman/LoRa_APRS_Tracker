#ifndef BOARD_PINOUT_H_
#define BOARD_PINOUT_H_

    //  LoRa Radio
    #define HAS_SX1268
    #define RADIO_SCLK_PIN      5
    #define RADIO_MISO_PIN      19
    #define RADIO_MOSI_PIN      27
    #define RADIO_CS_PIN        18
    #define RADIO_DIO0_PIN      26
    #define RADIO_RST_PIN       23
    #define RADIO_DIO1_PIN      33
    #define RADIO_BUSY_PIN      32

    //  Display
    #undef  OLED_SDA
    #undef  OLED_SCL
    #undef  OLED_RST

    #define OLED_SDA            21
    #define OLED_SCL            22
    #define OLED_RST            16

    //  GPS
    #define HAS_GPS_CTRL
    #define GPS_RX              12
    #define GPS_TX              34

    //  OTHER
    #define BUTTON_PIN          38  // The middle button GPIO on the T-Beam

    #define HAS_AXP192
    #define HAS_BT_CLASSIC

#endif