#ifndef PINS_CONFIG_H_
#define PINS_CONFIG_H_

#undef OLED_SDA
#undef OLED_SCL
#undef OLED_RST

#ifndef TTGO_T_Beam_S3_SUPREME_V3
#define OLED_SDA        21
#define OLED_SCL        22
#define OLED_RST        16
#endif

#if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_0_SX1268) || defined(TTGO_T_Beam_V1_2_SX1262)
#define GPS_RX          12
#define GPS_TX          34
#define BUTTON_PIN      38 // The middle button GPIO on the T-Beam
#endif

#if defined(ESP32_DIY_LoRa_GPS) || defined(TTGO_T_LORA_V2_1_GPS)
#define GPS_RX          12
#define GPS_TX          34
#define BUTTON_PIN      -1
#define LORA_SCK        5
#define LORA_MISO       19
#define LORA_MOSI       27
#define LORA_CS         18  // CS  --> NSS
#define LORA_RST        23
#define LORA_IRQ        26  // IRQ --> DIO0
#endif

#ifdef ESP32_DIY_1W_LoRa_GPS
#define GPS_RX          17
#define GPS_TX          16
#define BUTTON_PIN      15
#define RADIO_SCLK_PIN  18
#define RADIO_MISO_PIN  19
#define RADIO_MOSI_PIN  23
#define RADIO_CS_PIN    5
#define RADIO_RST_PIN   27
#define RADIO_DIO1_PIN  12
#define RADIO_BUSY_PIN  14
#define RADIO_RXEN      32
#define RADIO_TXEN      25
#endif

#if defined(TTGO_T_Beam_V1_0_SX1268) || defined(TTGO_T_Beam_V1_2_SX1262)
#define RADIO_SCLK_PIN  5
#define RADIO_MISO_PIN  19
#define RADIO_MOSI_PIN  27
#define RADIO_CS_PIN    18
#define RADIO_DIO0_PIN  26
#define RADIO_RST_PIN   23
#define RADIO_DIO1_PIN  33
#define RADIO_BUSY_PIN  32
#endif

#ifdef TTGO_T_Beam_V0_7
#define GPS_RX          15
#define GPS_TX          12
#define BUTTON_PIN      39
#endif

#ifdef TTGO_T_LORA_V2_1_TNC
#define GPS_RX          -1
#define GPS_TX          -1
#define BUTTON_PIN      -1
#endif


#if defined(TTGO_T_Beam_S3_SUPREME_V3)
#define OLED_SDA        17
#define OLED_SCL        18
#define OLED_RST        16
  
#define SDA             17
#define SCL             18

#define I2C1_SDA        42
#define I2C1_SCL        41
//#define PMU_IRQ                     40    // double defined in power_utils.cpp
//#define IRQ_PIN                     40    // double defined in power_utils.cpp

#define GPS_RX          8       // changed to maintain the same "neo6m_gps.begin(9600, SERIAL_8N1, GPS_TX, GPS_RX);"
#define GPS_TX          9       // 8 for 9 and 9 for 8

/*      NOT USED????
//#define GPS_WAKEUP_PIN  7       // ??
//#define GPS_1PPS_PIN    6       // ??*/

#define BUTTON_PIN      0

/*      NOT USED????
//#define BUTTON_PIN_MASK GPIO_SEL_0    // ??
//#define BUTTON_CONUT    1             // ??
//#define BUTTON_ARRAY    BUTTON_PIN    // ??*/

#define RADIO_SCLK_PIN  12
#define RADIO_MISO_PIN  13
#define RADIO_MOSI_PIN  11
#define RADIO_CS_PIN    10
#define RADIO_DIO0_PIN  -1
#define RADIO_RST_PIN   5
#define RADIO_DIO1_PIN  1
#define RADIO_BUSY_PIN  4

/*      NOT USED????
/*#define SPI_MOSI                    35      // ??
#define SPI_SCK                     36      // ??
#define SPI_MISO                    37      // ??
#define SPI_CS                      47      // ??
#define IMU_CS                      34      // ??
#define IMU_INT                     33      // ??*/

/*      NOT USED????
#define SDCARD_MOSI                 SPI_MOSI    // ??
#define SDCARD_MISO                 SPI_MISO    // ??
#define SDCARD_SCLK                 SPI_SCK     // ??
#define SDCARD_CS                   SPI_CS      // ??*/

/*      NOT USED????
#define PIN_NONE                    -1          // ??
#define RTC_INT                     14          // ??*/

//#define GPS_BAUD_RATE               9600      // not used!!

/*      NOT USED????
#define HAS_SDCARD      // ??
#define HAS_GPS      // ??
#define HAS_DISPLAY      // ??
#define HAS_PMU      // ??*/

/*      NOT USED????
#define __HAS_SPI1__      // ??
#define __HAS_SENSOR__      // ??*/

//#define PMU_WIRE_PORT   Wire1      // ??
//#define DISPLAY_MODEL   U8G2_SH1106_128X64_NONAME_F_HW_I2C

#endif


#endif