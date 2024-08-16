#include <logger.h>
#include <Wire.h>
#include "custom_characters.h"
#include "configuration.h"
#include "boards_pinout.h"
#include "display.h"
#include "TimeLib.h"

String currentSymbol, lastSymbol, lastHeader;

#ifdef HAS_TFT
    #include <TFT_eSPI.h>

    TFT_eSPI tft = TFT_eSPI(); 

    #ifdef HELTEC_WIRELESS_TRACKER
        #define bigSizeFont     2
        #define smallSizeFont   1
        #define lineSpacing     12
    #endif
    #ifdef TTGO_T_DECK_GPS
        #define bigSizeFont     4
        #define smallSizeFont   2
        #define lineSpacing     22
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
                                "C", "a", "Y", "O", "'", "=", "y", "U", "p"};
int   symbolArraySize         = sizeof(symbolArray)/sizeof(symbolArray[0]);
const uint8_t *symbolsAPRS[]  = {runnerSymbol, carSymbol, jeepSymbol, bikeSymbol, motorcycleSymbol, shipSymbol, 
                                truck18Symbol, recreationalVehicleSymbol, vanSymbol, carsateliteSymbol, tentSymbol,
                                houseSymbol, truckSymbol, canoeSymbol, ambulanceSymbol, yatchSymbol, baloonSymbol,
                                aircraftSymbol, trainSymbol, yagiSymbol, busSymbol, dogSymbol};
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

String fillStringLength(const String& line, uint8_t length) {
    String outputLine = line;
    for (int a = line.length(); a < length; a++) {
        outputLine += " ";
    }
    return outputLine;
}

void displaySetup() {
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
        #else
            if (!display.begin(0x3c, false)) {
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "SH1106", "allocation failed!");
                while (true) {
                }
            }
        #endif
        if (Config.display.turn180) {
            display.setRotation(2);
        }
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
        #else
            display.setContrast(screenBrightness);
        #endif
        display.display();
    #endif
}

void displayToggle(bool toggle) {
    if (toggle) {
        #ifdef HAS_TFT
            digitalWrite(TFT_BL, HIGH);
        #else
            #ifdef ssd1306
                display.ssd1306_command(SSD1306_DISPLAYON); 
            #else
                display.oled_command(SH110X_DISPLAYON);
            #endif
        #endif
    } else {
        #ifdef HAS_TFT
            digitalWrite(TFT_BL, LOW);
        #else
            #ifdef ssd1306
                display.ssd1306_command(SSD1306_DISPLAYOFF);
            #else
                display.oled_command(SH110X_DISPLAYOFF);
            #endif
        #endif
    }
}

void displayShow(const String& header, const String& line1, const String& line2, int wait) {
    #ifdef HAS_TFT
        String filledLine1 = fillStringLength(line1, 22);
        String filledLine2 = fillStringLength(line2, 22);
        const String* const lines[] = {&filledLine1, &filledLine2};
        
        cleanTFT();
        tft.setTextColor(TFT_WHITE,TFT_BLACK);
        tft.setTextSize(bigSizeFont);
        tft.setCursor(0, 0);

        if (header != lastHeader) {
            tft.print(fillStringLength(header, 11));
            lastHeader = header;
        } else {
            tft.print(header);
        }

        tft.setTextSize(smallSizeFont);
        for (int i = 0; i < 2; i++) {
            tft.setCursor(0, ((lineSpacing * (2 + i)) - 2));
            tft.print(*lines[i]);
        }
    #else
        const String* const lines[] = {&line1, &line2};

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
        for (int i = 0; i < 2; i++) {
            display.setCursor(0, 16 + (10 * i));
            display.println(*lines[i]);
        }
        #ifdef ssd1306
            display.ssd1306_command(SSD1306_SETCONTRAST);
            display.ssd1306_command(screenBrightness);
        #else
            display.setContrast(screenBrightness);
        #endif
        display.display();
    #endif
    delay(wait);
}

void displayShow(const String& header, const String& line1, const String& line2, const String& line3, const String& line4, const String& line5, int wait) {
    #ifdef HAS_TFT
        String filledLine1  = fillStringLength(line1, 22);
        String filledLine2  = fillStringLength(line2, 22);
        String filledLine3  = fillStringLength(line3, 22);
        String filledLine4  = fillStringLength(line4, 22);
        String filledLine5  = fillStringLength(line5, 22);
        const String* const lines[] = {&filledLine1, &filledLine2, &filledLine3, &filledLine4, &filledLine5};

        if (menuDisplay != lastMenuDisplay) {
            lastMenuDisplay = menuDisplay;
            cleanTFT();
        }
        //tft.setTextColor(TFT_RED,TFT_BLACK);
        tft.setTextColor(TFT_WHITE,TFT_BLACK);
        tft.setTextSize(bigSizeFont);
        tft.setCursor(0, 0);

        if (header != lastHeader) {
            tft.print(fillStringLength(header, 11));
            lastHeader = header;
        } else {
            tft.print(header);
        }

        tft.setTextSize(smallSizeFont);
        for (int i = 0; i < 5; i++) {
            tft.setCursor(0, ((lineSpacing * (2 + i)) - 2));
            tft.print(*lines[i]);
        }
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
                    currentSymbol = symbolArray[symbol];
                    #if HELTEC_WIRELESS_TRACKER
                        if (currentSymbol != lastSymbol) {
                            tft.fillRect((TFT_WIDTH - SYMBOL_WIDTH + (128 - TFT_WIDTH)), 0, SYMBOL_WIDTH, SYMBOL_HEIGHT, TFT_BLACK);
                            lastSymbol = currentSymbol;
                        }
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
        const String* const lines[] = {&line1, &line2, &line3, &line4, &line5};

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
        for (int i = 0; i < 5; i++) {
            display.setCursor(0, 16 + (10 * i));
            display.println(*lines[i]);
        }
        #ifdef ssd1306
            display.ssd1306_command(SSD1306_SETCONTRAST);
            display.ssd1306_command(screenBrightness);
        #else
            display.setContrast(screenBrightness);
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
    displayShow(" LoRa APRS", "      (TRACKER)", workingFreq, "", "", "  CA2RXU  " + version, 4000);
    #ifdef HAS_TFT
        cleanTFT();
    #endif
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "RichonGuzman (CA2RXU) --> LoRa APRS Tracker/Station");
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Version: %s", version);
}

String fillMessageLine(const String& line, const int& length) {
    String completeLine = line;
    for (int i = 0; completeLine.length() <= length; i++) {
        completeLine = completeLine + " ";
    }
    return completeLine;
}

void displayMessage(const String& sender, const String& message, const int& lineLength, bool next, int wait) {
    String messageLine1, messageLine2, messageLine3;
    int messageLength = message.length();

    if (message.length() > 0) {
        messageLine1 = message.substring(0, min(lineLength, messageLength));
        if (messageLength > lineLength) {
            messageLine2 = message.substring(lineLength, min(2 * lineLength, messageLength));
            if (messageLength > 2 * lineLength) {
                messageLine3 = message.substring(2 * lineLength);
            }
        }
    }
    if (next) {
        String nextLine = fillMessageLine("Next=Down", lineLength);
        displayShow("MSG_APRS>", "From --> " + sender, fillMessageLine(messageLine1, lineLength), fillMessageLine(messageLine2, lineLength), fillMessageLine(messageLine3, lineLength), nextLine);
    } else {
        displayShow("< MSG Rx >", "From --> " + sender, "", fillMessageLine(messageLine1, lineLength) , fillMessageLine(messageLine2, lineLength), fillMessageLine(messageLine3, lineLength), wait);
    }
    
}