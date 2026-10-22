/* Copyright (C) 2026 Ricardo Guzman - CA2RXU
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
    #define HAS_SX1278
    #define RADIO_VCC_PIN       16  // (RADIO_LDO_EN_PIN) // HIGH = turn on the radio, LOW = turns off the radio
    #define RADIO_SCLK_PIN      12
    #define RADIO_MISO_PIN      13
    #define RADIO_MOSI_PIN      11
    #define RADIO_CS_PIN        1
    #define RADIO_RST_PIN       18
    #define RADIO_BUSY_PIN      14  // DIO0
    #define RADIO_RXEN          39  // (RADIO_CTRL_PIN) // HIGH = turn on LNA (to Rx Data), LOW = turns off LNA (to Tx data)
    #define RADIO_MAX_POWER     10  // SX1278 drives the ~5W PA: LilyGo spec +10dBm (never above +12dBm)
    #define RADIO_MIN_FREQ      144000000   // BPF + PA only for 2m band: Tx is blocked outside 144-148 MHz
    #define RADIO_MAX_FREQ      148000000

    //  Display
    #undef  OLED_SDA
    #undef  OLED_SCL
    #undef  OLED_RST

    #define OLED_SDA            8
    #define OLED_SCL            9
    #define OLED_RST            -1

    //  GPS
    #define HAS_GPS_CTRL
    #define GPS_RX              6
    #define GPS_TX              5
    #define GPS_PPS             7

    //  OTHER
    #define BUTTON_PIN          0
    #define BUTTON2_PIN         3   // User button (not used yet)

    #define HAS_AXP2101

#endif
