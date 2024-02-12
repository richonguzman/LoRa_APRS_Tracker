#ifndef PINS_CONFIG_H_
#define PINS_CONFIG_H_

#undef OLED_SDA
#undef OLED_SCL
#undef OLED_RST

#if !defined(TTGO_T_Beam_S3_SUPREME_V3)  && !defined(HELTEC_V3_GPS)
#define OLED_SDA            21
#define OLED_SCL            22
#define OLED_RST            16
#endif

#if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_2) || defined(TTGO_T_Beam_V1_0_SX1268) || defined(TTGO_T_Beam_V1_2_SX1262)
#undef OLED_RST                 //Conflict with PSRAM. GPIO16 is PSRAM CS# PIN (T-Beam V1.1)
#define OLED_RST            -1  //Conflict with PSRAM. GPIO16 is PSRAM CS# PIN (T-Beam V1.1)
#define GPS_RX              12
#define GPS_TX              34
#define BUTTON_PIN          38  //The middle button GPIO on the T-Beam
#define LED_PIN             4   //V1_1 >> Red LED next to blue LED: LED_BUILTIN
#endif

#if defined(ESP32_DIY_LoRa_GPS) || defined(TTGO_T_LORA32_V2_1_GPS)
#define GPS_RX              12
#define GPS_TX              34
#define LORA_SCK            5
#define LORA_MISO           19
#define LORA_MOSI           27
#define LORA_CS             18  // CS  --> NSS
#define LORA_RST            23
#define LORA_IRQ            26  // IRQ --> DIO0
#define BATTERY_PIN         35  //LoRa32 Battery PIN 100k/100k
#define BUTTON_PIN          0   //Default PRG button.GPIO0, or use the following: GPIO12, 13, 14, 15
#endif

#ifdef ESP32_DIY_1W_LoRa_GPS
#undef OLED_RST                 //Conflict with GPS TX pin
#define OLED_RST            -1  //Conflict with GPS TX pin
#define GPS_RX              17
#define GPS_TX              16
#define BUTTON_PIN          15
#define RADIO_SCLK_PIN      18
#define RADIO_MISO_PIN      19
#define RADIO_MOSI_PIN      23
#define RADIO_CS_PIN        5
#define RADIO_RST_PIN       27
#define RADIO_DIO1_PIN      12
#define RADIO_BUSY_PIN      14
#define RADIO_RXEN          32
#define RADIO_TXEN          25
#define DIO3_TCXO_REF       1.8 //DIO3 Reference Voltage
#endif

#ifdef ESP32_BV5DJ_1W_LoRa_GPS
#undef OLED_RST                 //Conflict with GPS TX pin
#define OLED_RST            -1  //Conflict with GPS TX pin
#define GPS_RX              17
#define GPS_TX              16
#define BUTTON_PIN          0   //ENT
#define RADIO_SCLK_PIN      18
#define RADIO_MISO_PIN      19
#define RADIO_MOSI_PIN      23
#define RADIO_CS_PIN        5
#define RADIO_RST_PIN       14  //NRST
#define RADIO_DIO1_PIN      33
#define RADIO_BUSY_PIN      39
#define RADIO_RXEN          2   //Same LED_BUILTIN
#define RADIO_TXEN          4
//EXTRA PINS HERE:
#define BUTTON_UP           34  //joystick UP
#define BUTTON_DOWN         35  //joystick DOWN
#define BUTTON_LEFT         27  //joystick LEFT
#define BUTTON_RIGHT        32  //joystick RIGHT
#define SD_CS               13  //MicroSD card SlaveSelect
#define GPS_PPS             26  //GPS PPS pin 
#define BATTERY_PIN         36  //ADC pin from voltage divider
#define RGB_LED_PIN         12  //WS2812 LED GPIO
#define LEDNUM              2   //WS2812 LEDs number
#define KEEP_ALIVE          25  //Trigger -pad in PCB
#define DIO3_TCXO_REF       1.8 //DIO3 Reference Voltage
#endif

