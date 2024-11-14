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

    TFT_eSPI    tft     = TFT_eSPI(); 
    TFT_eSprite sprite  = TFT_eSprite(&tft);

    #define red     0xB061
    
    #ifdef HELTEC_WIRELESS_TRACKER
        #define bigSizeFont     2
        #define smallSizeFont   1
        #define lineSpacing     12
    #endif
    #if defined(TTGO_T_DECK_GPS) || defined(TTGO_T_DECK_PLUS)

        int             brightnessValues[6]     = {70, 90, 120, 160, 200, 250};
        int             tftBrightness           = 5;
        unsigned short  grays[13];

        #define color1  TFT_BLACK
        #define color2  0x0249
        
        #define green   0x1B08

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
    #if defined(HELTEC_V3_GPS) || defined(HELTEC_V3_TNC)
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

#ifdef HAS_TFT
void drawTop(const String& header, const String& datetime, const String& location) {  
    sprite.fillSprite(TFT_BLACK); 
    sprite.fillRect(0, 0, 320, 38, red);
    sprite.setTextFont(0);
    sprite.setTextSize(4);
    sprite.setTextColor(TFT_WHITE, red);
    sprite.drawString(currentBeacon->callsign, 3, 5);
    
    sprite.setTextSize(1);
    sprite.setTextColor(TFT_WHITE, red);
    String date = datetime.substring(0, datetime.indexOf("   "));
    sprite.drawString(date, 258, 5);
    String time = datetime.substring(datetime.indexOf("   ") + 3);
    sprite.drawString("UTC:" + time, 246, 15);

    sprite.fillRect(0, 38, 320, 2, TFT_ORANGE);

    sprite.fillRect(0, 40, 320, 22, TFT_DARKGREY);
    sprite.setTextSize(2);
    sprite.setTextColor(TFT_WHITE, TFT_DARKGREY);
    sprite.drawString(location, 8, 44);
}

