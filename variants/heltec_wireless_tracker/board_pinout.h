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
    #define RADIO_SCLK_PIN      9
    #define RADIO_MISO_PIN      11
    #define RADIO_MOSI_PIN      10
    #define RADIO_CS_PIN        8
    #define RADIO_RST_PIN       12
    #define RADIO_DIO1_PIN      14
    #define RADIO_BUSY_PIN      13
    #define RADIO_WAKEUP_PIN    RADIO_DIO1_PIN
    #define WAKEUP_RADIO        GPIO_SEL_14

    //  Display
    #undef  OLED_SDA
    #undef  OLED_SCL
    #undef  OLED_RST

    #define HAS_TFT
    #define BOARD_BL_PIN        21

    //  GPS
    #define HAS_GPS_CTRL
    #define GPS_RX              34
    #define GPS_TX              33
    #define GPS_PPS             36
    #define GPS_RESET           35
    #define GPS_BAUDRATE        115200

    //  OTHER
    #define BUTTON_PIN          0
    #define WAKEUP_BUTTON       GPIO_NUM_0
    #define BATTERY_PIN         1
    #define ADC_CTRL            2   // HELTEC Wireless Tracker ADC_CTRL = HIGH powers the voltage divider to read BatteryPin. Only on V05 = V1.1
    #define VEXT_CTRL           3   // To turn on GPS and TFT

    #define BOARD_I2C_SDA       7
    #define BOARD_I2C_SCL       6

#endif