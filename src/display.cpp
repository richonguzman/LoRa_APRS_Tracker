#include <logger.h>
#include <Wire.h>
#include "custom_characters.h"
#include "configuration.h"
#include "pins_config.h"
#include "display.h"
#include "TimeLib.h"

#ifdef HAS_TFT
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI(); 

#else
#include <Adafruit_GFX.h>

#define ssd1306 //comment this line with "//" when using SH1106 screen instead of SSD1306

#if defined(TTGO_T_Beam_S3_SUPREME_V3)
#undef ssd1306
#endif
#if defined(HELTEC_V3_GPS)
#define OLED_DISPLAY_HAS_RST_PIN
#endif

#ifdef ssd1306
#include <Adafruit_SSD1306.h>
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RST);
#else
#include <Adafruit_SH110X.h>
Adafruit_SH1106G display(128, 64, &Wire, OLED_RST);
#endif

#define SYM_HEIGHT 14
#define SYM_WIDTH  16

#endif

extern Configuration    Config;
extern Beacon           *currentBeacon;
extern int              menuDisplay;
extern bool             symbolAvailable;
extern bool             bluetoothConnected;
extern uint8_t          screenBrightness; //from 1 to 255 to regulate brightness of oled scren

const char* symbolArray[]     = { "[", ">", "j", "b", "<", "s", "u", "R", "v", "(", ";", "-", "k",
                                "C", "a", "Y", "O", "'", "=", "y"};
int   symbolArraySize         = sizeof(symbolArray)/sizeof(symbolArray[0]);
const uint8_t *symbolsAPRS[]  = {runnerSymbol, carSymbol, jeepSymbol, bikeSymbol, motorcycleSymbol, shipSymbol, 
                                truck18Symbol, recreationalVehicleSymbol, vanSymbol, carsateliteSymbol, tentSymbol,
                                houseSymbol, truckSymbol, canoeSymbol, ambulanceSymbol, yatchSymbol, baloonSymbol,
                                aircraftSymbol, trainSymbol, yagiSymbol};
// T-Beams bought with soldered OLED Screen comes with only 4 pins (VCC, GND, SDA, SCL)
// If your board didn't come with 4 pins OLED Screen and comes with 5 and one of them is RST...
// Uncomment Next Line (Remember ONLY if your OLED Screen has a RST pin). This is to avoid memory issues.
//#define OLED_DISPLAY_HAS_RST_PIN

extern logging::Logger logger;

void cleanTFT() {
    #ifdef HAS_TFT
    tft.fillScreen(TFT_BLACK);
    #endif
}

// cppcheck-suppress unusedFunction
void setup_display() {
    delay(500);
    #ifdef HAS_TFT
    tft.init();
    tft.begin();
    tft.setRotation(1);
    tft.setTextFont(0);
    tft.fillScreen(TFT_BLACK);
    #else    
    #ifdef OLED_DISPLAY_HAS_RST_PIN
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, LOW);
    delay(20);
    digitalWrite(OLED_RST, HIGH);
    #endif

    Wire.begin(OLED_SDA, OLED_SCL);
    #ifdef ssd1306
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3c, false, false)) {
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "SSD1306", "allocation failed!");
        while (true) {
        }
    }
    if (Config.display.turn180) {
        display.setRotation(2);
    }
    #else
    if (!display.begin(0x3c, true)) {
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "SH1106", "allocation failed!");
        while (true) {
        }
    }
    #endif
    display.clearDisplay();
    #ifdef ssd1306
    display.setTextColor(WHITE);
    #else
    display.setTextColor(SH110X_WHITE);
    #endif
    display.setTextSize(1);
    display.setCursor(0, 0);
    #ifdef ssd1306
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(screenBrightness);
    #endif
    display.display();
    #endif
}

// cppcheck-suppress unusedFunction
void display_toggle(bool toggle) {
    #ifdef HAS_TFT
    //algo
    #else
    if (toggle) {
        #ifdef ssd1306
        display.ssd1306_command(SSD1306_DISPLAYON);
        #endif
    } else {
        #ifdef ssd1306
        display.ssd1306_command(SSD1306_DISPLAYOFF);
        #endif
    }
    #endif
}

