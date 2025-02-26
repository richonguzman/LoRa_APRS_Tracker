#ifndef BOARD_PINOUT_H_
#define BOARD_PINOUT_H_

    //  LoRa Radio
    #define HAS_SX1262
    #define RADIO_SCLK_PIN      40
    #define RADIO_MISO_PIN      38
    #define RADIO_MOSI_PIN      41
    #define RADIO_CS_PIN        9
    #define RADIO_RST_PIN       17
    #define RADIO_DIO1_PIN      45
    #define RADIO_BUSY_PIN      13

    //  Display
    #undef  OLED_SDA
    #undef  OLED_SCL
    #undef  OLED_RST

    #define HAS_TFT

    //  GPS
    #define GPS_RX              43
    #define GPS_TX              44

    //  OTHER
    #define BATTERY_PIN         4

    #define BOARD_POWERON       10
    #define BOARD_SDCARD_CS     39
    #define BOARD_BL_PIN        42

    #define BOARD_I2C_SDA       18
    #define BOARD_I2C_SCL       8

    //  JOYSTICK
    #define HAS_JOYSTICK
    #define JOYSTICK_CENTER     0
    #define BUTTON_PIN          JOYSTICK_CENTER
    #define JOYSTICK_UP         3   // G S1
    #define JOYSTICK_DOWN       15  // G S3
    #define JOYSTICK_LEFT       1   // G S4
    #define JOYSTICK_RIGHT      2   // G S2

    #define HAS_TOUCHSCREEN
    #define HAS_I2S
    #define DAC_I2S_WS          5
    #define DAC_I2S_DOUT        6
    #define DAC_I2S_BCK         7
    #define SPK_I2S_PORT        I2S_NUM_0
    #define MIC_I2S_SAMPLE_RATE 16000
    #define MIC_I2S_PORT        I2S_NUM_1

#endif