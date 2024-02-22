#include <logger.h>
#include <Wire.h>
#include "custom_characters.h"
#include "configuration.h"
#include "pins_config.h"
#include "display.h"
#include "TimeLib.h"
#ifdef tST7735
//#include <U8g2lib.h>
#include <Arduino_GFX_Library.h>
#elif
#include <Adafruit_GFX.h>

#ifdef oSSD1306
#include <Adafruit_SSD1306.h>
#else
#include <Adafruit_SH110X.h>
#endif

#define SYM_HEIGHT 14
#define SYM_WIDTH  16
#define SCREEN_W   128
#ifndef SCREEN_H
#define SCREEN_H   64
#endif
#endif

extern Configuration    Config;
extern Beacon           *currentBeacon;
extern int              menuDisplay;
extern bool             symbolAvailable;
extern bool             bluetoothConnected;
extern int              screenBrightness; //from 1 to 255 to regulate brightness of oled scren

const char* symbolArray[]     = { "[", ">", "j", "b", "<", "s", "u", "R", "v", "(", ";", "-", "k",
                                "C", "a", "Y", "O", "'"};
int   symbolArraySize         = sizeof(symbolArray)/sizeof(symbolArray[0]);
const uint8_t *symbolsAPRS[]  = {runnerSymbol, carSymbol, jeepSymbol, bikeSymbol, motorcycleSymbol, shipSymbol, 
                                truck18Symbol, recreationalVehicleSymbol, vanSymbol, carsateliteSymbol, tentSymbol,
                                houseSymbol, truckSymbol, canoeSymbol, ambulanceSymbol, yatchSymbol, baloonSymbol,
                                aircraftSymbol};
// T-Beams bought with soldered OLED Screen comes with only 4 pins (VCC, GND, SDA, SCL)
// If your board didn't come with 4 pins OLED Screen and comes with 5 and one of them is RST...
// Uncomment Next Line (Remember ONLY if your OLED Screen has a RST pin). This is to avoid memory issues.
//#define OLED_DISPLAY_HAS_RST_PIN

extern logging::Logger logger;

#ifndef tST7735
#ifdef oSSD1306
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, OLED_RST);
#ifdef DUALSCREEN
Adafruit_SSD1306 display2(SCREEN_W, SCREEN_H, &Wire, OLED_RST);
#endif
#elif oSH1107
Adafruit_SH1107 display(SCREEN_W, SCREEN_H, &Wire, OLED_RST);
#elif oSH1106
Adafruit_SH1106G display(SCREEN_W, SCREEN_H, &Wire, OLED_RST);
#endif

// cppcheck-suppress unusedFunction
void setup_display() {
  delay(500);
  #ifdef OLED_DISPLAY_HAS_RST_PIN //
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, LOW);
    delay(20);
    digitalWrite(OLED_RST, HIGH);
  #endif

  Wire.begin(OLED_SDA, OLED_SCL);
  #ifdef oSSD1306
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3c, false, false)) {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "SSD1306", "allocation failed!");
    while (true) {
    }
  }
  #ifdef DUALSCREEN
    if (!display2.begin(SSD1306_SWITCHCAPVCC, 0x3d, false, false)) {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "SSD1306-2", "allocation failed!");
    while (true) {
    }
  }
  #endif
  #else
  if (!display.begin(0x3c, true)) {
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "SH1106", "allocation failed!");
    while (true) {
    }
  }
  #endif
  if (Config.display.turn180) {
    display.setRotation(2);
    #ifdef DUALSCREEN
    display2.setRotation(2);
    #endif
  }
  display.clearDisplay();
  display.setTextColor(1); //WHITE & SH110X_WHITE value is 1
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.display();
  #ifdef DUALSCREEN
  display2.clearDisplay();
  display2.setTextColor(1); //WHITE & SH110X_WHITE value is 1
  display2.setTextSize(1);
  display2.setCursor(0, 0);
  display2.display();
  #endif
  _setContrast(screenBrightness);
}

// cppcheck-suppress unusedFunction
void display_toggle(bool toggle) {
  if (toggle) {
    #ifdef oSSD1306
    display.ssd1306_command(SSD1306_DISPLAYON);
    #ifdef DUALSCREEN
    display2.ssd1306_command(SSD1306_DISPLAYON);
    #endif
    #else
    display.oled_command(SH110X_DISPLAYON);
    #endif

  } else {
    #ifdef oSSD1306
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    #ifdef DUALSCREEN
    display2.ssd1306_command(SSD1306_DISPLAYOFF);
    #endif
    #else
    display.oled_command(SH110X_DISPLAYOFF);
    #endif
  }
}

