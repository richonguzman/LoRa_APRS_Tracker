/* Copyright (C) 2025 Ricardo Guzman - CA2RXU
 * 
 * This file is part of LoRa APRS Tracker.
 * 
 * LoRa APRS Tracker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or 
 * (at your option) any later version.
 */

#ifndef BOARD_PINOUT_H_
#define BOARD_PINOUT_H_

    //  LoRa Radio (SX1262 on CrowPanel external module)
    #define HAS_SX1262
    #define RADIO_SCLK_PIN      10
    #define RADIO_MISO_PIN      9
    #define RADIO_MOSI_PIN      3
    #define RADIO_CS_PIN        0U  // NSS (0U prevents ambiguity with null pointer)
    #define RADIO_RST_PIN       2   // Shared with TFT_RST
    #define RADIO_DIO1_PIN      1
    #define RADIO_DIO2_PIN      RADIOLIB_NC  // Not connected on CrowPanel
    #define RADIO_DIO3_PIN      RADIOLIB_NC  // Not connected on CrowPanel
    #define RADIO_BUSY_PIN      46

    //  Display
    #undef  OLED_SDA
    #undef  OLED_SCL
    #undef  OLED_RST

    #define HAS_TFT
    #define HAS_TOUCHSCREEN

    //  GPS (Placeholder pins - map to your external UART module)
    #define GPS_RX              17 // TODO: Configure for your wiring
    #define GPS_TX              18 // TODO: Configure for your wiring
    #define GPS_BAUDRATE        9600

    //  Audio
    #define BUZZER_PIN          8   // Direct PWM buzzer (no VCC pin needed)

    //  OTHER
    // Battery measurement is undefined (IO4 is SD_MISO)
    #define BATTERY_PIN         -1

    // I2C fuel gauge (MAX17048) on external I2C bus (shared with GT911 touch)
    #define HAS_FUEL_GAUGE_I2C
    #define FUEL_GAUGE_I2C_SDA  15
    #define FUEL_GAUGE_I2C_SCL  16

    // SD Card
    #define BOARD_SDCARD_CS     7
    #define BOARD_SDCARD_MOSI   6
    #define BOARD_SDCARD_MISO   4
    #define BOARD_SDCARD_SCK    5

    #define BUTTON_PIN          -1 // IO0 is used by LoRa CS, disabling boot button as input

    #define BOARD_I2C_SDA       15  // I2C SDA for CrowPanel Touch GT911
    #define BOARD_I2C_SCL       16  // I2C SCL for CrowPanel Touch GT911

#endif