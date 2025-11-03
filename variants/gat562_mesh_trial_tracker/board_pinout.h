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
    #define RADIO_SCLK_PIN      43//PIN_SPI_SCK (43)
    #define RADIO_MISO_PIN      45//
    #define RADIO_MOSI_PIN      44//
    #define RADIO_CS_PIN        42//
    #define RADIO_RST_PIN       38//
    #define RADIO_DIO1_PIN      47//
    #define RADIO_BUSY_PIN      46//
    #define RADIO_WAKEUP_PIN    RADIO_DIO1_PIN


// #define SX126X_TXEN (39)
// #define SX126X_RXEN (37)
//#define SX126X_POWER_EN (37)
// DIO2 controlls an antenna switch and the TCXO voltage is controlled by DIO3
//#define SX126X_DIO2_AS_RF_SWITCH
//#define SX126X_DIO3_TCXO_VOLTAGE 1.8
    //  Display
    #undef  OLED_SDA
    #undef  OLED_SCL
    #undef  OLED_RST

    #define OLED_SDA            13//
    #define OLED_SCL            14//
//    #define OLED_RST            16

    //  GPS
    #define HAS_GPS_CTRL
    #define GPS_RX              15//
    #define GPS_TX              16//
    #define GPS_BAUDRATE        9600
    #define GPS_PPS             17 // Pulse per second input from the GPS
// Define pin to enable GPS toggle (set GPIO to LOW) via user button triple press
// Connected to Jlink CDC
//#define PIN_SERIAL2_RX (8)
//#define PIN_SERIAL2_TX (6)
/*
static const uint8_t A0 = PIN_A0;
static const uint8_t A1 = PIN_A1;
static const uint8_t A2 = PIN_A2;
static const uint8_t A3 = PIN_A3;
static const uint8_t A4 = PIN_A4;
static const uint8_t A5 = PIN_A5;
static const uint8_t A6 = PIN_A6;
static const uint8_t A7 = PIN_A7;
#define ADC_RESOLUTION 14

// Other pins
#define PIN_AREF (2)
#define PIN_NFC1 (9)
#define PIN_NFC2 (10)

static const uint8_t AREF = PIN_AREF;
*/

/*
 * SPI Interfaces
 */
//#define SPI_INTERFACES_COUNT 2

/*
 * Wire Interfaces
 */
//#define WIRE_INTERFACES_COUNT 1

// QSPI Pins
/*
#define PIN_QSPI_SCK 3
#define PIN_QSPI_CS 26
#define PIN_QSPI_IO0 30
#define PIN_QSPI_IO1 29
#define PIN_QSPI_IO2 28
#define PIN_QSPI_IO3 2
*/
// On-board QSPI Flash
/*
#define EXTERNAL_FLASH_DEVICES IS25LP080D
#define EXTERNAL_FLASH_USE_QSPI
*/
/* @note RAK5005-O GPIO mapping to RAK4631 GPIO ports
   RAK5005-O <->  nRF52840
   IO1       <->  P0.17 (Arduino GPIO number 17)
   IO2       <->  P1.02 (Arduino GPIO number 34)
   IO3       <->  P0.21 (Arduino GPIO number 21)
   IO4       <->  P0.04 (Arduino GPIO number 4)
   IO5       <->  P0.09 (Arduino GPIO number 9)
   IO6       <->  P0.10 (Arduino GPIO number 10)
   IO7       <->  P0.28 (Arduino GPIO number 28)
   SW1       <->  P0.01 (Arduino GPIO number 1)
   A0        <->  P0.04/AIN2 (Arduino Analog A2
   A1        <->  P0.31/AIN7 (Arduino Analog A7
   SPI_CS    <->  P0.26 (Arduino GPIO number 26)
*/

// configure the SET pin on the RAK12039 sensor board to disable the sensor while not reading
// air quality telemetry.  PIN_NFC2 doesn't seem to be used anywhere else in the codebase, but if
// you're having problems with your node behaving weirdly when a RAK12039 board isn't connected,
// try disabling this.
// #define PMSA003I_ENABLE_PIN PIN_NFC2

// #define DETECTION_SENSOR_EN 4



// Testing USB detection
//#define NRF_APM

// enables 3.3V periphery like GPS or IO Module
// Do not toggle this for GPS power savings
//#define PIN_3V3_EN (34)

// RAK1910 GPS module
// If using the wisblock GPS module and pluged into Port A on WisBlock base
// IO1 is hooked to PPS (pin 12 on header) = gpio 17
// IO2 is hooked to GPS RESET = gpio 34, but it can not be used to this because IO2 is ALSO used to control 3V3_S power (1 is on).
// Therefore must be 1 to keep peripherals powered
// Power is on the controllable 3V3_S rail
// #define PIN_GPS_RESET (34)
// #define PIN_GPS_EN PIN_3V3_EN

// RAK12002 RTC Module
// #define RV3028_RTC (uint8_t)0b1010010

// RAK18001 Buzzer in Slot C
// #define PIN_BUZZER 21 // IO3 is PWM2
// NEW: set this via protobuf instead!

// Battery
// The battery sense is hooked to pin A0 (5)
//#define BATTERY_PIN PIN_A0
// and has 12 bit resolution
//#define BATTERY_SENSE_RESOLUTION_BITS 12
//#define BATTERY_SENSE_RESOLUTION 4096.0
//#undef AREF_VOLTAGE
//#define AREF_VOLTAGE 3.0
//#define VBAT_AR_INTERNAL AR_INTERNAL_3_0
//#define ADC_MULTIPLIER 1.73

 #define HAS_RTC 1

// #define HAS_ETHERNET 1

 #define RAK_4631 1

// #define PIN_ETHERNET_RESET 21
// #define PIN_ETHERNET_SS PIN_EINK_CS
// #define ETH_SPI_PORT SPI1
// #define AQ_SET_PIN 10

    //  OTHER
    // #define BUTTON_PIN          38 // The middle button GPIO on the T-Beam - 注释掉，使用摇杆中心按钮

    #define HAS_BT_CLASSIC
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