void _setContrast(uint8_t dimm) {
  #ifdef oSSD1306
  display.ssd1306_command(0x81); //Set Contrast Command 0x81
  display.ssd1306_command(dimm);
  #ifdef DUALSCREEN
  display2.ssd1306_command(0x81); //Set Contrast Command 0x81
  display2.ssd1306_command(dimm);
  #endif
  #else
  display.oled_command(0x81); //Set Contrast Command
  display.oled_command(dimm);
  #endif
}

// cppcheck-suppress unusedFunction
void show_display(String header, int wait) {
  display.clearDisplay();
  _setContrast(screenBrightness);
  display.setTextColor(1);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(header);
  display.display();
  delay(wait);
}

// cppcheck-suppress unusedFunction
void show_display(String header, String line1, int wait) {
  display.clearDisplay();
  _setContrast(screenBrightness);
  display.setTextColor(1);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(header);
  display.setTextSize(1);
  display.setCursor(0, 16);
  display.println(line1);
  display.display();
  delay(wait);
}

// cppcheck-suppress unusedFunction
void show_display(String header, String line1, String line2, int wait) {
  display.clearDisplay();
  _setContrast(screenBrightness);
  display.setTextColor(1);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(header);
  display.setTextSize(1);
  display.setCursor(0, 16);
  display.println(line1);
  display.setCursor(0, 26);
  display.println(line2);
  display.display();
  delay(wait);
}

// cppcheck-suppress unusedFunction
void show_display(String header, String line1, String line2, String line3, int wait) {
  display.clearDisplay();
  _setContrast(screenBrightness);
  display.setTextColor(1);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(header);
  display.setTextSize(1);
  display.setCursor(0, 16);
  display.println(line1);
  display.setCursor(0, 26);
  display.println(line2);
  display.setCursor(0, 36);
  display.println(line3);
  display.display();
  delay(wait);
}

// cppcheck-suppress unusedFunction
void show_display(String header, String line1, String line2, String line3, String line4, int wait) {
  display.clearDisplay();
  _setContrast(screenBrightness);
  display.setTextColor(1);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(header);
  display.setTextSize(1);
  display.setCursor(0, 16);
  display.println(line1);
  display.setCursor(0, 26);
  display.println(line2);
  display.setCursor(0, 36);
  display.println(line3);
  display.setCursor(0, 46);
  display.println(line4);
  display.display();
  delay(wait);
}

// cppcheck-suppress unusedFunction
void show_display(String header, String line1, String line2, String line3, String line4, String line5, int wait) {
  _setContrast(screenBrightness);
  display.clearDisplay();
  display.setTextColor(1);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(header);
  display.setTextSize(1);
  display.setCursor(0, 16);
  display.println(line1);
  display.setCursor(0, 26);
  display.println(line2);
  display.setCursor(0, 36);
  display.println(line3);
  display.setCursor(0, 46);
  display.println(line4);
  display.setCursor(0, 56);
  display.println(line5);

  if (menuDisplay==0 && Config.display.showSymbol) {
    int symbol = 100;
    for (int i=0; i<symbolArraySize; i++) {
      if (currentBeacon->symbol == symbolArray[i]) {
        symbol = i;
        break;
      }
    }

    symbolAvailable = symbol != 100;

    /*
     * Symbol alternate every 5s
     * If bluetooth is disconnected or if we are in the first part of the clock, then we show the APRS symbol
     * Otherwise, we are in the second part of the clock, then we show BT connected
     */
    const auto time_now = now();
    if (!bluetoothConnected || time_now % 10 < 5) {
      if (symbolAvailable) {
        display.drawBitmap((display.width() - SYM_WIDTH), 0, symbolsAPRS[symbol], SYM_WIDTH, SYM_HEIGHT, 1);
      }
    } else if (bluetoothConnected) {
      // TODO In this case, the text symbol stay displayed due to symbolAvailable false in menu_utils
      display.drawBitmap((display.width() - SYM_WIDTH), 0, bluetoothSymbol, SYM_WIDTH, SYM_HEIGHT, 1);
    }
  }
  
  display.display();
  #ifdef DUALSCREEN
  //second SSD1306 test HERE!
  display2.clearDisplay();
  display2.display();
  #endif
  delay(wait);
}
#else

#include <Fonts/FreeMonoBold9pt7b.h>
//#include <Fonts/FreeMono9pt7b.h>
//
#ifndef VSPI
#define VSPI 1
#endif
Arduino_DataBus *bus = new Arduino_ESP32SPI(ST7735_DC_PIN /* DC */, ST7735_CS_PIN /* CS */, ST7735_SCLK_PIN, ST7735_MOSI_PIN, ST7735_MOSI_PIN, VSPI);
Arduino_GFX *gfx = new Arduino_ST7735(bus, GFX_NOT_DEFINED /* RST */, 1 /* rotation */, true /* IPS */, 80 /* width */, 160 /* height */, 26 /* col offset 1 */, 1 /* row offset 1 */, 26 /* col offset 2 */, 1 /* row offset 2 */);

