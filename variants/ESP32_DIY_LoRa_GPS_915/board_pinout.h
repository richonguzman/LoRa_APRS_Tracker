#ifndef BOARD_PINOUT_H_
#define BOARD_PINOUT_H_

    //  LoRa Radio
    #define HAS_SX1278
    #define RADIO_SCLK_PIN      5
    #define RADIO_MISO_PIN      19
    #define RADIO_MOSI_PIN      27
    #define RADIO_CS_PIN        18
    #define RADIO_RST_PIN       23
    #define RADIO_BUSY_PIN      26

    //  Display
    #undef  OLED_SDA
    #undef  OLED_SCL
    #undef  OLED_RST

    #define OLED_SDA            21
    #define OLED_SCL            22
    #define OLED_RST            16

    //  GPS
    #define GPS_RX              12
    #define GPS_TX              34

    //  OTHER
    #define BUTTON_PIN          15
    #define BATTERY_PIN         35  //LoRa32 Battery PIN 100k/100k

    #define HAS_BT_CLASSIC

#endif