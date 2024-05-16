#include <logger.h>
#include <Wire.h>
#include "custom_characters.h"
#include "configuration.h"
#include "boards_pinout.h"
#include "display.h"
#include "TimeLib.h"

#ifdef HAS_TFT
    #include <TFT_eSPI.h>

    TFT_eSPI tft = TFT_eSPI(); 

    #ifdef HELTEC_WIRELESS_TRACKER
        #define bigSizeFont     2
        #define smallSizeFont   1
        #define lineSpacing     9
    #endif
    #ifdef TTGO_T_DECK_GPS
        #define bigSizeFont     4
        #define smallSizeFont   2
        #define lineSpacing     18
    #endif
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
#endif

#define SYMBOL_HEIGHT 14
#define SYMBOL_WIDTH  16

extern Configuration    Config;
extern Beacon           *currentBeacon;
extern int              menuDisplay;
extern bool             bluetoothConnected;

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

int         lastMenuDisplay         = 0;
uint8_t     screenBrightness        = 1;    //from 1 to 255 to regulate brightness of oled scren
bool        symbolAvailable         = true;

extern logging::Logger logger;

void cleanTFT() {
    #ifdef HAS_TFT
        tft.fillScreen(TFT_BLACK);
    #endif
}

void setup_display() {
    delay(500);
    #ifdef HAS_TFT
        tft.init();
        tft.begin();
        if (Config.display.turn180) {
                tft.setRotation(3);
        } else {
            tft.setRotation(1);
        }
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

void display_toggle(bool toggle) {
    if (toggle) {
        #ifdef HAS_TFT
            digitalWrite(TFT_BL, HIGH);
        #endif
        #ifdef ssd1306
            display.ssd1306_command(SSD1306_DISPLAYON);
        #endif
    } else {
        #ifdef HAS_TFT
            digitalWrite(TFT_BL, LOW);
        #endif
        #ifdef ssd1306
            display.ssd1306_command(SSD1306_DISPLAYOFF);
        #endif
    }
}

void show_display(const String& header, const String& line1, const String& line2, int wait) {
    #ifdef HAS_TFT
        cleanTFT();
        tft.setTextColor(TFT_WHITE,TFT_BLACK);
        tft.setTextSize(bigSizeFont);
        tft.setCursor(0, 0);
        tft.print(header);
        tft.setTextSize(smallSizeFont);
        tft.setCursor(0, ((lineSpacing * 2) - 2));
        tft.print(line1);
        tft.setCursor(0, ((lineSpacing * 3) - 2));
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

void show_display(const String& header, const String& line1, const String& line2, const String& line3, const String& line4, const String& line5, int wait) {
    #ifdef HAS_TFT
        if (menuDisplay != lastMenuDisplay) {
            lastMenuDisplay = menuDisplay;
            cleanTFT();
        }
        //tft.setTextColor(TFT_RED,TFT_BLACK);
        tft.setTextColor(TFT_WHITE,TFT_BLACK);
        tft.setTextSize(bigSizeFont);
        tft.setCursor(0, 0);
        tft.print(header);
        tft.setTextSize(smallSizeFont);
        tft.setCursor(0, ((lineSpacing * 2) - 2));
        tft.print(line1);
        tft.setCursor(0, ((lineSpacing * 3) - 2));
        tft.print(line2);
        tft.setCursor(0, ((lineSpacing * 4) - 2));
        tft.print(line3);
        tft.setCursor(0, ((lineSpacing * 5) - 2));
        tft.print(line4);
        tft.setCursor(0, ((lineSpacing *6) - 2));
        tft.print(line5);

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
                    #if HELTEC_WIRELESS_TRACKER
                        tft.drawBitmap((TFT_WIDTH - SYMBOL_WIDTH + (128 - TFT_WIDTH)), 0, symbolsAPRS[symbol], SYMBOL_WIDTH, SYMBOL_HEIGHT, TFT_WHITE);//, TFT_RED);
                    #endif
                    #if TTGO_T_DECK_GPS
                        tft.drawBitmap((TFT_WIDTH - SYMBOL_WIDTH), 0, symbolsAPRS[symbol], SYMBOL_WIDTH, SYMBOL_HEIGHT, TFT_WHITE);//, TFT_RED);
                    #endif
                }
            } else if (bluetoothConnected) {    // TODO In this case, the text symbol stay displayed due to symbolAvailable false in menu_utils
                #if HELTEC_WIRELESS_TRACKER
                    tft.drawBitmap((TFT_WIDTH - SYMBOL_WIDTH + (128 - TFT_WIDTH)), 0, bluetoothSymbol, SYMBOL_WIDTH, SYMBOL_HEIGHT, TFT_WHITE);
                #endif
                #if TTGO_T_DECK_GPS
                    tft.drawBitmap((TFT_WIDTH - SYMBOL_WIDTH), 0, bluetoothSymbol, SYMBOL_WIDTH, SYMBOL_HEIGHT, TFT_WHITE);
                #endif
            }
        }

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
                    display.drawBitmap((display.width() - SYMBOL_WIDTH), 0, symbolsAPRS[symbol], SYMBOL_WIDTH, SYMBOL_HEIGHT, 1);
                }
            } else if (bluetoothConnected) {
                // TODO In this case, the text symbol stay displayed due to symbolAvailable false in menu_utils
                display.drawBitmap((display.width() - SYMBOL_WIDTH), 0, bluetoothSymbol, SYMBOL_WIDTH, SYMBOL_HEIGHT, 1);
            }
        }
        
        display.display();
    #endif
    delay(wait);
}

void startupScreen(uint8_t index, const String& version) {
    String workingFreq = "    LoRa Freq [";
    switch (index) {
        case 0: workingFreq += "Eu]"; break;
        case 1: workingFreq += "PL]"; break;
        case 2: workingFreq += "UK]"; break;
    }
    show_display(" LoRa APRS", "      (TRACKER)", workingFreq, "", "Richonguzman / CA2RXU", "      " + version, 4000);
    #ifdef HAS_TFT
        cleanTFT();
    #endif
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "RichonGuzman (CA2RXU) --> LoRa APRS Tracker/Station");
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Version: %s", version);
}