#if defined(TTGO_T_Beam_V1_0_SX1268)
#define RADIO_SCLK_PIN      5
#define RADIO_MISO_PIN      19
#define RADIO_MOSI_PIN      27
#define RADIO_CS_PIN        18
#define RADIO_DIO0_PIN      26
#define RADIO_RST_PIN       23
#define RADIO_DIO1_PIN      33
#define RADIO_BUSY_PIN      32
#endif

#if defined(TTGO_T_Beam_V1_2_SX1262)
#define RADIO_SCLK_PIN      5
#define RADIO_MISO_PIN      19
#define RADIO_MOSI_PIN      27
#define RADIO_CS_PIN        18
#define RADIO_DIO0_PIN      26
#define RADIO_RST_PIN       23
#define RADIO_DIO1_PIN      33
#define RADIO_BUSY_PIN      32
#define DIO3_TCXO_REF       1.8 //DIO3 Reference Voltage
#endif

#ifdef TTGO_T_Beam_V0_7
#define GPS_RX              15
#define GPS_TX              12
#define BUTTON_PIN          39
#define BATTERY_PIN         35
#endif

#ifdef TTGO_T_LORA32_V2_1_TNC
#define GPS_RX              -1
#define GPS_TX              -1
#define BUTTON_PIN          -1
#define BATTERY_PIN         35  //LoRa32 Battery PIN 100k/100k
#endif

#if defined(TTGO_T_Beam_S3_SUPREME_V3)
#define OLED_SDA            17
#define OLED_SCL            18
#define OLED_RST            16
#define GPS_RX              8
#define GPS_TX              9
#define BUTTON_PIN          0
#define RADIO_SCLK_PIN      12
#define RADIO_MISO_PIN      13
#define RADIO_MOSI_PIN      11
#define RADIO_CS_PIN        10
#define RADIO_DIO0_PIN      -1
#define RADIO_RST_PIN       5
#define RADIO_DIO1_PIN      1
#define RADIO_BUSY_PIN      4
#define DIO3_TCXO_REF       1.8 //DIO3 Reference Voltage
#endif

#if defined(HELTEC_V3_GPS)
#define OLED_SDA            17
#define OLED_SCL            18
#define OLED_RST            21
#define GPS_RX              19
#define GPS_TX              20
#define BUTTON_PIN          0
#define RADIO_SCLK_PIN      9
#define RADIO_MISO_PIN      11
#define RADIO_MOSI_PIN      10
#define RADIO_CS_PIN        8
#define RADIO_RST_PIN       12
#define RADIO_DIO1_PIN      14
#define RADIO_BUSY_PIN      13
#define BATTERY_PIN         1   //390k/100k
#define ADC_CTRL            37  //ADC control PIN -should be low for the measurement battery.
#define VEXT_CTRL           36  //Vext control PIN -for Display on/off
#define DIO3_TCXO_REF       1.8 //DIO3 TCXO Reference Voltage
#endif

#if defined(OE5HWN_MeshCom)
#define GPS_RX              17
#define GPS_TX              16
#define BUTTON_PIN          12
#define RADIO_SCLK_PIN      18
#define RADIO_MISO_PIN      19
#define RADIO_MOSI_PIN      23
#define RADIO_CS_PIN        5
#define RADIO_RST_PIN       27
#define RADIO_DIO1_PIN      33
#define RADIO_BUSY_PIN      26
#define RADIO_RXEN          14
#define RADIO_TXEN          13
#define DIO3_TCXO_REF       1.8 //DIO3 TCXO Reference Voltage
#endif

#if defined(ESP32_BATTERY_OLED)
#undef OLED_RST
#undef OLED_SCL
#undef OLED_SDA
#define OLED_SCL            4
#define OLED_SDA            5
#define OLED_RST            -1
#define GPS_RX              12
#define GPS_TX              14
#define LORA_SCK            18
#define LORA_MISO           19
#define LORA_MOSI           23
#define LORA_CS             17  // CS  --> NSS
#define LORA_RST            15
#define LORA_IRQ            22  // IRQ --> DIO0
#define BATTERY_PIN         35  //LoRa32 Battery PIN 100k/100k
#define BUTTON_PIN          0
#define LED_PIN             16  //LED_BUILTIN
#endif

#endif