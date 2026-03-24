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
    #define HAS_TCXO
    #define RADIO_VCC_PIN       40  // (RADIO_LDO_EN_PIN) // HIGH = turn on the radio, LOW = turns off the radio
    #define RADIO_SCLK_PIN      13
    #define RADIO_MISO_PIN      12
    #define RADIO_MOSI_PIN      11
    #define RADIO_CS_PIN        15
    #define RADIO_RST_PIN       3
    #define RADIO_DIO1_PIN      1
    #define RADIO_BUSY_PIN      38
    #define RADIO_RXEN          21  // (RADIO_CTRL_PIN) // HIGH = turn on LNA (to Rx Data), LOW = turns off LNA (to Tx data)

    //  Display
    #undef  OLED_SDA
    #undef  OLED_SCL
    #undef  OLED_RST

    #define OLED_SDA            8
    #define OLED_SCL            9
    #define OLED_RST            -1

    //  GPS
    #define GPS_RX              6
    #define GPS_TX              5
    #define GPS_PPS             7
    #define GPS_WAKEUP          16

    //  OTHER
    #define BUTTON_PIN          0 // The middle button GPIO on the T-Beam
    #define BUTTON2_PIN         17 // ???? botton customizable? para que?

    #define BATTERY_PIN         4

    //ON_BOARD_LED 18

    #define TEMP_PIN            14  // NTC Temperature sensor
    #define FAN_CTRL_PIN        41

    //#define HAS_AXP2101     // ?????????????


#endif