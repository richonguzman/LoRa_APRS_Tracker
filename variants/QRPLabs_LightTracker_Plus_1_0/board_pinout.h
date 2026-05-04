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
    #define HAS_SX1268
	#define HAS_1W_LORA
    #define RADIO_VCC_PIN       21
    #define RADIO_SCLK_PIN      12
    #define RADIO_MISO_PIN      13
    #define RADIO_MOSI_PIN      11
    #define RADIO_CS_PIN        10
    #define RADIO_RST_PIN       9
    #define RADIO_DIO1_PIN      5
    #define RADIO_BUSY_PIN      6
    #define RADIO_RXEN          42
    #define RADIO_TXEN          14
    
    //  Display
    #undef  OLED_SDA
    #undef  OLED_SCL
    #undef  OLED_RST

    #define OLED_SDA            3
    #define OLED_SCL            4
    #define OLED_RST            -1

    //  GPS
    #define GPS_RX              17
    #define GPS_TX              18
    #define GPS_VCC             33    //#define LED_PIN             16

    //  OTHER
    #define BUTTON_PIN          0
    #define BATTERY_PIN         1
    
#endif