void drawBody(const String& linea1, const String& linea2, const String& linea3, const String& linea4, const String& linea5, const String& linea6) {
    sprite.setTextSize(2);
    sprite.setTextColor(TFT_WHITE, TFT_BLACK);

    sprite.drawString(linea1, 3, 70);
    sprite.drawString(linea2, 3, 90);
    sprite.drawString(linea3, 3, 110);
    sprite.drawString(linea4, 3, 130);
    sprite.drawString(linea5, 3, 150);
    sprite.drawString(linea6, 3, 170);
}
#endif
    
    /*String lat = location.substring(0, location.indexOf(" "));
    sprite.drawString(lat, 3, 46);
    String temp = location.substring(location.indexOf(" ") + 1);
    String lng = temp.substring(0, temp.indexOf(" "));
    sprite.drawString(lng, 126, 46);
    String sat = temp.substring(temp.indexOf(" ") + 1);
    sprite.setTextSize(1);
    sprite.drawString(sat, 290, 25);*/

    
    //sprite.fillRect(0, 38, 320, 2, TFT_DARKGREY);
    //sprite.fillRect(0, 20,  320, 2,  color2);                                           // linea bajo techo
    //sprite.fillRect(0, 202, 320, 2,  0xBC81);                                           // linea abajo amarilla

    //sprite.fillSmoothRoundRect(  0, 218, 320, 22 , 2, red, TFT_BLACK);                  // piso
    
    //sprite.fillSmoothRoundRect(  4,   2,  56, 14 , 2, grays[6],     grays[9]);          // cuadrado gris izquierda arriba
    //sprite.fillSmoothRoundRect(  2,   2,  16, 14 , 2, TFT_BLUE,          grays[9]);     // cuadrado rojo izquierda arriba

    //sprite.fillSmoothRoundRect(272,   2,  40, 16 , 2, green,        grays[9]);          // bateria
    //sprite.fillSmoothRoundRect(308,   6,   8,  8 , 2, green,        grays[9]);          // bateria
    //sprite.fillSmoothRoundRect(275,   4,  34, 12 , 2, TFT_BLACK,    green);             // centro bateria

    /*for (int i = 0; i < 5; i++) {                                                      // cubos que muestran el brillo (abajo a la derecha)
        if(i < tftBrightness) {
            sprite.fillRect(282+(i*8), 207, 5, 8, grays[3]);
        } else {
            sprite.fillRect(282+(i*8), 207, 5, 8, grays[7]);
        }
    }*/
    
    //for (int i = 0; i < 9; i++) sprite.drawFastHLine(4, 38+(i*18), 312, grays[8]);      // draw horizonatl lines
    
    
    /*sprite.setTextFont(0);
    sprite.setTextSize(1);
    sprite.setTextColor(TFT_WHITE, TFT_BLUE);
    sprite.drawString("LoRa", 6, 4);
    sprite.setTextColor(TFT_BLACK, grays[6]);
    sprite.drawString("APRS", 21, 4);           // escribir DECK en x=21 , y=4*/
    
    

    /*sprite.setTextColor(grays[1],c olor2);
    sprite.drawString(notice, 6, 223, 2);

    for (int i = 0; i < nMsg + 1; i++) {
        if (msg[i].length() > 0) {
            if (writer[i] == 1) {
                sprite.setTextColor(grays[1], color1); else sprite.setTextColor(0x663C, color1);
                sprite.drawString(msg[i], 6, 25 + (i*18), 2);
            }
        }
    }

    sprite.setTextColor(grays[5], TFT_BLACK);
    sprite.unloadFont(); 
    sprite.drawString("Your ID: " + name, 2, 208);
    sprite.setTextColor(grays[4],grays[9]);
    sprite.drawString("VOLOS", 210, 2);
    sprite.setTextColor(grays[5], grays[9]);
    sprite.drawString("projects", 210, 9);
    sprite.setTextColor(grays[2], TFT_BLACK);
    sprite.drawString(String(analogRead(4)), 280, 7);

    sprite.setTextColor(grays[8], color2);
    sprite.drawString("ENTER YOUR MESSAGE", 200, 226);

    sprite.setTextColor(grays[7], TFT_BLACK);
    sprite.drawString("SND:", 120, 208);
    sprite.drawString(String(sndN), 145, 8);

    sprite.drawString("REC:",190,208);
    sprite.drawString(String(recN), 215, 208);*/

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
        #if defined(TTGO_T_DECK_PLUS)
            tft.init();
            tft.begin();
            if (Config.display.turn180) {
                    tft.setRotation(3);
            } else {
                tft.setRotation(1);
            }
            analogWrite(BOARD_BL_PIN, brightnessValues[tftBrightness]);
            tft.setTextFont(0);
            tft.fillScreen(TFT_BLACK);

            sprite.createSprite(320,240);

            int co = 210;
            for (int i = 0; i < 13; i++) {
                grays[i] = tft.color565(co, co, co);
                co = co - 20;
            }
        #else
            tft.init();
            tft.begin();
            if (Config.display.turn180) {
                    tft.setRotation(3);
            } else {
                tft.setRotation(1);
            }
            tft.setTextFont(0);
            tft.fillScreen(TFT_BLACK);
        #endif
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
        if (Config.display.turn180) display.setRotation(2);
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
        #if defined(TTGO_T_DECK_PLUS)
            drawTop(header, line1, line2);
            sprite.pushSprite(0,0);
        #else
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
        #endif
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
        #if defined(TTGO_T_DECK_PLUS)
            drawTop(header, line1, line2);
            drawBody(header, line1, line2, line3, line4, line5);
            sprite.pushSprite(0,0);
        #else
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
                        #if defined(HELTEC_WIRELESS_TRACKER)
                            if (currentSymbol != lastSymbol) {
                                tft.fillRect((TFT_WIDTH - SYMBOL_WIDTH + (128 - TFT_WIDTH)), 0, SYMBOL_WIDTH, SYMBOL_HEIGHT, TFT_BLACK);
                                lastSymbol = currentSymbol;
                            }
                            tft.drawBitmap((TFT_WIDTH - SYMBOL_WIDTH + (128 - TFT_WIDTH)), 0, symbolsAPRS[symbol], SYMBOL_WIDTH, SYMBOL_HEIGHT, TFT_WHITE);//, TFT_RED);
                        #endif
                        #if defined(TTGO_T_DECK_GPS) || defined(TTGO_T_DECK_PLUS)
                            tft.drawBitmap((TFT_WIDTH - SYMBOL_WIDTH), 0, symbolsAPRS[symbol], SYMBOL_WIDTH, SYMBOL_HEIGHT, TFT_WHITE);//, TFT_RED);
                        #endif
                    }
                } else if (bluetoothConnected) {    // TODO In this case, the text symbol stay displayed due to symbolAvailable false in menu_utils
                    #if defined(HELTEC_WIRELESS_TRACKER)
                        tft.drawBitmap((TFT_WIDTH - SYMBOL_WIDTH + (128 - TFT_WIDTH)), 0, bluetoothSymbol, SYMBOL_WIDTH, SYMBOL_HEIGHT, TFT_WHITE);
                    #endif
                    #if defined(TTGO_T_DECK_GPS) || defined(TTGO_T_DECK_PLUS)
                        tft.drawBitmap((TFT_WIDTH - SYMBOL_WIDTH), 0, bluetoothSymbol, SYMBOL_WIDTH, SYMBOL_HEIGHT, TFT_WHITE);
                    #endif
                }
            }
        #endif
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