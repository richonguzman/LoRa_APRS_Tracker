/* Copyright (C) 2025 Ricardo Guzman - CA2RXU
 *
 * This file is part of LoRa APRS Tracker.
 *
 * LoRa APRS Tracker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LoRa APRS Tracker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LoRa APRS Tracker. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef BOARD_PINOUT_H_
#define BOARD_PINOUT_H_

    //  LoRa Radio
    #define HAS_SX1262
    #define RADIO_SCLK_PIN      35
    #define RADIO_MISO_PIN      33
    #define RADIO_MOSI_PIN      34
    #define RADIO_CS_PIN        36
    #define RADIO_RST_PIN       47
    #define RADIO_DIO1_PIN      14
    #define RADIO_BUSY_PIN      48
    #define RADIO_WAKEUP_PIN    RADIO_DIO1_PIN
    #define WAKEUP_RADIO        GPIO_SEL_14

    //  Display
    #undef  OLED_SDA
    #undef  OLED_SCL
    #undef  OLED_RST

    #define HAS_TFT
    #define BOARD_BL_PIN        21

    //  GPS
    //#define HAS_GPS_CTRL
    #define GPS_RX              12
    #define GPS_TX              4
    #define GPS_PPS             13
    //#define GPS_RESET           35
    #define GPS_BAUDRATE        38400//9600//115200

    //  OTHER
    //#define BUTTON_PIN          0
    #define WAKEUP_BUTTON       GPIO_NUM_0
    #define BATTERY_PIN         1

    //  I2C Bus (BHI260, PCF85063, BQ25896, BQ27220, DRV2605, ES8311, XL9555)
    #define BOARD_I2C_SDA       3
    #define BOARD_I2C_SCL       2

    //  Display — ST7796 TFT 480x222 (landscape), shares SPI bus
    #define HAS_TFT
    #define DISP_WIDTH          480
    #define DISP_HEIGHT         222
    #define DISP_CS             38
    #define DISP_DC             37
    #define DISP_RST            -1
    #define DISP_BL             42
    #define DISP_MOSI           34
    #define DISP_MISO           33
    #define DISP_SCK            35

    //  Keyboard matrix
    #define HAS_KEYBOARD
    #define KB_INT              6
    #define KB_BACKLIGHT        46

    //  Rotary encoder + center button
    #define HAS_ROTARY_ENCODER
    #define ROTARY_A            40
    #define ROTARY_B            41
    #define ROTARY_C            7   // center push button
    #define BUTTON_PIN          ROTARY_C

    //  Power buttons
    #define POWER_KEY           0   // BOOT/wake button
    #define BOOT_KEY            9

    //  SD card (shares SPI bus)
    #define BOARD_SDCARD_CS     21

    //  NFC — ST25R3916 (shares SPI bus)
    #define NFC_CS              39
    #define NFC_INT             5

    //  Interrupt lines
    #define RTC_INT             1
    #define SENSOR_INT          8

    //  I2S Audio — ES8311 codec
    #define HAS_I2S
    #define I2S_WS              18
    #define I2S_SCK             11
    #define I2S_MCLK            10
    #define I2S_SDOUT           45
    #define I2S_SDIN            17

    //  XL9555 GPIO expander — controls peripheral power rails
    #define HAS_XL9555
    #define EXPANDS_DRV_EN      0   // haptic motor enable
    #define EXPANDS_AMP_EN      1   // audio amplifier enable
    #define EXPANDS_KB_RST      2   // keyboard reset
    #define EXPANDS_LORA_EN     3   // LoRa radio power
    #define EXPANDS_GPS_EN      4   // GPS power
    #define EXPANDS_NFC_EN      5   // NFC power
    #define EXPANDS_GPS_RST     7   // GPS reset
    #define EXPANDS_KB_EN       8   // keyboard enable
    #define EXPANDS_GPIO_EN     9
    #define EXPANDS_SD_DET      10  // SD card detect
    #define EXPANDS_SD_PULLEN   11
    #define EXPANDS_SD_EN       12  // SD card power

    //  Feature flags
    #define HAS_BQ27220         // fuel gauge (battery %)
    #define HAS_BQ25896         // charger IC
    #define HAS_PCF85063        // RTC
    #define HAS_DRV2605         // haptic motor

#endif
