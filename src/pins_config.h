#ifndef PINS_CONFIG_H_
#define PINS_CONFIG_H_

#undef OLED_SDA
#undef OLED_SCL
#undef OLED_RST

#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_RST 16

#define BUTTON_PIN 38 // The middle button GPIO on the T-Beam

#ifdef TTGO_T_Beam_V0_7
#define GPS_RX 15
#define GPS_TX 12
#endif

#if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_Beam_V1_2)
#define GPS_RX 12
#define GPS_TX 34
#endif

#endif