void setup_display() {
  #ifdef HWT_VERSION05
  pinMode(VEXT_ENABLE_V05, OUTPUT);     //Vext control pin
  digitalWrite(VEXT_ENABLE_V05, HIGH);  //LCD needs power before init.
  pinMode(ST7735_LED_K_PIN_V05, OUTPUT);    //backlight pin ledk
  digitalWrite(ST7735_LED_K_PIN_V05, HIGH); //backlight HIGH turn ON
  #else
  pinMode(VEXT_ENABLE_V03, OUTPUT);     //Vext control pin
  digitalWrite(VEXT_ENABLE_V03, LOW);  //LCD needs power before init.
  pinMode(ST7735_LED_K_PIN_V03, OUTPUT);    //backlight pin ledk
  digitalWrite(ST7735_LED_K_PIN_V03, HIGH); //backlight HIGH turn ON
  #endif
  if (!gfx->begin())
  {
    Serial.println("gfx->begin() failed!");
  }
  gfx->fillScreen(BLACK);
}

void display_toggle(bool toggle) {
  if (toggle) {
    #ifdef HWT_VERSION05
    digitalWrite(ST7735_LED_K_PIN_V05, HIGH);
    #elif HWT_VERSION03
    digitalWrite(ST7735_LED_K_PIN_V03, HIGH);
    #endif
  } else {
    #ifdef HWT_VERSION05
    digitalWrite(ST7735_LED_K_PIN_V05, LOW);
    #elif WT_VERSION03
    digitalWrite(ST7735_LED_K_PIN_V03, LOW);
    #endif
  }
}

void _setContrast(uint8_t dimm) {
  //return dimm;
}

void show_display(String header, int wait) {
  int sy = 13;
  gfx->fillScreen(BLACK);
  gfx->setFont(&FreeMonoBold9pt7b);
  gfx->setTextColor(BLUE);
  gfx->setCursor(0, sy);
  gfx->println(header);
  gfx->setFont();
  delay(wait);
}

void show_display(String header, String line1, int wait) {
  int sy = 13;
  gfx->fillScreen(BLACK);
  gfx->setFont(&FreeMonoBold9pt7b);
  gfx->setTextColor(BLUE);
  gfx->setCursor(0, sy);
  gfx->println(header);
  gfx->setFont();
  gfx->setCursor(0, 20);
  gfx->println(line1);
  delay(wait);
}

void show_display(String header, String line1, String line2, int wait) {
  int sy = 13;
  gfx->fillScreen(BLACK);
  gfx->setFont(&FreeMonoBold9pt7b);
  gfx->setTextColor(BLUE);
  gfx->setCursor(0, sy);
  gfx->println(header);
  gfx->setFont();
  gfx->setCursor(0, 20);
  gfx->println(line1);
  gfx->setCursor(0, 32);
  gfx->println(line2);
  delay(wait);
}

void show_display(String header, String line1, String line2, String line3, int wait) {
  int sy = 13;
  gfx->fillScreen(BLACK);
  gfx->setFont(&FreeMonoBold9pt7b);
  gfx->setTextColor(BLUE);
  gfx->setCursor(0, sy);
  gfx->println(header);
  gfx->setFont();
  gfx->setCursor(0, 20);
  gfx->println(line1);
  gfx->setCursor(0, 32);
  gfx->println(line2);
  gfx->setCursor(0, 44);
  gfx->println(line3);
  delay(wait);
}

void show_display(String header, String line1, String line2, String line3, String line4, int wait) {
  int sy = 13;
  gfx->fillScreen(BLACK);
  gfx->setFont(&FreeMonoBold9pt7b);
  gfx->setTextColor(BLUE);
  gfx->setCursor(0, sy);
  gfx->println(header);
  gfx->setFont();
  gfx->setCursor(0, 20);
  gfx->println(line1);
  gfx->setCursor(0, 32);
  gfx->println(line2);
  gfx->setCursor(0, 44);
  gfx->println(line3);
  gfx->setCursor(0, 56);
  gfx->println(line4);
  delay(wait);
}

void show_display(String header, String line1, String line2, String line3, String line4, String line5, int wait) {
  int sy = 13;
  gfx->fillScreen(BLACK);
  gfx->setFont(&FreeMonoBold9pt7b);
  gfx->setTextColor(BLUE);
  gfx->setCursor(0, sy);
  gfx->println(header);
  gfx->setFont();
  gfx->setCursor(0, 20);
  gfx->println(line1);
  gfx->setCursor(0, 32);
  gfx->println(line2);
  gfx->setCursor(0, 44);
  gfx->println(line3);
  gfx->setCursor(0, 56);
  gfx->println(line4);
  gfx->setCursor(0, 68);
  gfx->println(line5);
  delay(wait);
}
#endif