// cppcheck-suppress unusedFunction
void show_display(String header, int wait) {
    #ifdef HAS_TFT
    cleanTFT();
    tft.setTextColor(TFT_WHITE,TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(0, 0);
    tft.print(header);
    #else
    display.clearDisplay();
    #ifdef ssd1306
    display.setTextColor(WHITE);
    #else
    display.setTextColor(SH110X_WHITE);
    #endif
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println(header);
    #ifdef ssd1306
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(screenBrightness);
    #endif
    display.display();
    #endif
    delay(wait);
}

// cppcheck-suppress unusedFunction
void show_display(String header, String line1, int wait) {
    #ifdef HAS_TFT
    cleanTFT();
    tft.setTextColor(TFT_WHITE,TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(0, 0);
    tft.print(header);
    tft.setTextSize(1);
    tft.setCursor(0, 16);
    tft.print(line1);
    #else
    display.clearDisplay();
    #ifdef ssd1306
    display.setTextColor(WHITE);
    #else
    display.setTextColor(SH110X_WHITE);
    #endif
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println(header);
    display.setTextSize(1);
    display.setCursor(0, 16);
    display.println(line1);
    #ifdef ssd1306
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(screenBrightness);
    #endif
    display.display();
    #endif
    delay(wait);
}

// cppcheck-suppress unusedFunction
void show_display(String header, String line1, String line2, int wait) {
    #ifdef HAS_TFT
    cleanTFT();
    tft.setTextColor(TFT_WHITE,TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(0, 0);
    tft.print(header);
    tft.setTextSize(1);
    tft.setCursor(0, 16);
    tft.print(line1);
    tft.setCursor(0, 25);
    tft.print(line2);
    #else
    display.clearDisplay();
    #ifdef ssd1306
    display.setTextColor(WHITE);
    #else
    display.setTextColor(SH110X_WHITE);
    #endif
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println(header);
    display.setTextSize(1);
    display.setCursor(0, 16);
    display.println(line1);
    display.setCursor(0, 26);
    display.println(line2);
    #ifdef ssd1306
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(screenBrightness);
    #endif
    display.display();
    #endif
    delay(wait);
}

// cppcheck-suppress unusedFunction
void show_display(String header, String line1, String line2, String line3, int wait) {
    #ifdef HAS_TFT
    cleanTFT();
    tft.setTextColor(TFT_WHITE,TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(0, 0);
    tft.print(header);
    tft.setTextSize(1);
    tft.setCursor(0, 16);
    tft.print(line1);
    tft.setCursor(0, 25);
    tft.print(line2);
    tft.setCursor(0, 34);
    tft.print(line3);
    #else
    display.clearDisplay();
    #ifdef ssd1306
    display.setTextColor(WHITE);
    #else
    display.setTextColor(SH110X_WHITE);
    #endif
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
    #ifdef ssd1306
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(screenBrightness);
    #endif
    display.display();
    #endif
    delay(wait);
}

// cppcheck-suppress unusedFunction
void show_display(String header, String line1, String line2, String line3, String line4, int wait) {
    #ifdef HAS_TFT
    cleanTFT();
    tft.setTextColor(TFT_WHITE,TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(0, 0);
    tft.print(header);
    tft.setTextSize(1);
    tft.setCursor(0, 16);
    tft.print(line1);
    tft.setCursor(0, 25);
    tft.print(line2);
    tft.setCursor(0, 34);
    tft.print(line3);
    tft.setCursor(0, 43);
    tft.print(line4);
    #else
    display.clearDisplay();
    #ifdef ssd1306
    display.setTextColor(WHITE);
    #else
    display.setTextColor(SH110X_WHITE);
    #endif
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
    #ifdef ssd1306
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(screenBrightness);
    #endif
    display.display();
    #endif
    delay(wait);
}

// cppcheck-suppress unusedFunction
void show_display(String header, String line1, String line2, String line3, String line4, String line5, int wait) {
    #ifdef HAS_TFT
    cleanTFT();
    //tft.setTextColor(TFT_RED,TFT_BLACK);
    tft.setTextColor(TFT_WHITE,TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(0, 0);
    tft.print(header);
    tft.setTextSize(1);
    tft.setCursor(0, 16);
    tft.print(line1);
    tft.setCursor(0, 25);
    tft.print(line2);
    tft.setCursor(0, 34);
    tft.print(line3);
    tft.setCursor(0, 43);
    tft.print(line4);
    tft.setCursor(0, 52);
    tft.print(line5);
    #else
    display.clearDisplay();
    #ifdef ssd1306
    display.setTextColor(WHITE);
    #else
    display.setTextColor(SH110X_WHITE);
    #endif
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
    #ifdef ssd1306
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(screenBrightness);
    #endif

    if (menuDisplay == 0 && Config.display.showSymbol) {
        int symbol = 100;
        for (int i = 0; i < symbolArraySize; i++) {
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
    #endif
    delay(wait);
}