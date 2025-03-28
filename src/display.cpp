#include <logger.h>
#include <Wire.h>
#include "custom_characters.h"
#include "custom_colors.h"
#include "configuration.h"
#include "station_utils.h"
#include "board_pinout.h"
#include "display.h"
#include "TimeLib.h"


#ifdef HAS_TFT
    #include <TFT_eSPI.h>

    TFT_eSPI    tft     = TFT_eSPI(); 
    TFT_eSprite sprite  = TFT_eSprite(&tft);
    
    #ifdef HELTEC_WIRELESS_TRACKER
        #define bigSizeFont     2
        #define smallSizeFont   1
        #define lineSpacing     12
        #define maxLineLength   26
    #endif
    #if defined(TTGO_T_DECK_GPS) || defined(TTGO_T_DECK_PLUS)
        #define color1  TFT_BLACK
        #define color2  0x0249
        #define green   0x1B08

        #define bigSizeFont     4
        #define normalSizeFont  2
        #define smallSizeFont   1
        #define lineSpacing     20
        #define maxLineLength   22

        extern String topHeader1;
        extern String topHeader1_1;
        extern String topHeader1_2;
        extern String topHeader1_3;
        extern String topHeader2;
    #endif
#else
    #include <Adafruit_GFX.h>

    #define ssd1306 //comment this line with "//" when using SH1106 screen instead of SSD1306

    #if defined(TTGO_T_Beam_S3_SUPREME_V3)
        #undef ssd1306
    #endif
    #if defined(HELTEC_V3_GPS) || defined(HELTEC_V3_TNC) || defined(HELTEC_V3_2_GPS) || defined(HELTEC_V3_2_TNC)
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
                                "C", "a", "Y", "O", "'", "=", "y", "U", "p", "_"};
int   symbolArraySize         = sizeof(symbolArray)/sizeof(symbolArray[0]);
const uint8_t *symbolsAPRS[]  = {runnerSymbol, carSymbol, jeepSymbol, bikeSymbol, motorcycleSymbol, shipSymbol, 
                                truck18Symbol, recreationalVehicleSymbol, vanSymbol, carsateliteSymbol, tentSymbol,
                                houseSymbol, truckSymbol, canoeSymbol, ambulanceSymbol, yatchSymbol, baloonSymbol,
                                aircraftSymbol, trainSymbol, yagiSymbol, busSymbol, dogSymbol, wxSymbol};
// T-Beams bought with soldered OLED Screen comes with only 4 pins (VCC, GND, SDA, SCL)
// If your board didn't come with 4 pins OLED Screen and comes with 5 and one of them is RST...
// Uncomment Next Line (Remember ONLY if your OLED Screen has a RST pin). This is to avoid memory issues.
//#define OLED_DISPLAY_HAS_RST_PIN

int         lastMenuDisplay         = 0;
uint8_t     screenBrightness        = 1;    //from 1 to 255 to regulate brightness of screens
bool        symbolAvailable         = true;

extern logging::Logger logger;


#if defined(HAS_TFT) && (defined(TTGO_T_DECK_GPS) || defined(TTGO_T_DECK_PLUS))
    void drawButton(int xPos, int yPos, int wide, int height, String buttonText, int color) {
        uint16_t baseColor, lightColor, darkColor;
        switch (color) {
            case 0:     // Grey Theme
                baseColor   = greyColor;
                lightColor  = greyColorLight;
                darkColor   = greyColorDark;
                break;
            case 1:     // Green Theme
                baseColor   = greenColor;
                lightColor  = greenColorLight;
                darkColor   = greenColorDark;
                break;
            case 2:     // Red Theme
                baseColor   = redColor;
                lightColor  = redColorLight;
                darkColor   = redColorDark;
                break;
            default:    // Fallback color
                baseColor   = 0x0000;   // Black
                lightColor  = 0xFFFF;   // White
                darkColor   = 0x0000;   // Black
                break;
        }

        sprite.fillRect(xPos, yPos, wide, height, baseColor);           // Dibuja el fondo del botón
        sprite.fillRect(xPos, yPos + height - 2, wide, 2, darkColor);   // Línea inferior
        sprite.fillRect(xPos, yPos, wide, 2, lightColor);               // Línea superior
        sprite.fillRect(xPos, yPos, 2, height, lightColor);             // Línea izquierda
        sprite.fillRect(xPos + wide - 2, yPos, 2, height, darkColor);   // Línea derecha
        
        sprite.setTextSize(2);
        sprite.setTextColor(TFT_WHITE, baseColor);

        // Calcula la posición del texto para que esté centrado
        int textWidth = sprite.textWidth(buttonText);           // Ancho del texto
        int textHeight = 16;                                    // Altura aproximada (depende de `setTextSize`)
        int textX = xPos + (wide - textWidth) / 2;              // Centrado horizontal
        int textY = yPos + (height - textHeight) / 2;           // Centrado vertical

        sprite.drawString(buttonText, textX, textY);
    }

    void draw_T_DECK_Top() {
        sprite.fillSprite(TFT_BLACK); 
        sprite.fillRect(0, 0, 320, 38, redColor);
        sprite.setTextFont(0);
        sprite.setTextSize(bigSizeFont);
        sprite.setTextColor(TFT_WHITE, redColor);
        sprite.drawString(topHeader1, 3, 5);
        
        sprite.setTextSize(smallSizeFont);
        sprite.setTextColor(TFT_WHITE, redColor);
        sprite.drawString(topHeader1_1, 258, 5);
        sprite.drawString("UTC:" + topHeader1_2, 246, 15);

        sprite.fillRect(0, 38, 320, 2, redColorDark);

        sprite.fillRect(0, 40, 320, 2, greyColorLight);
        sprite.fillRect(0, 42, 320, 20, greyColor);
        sprite.setTextSize(2);
        sprite.setTextColor(TFT_WHITE, greyColor);
        sprite.drawString(topHeader2, 8, 44);
        sprite.fillRect(0, 60, 320, 2, greyColorDark);
    }

    void draw_T_DECK_MenuButtons(int menu) {
        int ladoCuadrado            = 45;
        int curvaCuadrado           = 8;
        int espacioEntreCuadrados   = 18;
        int margenLineaCuadrados    = 10;
        int alturaPrimeraLinea      = 75;
        int alturaSegundaLinea      = 145;
        int16_t colorCuadrados      = 0x2925;
        int16_t colorDestacado      = greyColor;

        for (int i = 0; i < 5; i++) {
            if (i == menu - 1) {
                sprite.fillRoundRect(
                    margenLineaCuadrados + (i * (ladoCuadrado + espacioEntreCuadrados)) - 1,
                    alturaPrimeraLinea - 1,
                    ladoCuadrado + 2,
                    ladoCuadrado + 2,
                    curvaCuadrado,
                    TFT_WHITE
                );
                sprite.fillRoundRect(
                    margenLineaCuadrados + (i * (ladoCuadrado + espacioEntreCuadrados)),
                    alturaPrimeraLinea,
                    ladoCuadrado,
                    ladoCuadrado,
                    curvaCuadrado,
                    TFT_BLACK
                );
                sprite.fillRoundRect(
                    margenLineaCuadrados + (i * (ladoCuadrado + espacioEntreCuadrados)),    // x-coordinate
                    alturaPrimeraLinea,                                                     // y-coordinate
                    ladoCuadrado,                                                           // width
                    ladoCuadrado,                                                           // height
                    curvaCuadrado,                                                          // corner radius
                    colorDestacado                                                          // color
                );
            } else {
                sprite.fillRoundRect(
                    margenLineaCuadrados + (i * (ladoCuadrado + espacioEntreCuadrados)),    // x-coordinate
                    alturaPrimeraLinea,                                                     // y-coordinate
                    ladoCuadrado,                                                           // width
                    ladoCuadrado,                                                           // height
                    curvaCuadrado,                                                          // corner radius
                    colorCuadrados                                                          // color
                );
            }
            sprite.fillRoundRect(
                margenLineaCuadrados + (i * (ladoCuadrado + espacioEntreCuadrados)),    // x-coordinate
                alturaSegundaLinea,                                                     // y-coordinate
                ladoCuadrado,                                                           // width
                ladoCuadrado,                                                           // height
                curvaCuadrado,                                                          // corner radius
                colorCuadrados                                                          // color
            );
        }
    }

#endif   

void displaySetup() {
    delay(500);
    STATION_Utils::loadIndex(2);    // Screen Brightness value
    #ifdef HAS_TFT
        tft.init();
        tft.begin();
        if (Config.display.turn180) {
                tft.setRotation(3);
        } else {
            tft.setRotation(1);
        }
        pinMode(TFT_BL, OUTPUT);
        analogWrite(TFT_BL, screenBrightness);
        tft.setTextFont(0);
        tft.fillScreen(TFT_BLACK);
        #if defined(TTGO_T_DECK_GPS) || defined(TTGO_T_DECK_PLUS)
            sprite.createSprite(320,240);
        #else
            sprite.createSprite(160,80);
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
                while (true) {}
            }
        #else
            if (!display.begin(0x3c, false)) {
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "SH1106", "allocation failed!");
                while (true) {}
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
            analogWrite(TFT_BL, screenBrightness);
        #else
            #ifdef ssd1306
                display.ssd1306_command(SSD1306_DISPLAYON); 
            #else
                display.oled_command(SH110X_DISPLAYON);
            #endif
        #endif
    } else {
        #ifdef HAS_TFT
            analogWrite(TFT_BL, 0);
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
        #if defined(TTGO_T_DECK_GPS) || defined(TTGO_T_DECK_PLUS)
            draw_T_DECK_Top();

            sprite.setTextSize(normalSizeFont);
            sprite.setTextColor(TFT_WHITE, TFT_BLACK);

            const String* const lines[] = {&header, &line1, &line2};
            int yLineOffset = 70;

            for (int i = 0; i < 3; i++) {
                String text = *lines[i];
                if (text.length() > 0) {
                    while (text.length() > 0) {
                        String chunk = text.substring(0, maxLineLength);
                        sprite.drawString(chunk, 35, yLineOffset);
                        text = text.substring(maxLineLength);
                        yLineOffset += lineSpacing;
                    }
                } else {
                    sprite.drawString(text, 3, yLineOffset);
                    yLineOffset += lineSpacing;
                }
            }
        #endif
        #if defined(HELTEC_WIRELESS_TRACKER)
            sprite.fillSprite(TFT_BLACK);
            sprite.fillRect(0, 0, 160, 19, TFT_YELLOW);
            sprite.setTextFont(0);
            sprite.setTextSize(bigSizeFont);
            sprite.setTextColor(TFT_BLACK, TFT_YELLOW);
            sprite.drawString(header, 3, 3);

            const String* const lines[] = {&line1, &line2};

            sprite.setTextSize(smallSizeFont);
            sprite.setTextColor(TFT_WHITE, TFT_BLACK);

            int yLineOffset = (lineSpacing * 2) - 2;

            for (int i = 0; i < 2; i++) {
                String text = *lines[i];
                if (text.length() > 0) {                    
                    while (text.length() > 0) {
                        String chunk = text.substring(0, maxLineLength);
                        sprite.drawString(chunk, 3, yLineOffset);
                        text = text.substring(maxLineLength);
                        yLineOffset += lineSpacing;
                    }
                } else {
                    sprite.drawString(text, 3, yLineOffset);
                    yLineOffset += lineSpacing;
                }
            }
        #endif
        sprite.pushSprite(0,0);
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

void drawSymbol(int symbolIndex, bool bluetoothActive) {
    const uint8_t *bitMap = symbolsAPRS[symbolIndex];
    #ifdef HAS_TFT
        if (bluetoothActive) bitMap = bluetoothSymbol;
        #if defined(HELTEC_WIRELESS_TRACKER)
            sprite.drawBitmap(128 - SYMBOL_WIDTH, 3, bitMap, SYMBOL_WIDTH, SYMBOL_HEIGHT, TFT_WHITE);
        #endif
        #if defined(TTGO_T_DECK_GPS) || defined(TTGO_T_DECK_PLUS)
            sprite.drawBitmap(280, 70, bitMap, SYMBOL_WIDTH, SYMBOL_HEIGHT, TFT_WHITE);
        #endif
    #else
        display.drawBitmap((display.width() - SYMBOL_WIDTH), 0, bitMap, SYMBOL_WIDTH, SYMBOL_HEIGHT, 1);
    #endif
}

void displayShow(const String& header, const String& line1, const String& line2, const String& line3, const String& line4, const String& line5, int wait) {
    #ifdef HAS_TFT
        #if defined(TTGO_T_DECK_GPS) || defined(TTGO_T_DECK_PLUS)
            draw_T_DECK_Top();
            sprite.setTextSize(normalSizeFont);
            sprite.setTextColor(TFT_WHITE, TFT_BLACK);

            const String* const lines[] = {&header, &line1, &line2, &line3, &line4, &line5};
            int yLineOffset = 70;

            for (int i = 0; i < 6; i++) {
                String text = *lines[i];
                if (text.length() > 0) {
                    while (text.length() > 0) {
                        String chunk = text.substring(0, maxLineLength);
                        sprite.drawString(chunk, 35, yLineOffset);
                        text = text.substring(maxLineLength);
                        yLineOffset += lineSpacing;
                    }
                } else {
                    sprite.drawString(text, 3, yLineOffset);
                    yLineOffset += lineSpacing;
                }
            }

            drawButton(30,  210, 80, 28, "Send", 1);
            drawButton(125, 210, 80, 28, "Menu", 0);
            drawButton(220, 210, 80, 28, "Exit", 2);
        #endif
        #if defined(HELTEC_WIRELESS_TRACKER)
            sprite.fillSprite(TFT_BLACK); 
            sprite.fillRect(0, 0, 160, 19, redColor);
            sprite.setTextFont(0);
            sprite.setTextSize(bigSizeFont);
            sprite.setTextColor(TFT_WHITE, redColor);
            sprite.drawString(header, 3, 3);

            const String* const lines[] = {&line1, &line2, &line3, &line4, &line5};

            sprite.setTextSize(smallSizeFont);
            sprite.setTextColor(TFT_WHITE, TFT_BLACK);

            int yLineOffset = (lineSpacing * 2) - 2;

            for (int i = 0; i < 5; i++) {
                String text = *lines[i];
                if (text.length() > 0) {
                    while (text.length() > 0) {
                        String chunk = text.substring(0, maxLineLength);
                        sprite.drawString(chunk, 3, yLineOffset);
                        text = text.substring(maxLineLength);
                        yLineOffset += lineSpacing;
                    }
                } else {
                    sprite.drawString(text, 3, yLineOffset);
                    yLineOffset += lineSpacing;
                }
            }
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

                /*  Symbol alternate every 5s
                *   If bluetooth is disconnected or if we are in the first part of the clock, then we show the APRS symbol
                *   Otherwise, we are in the second part of the clock, then we show BT connected */
            
                const auto time_now = now();
                if (!bluetoothConnected || time_now % 10 < 5) {
                    if (symbolAvailable) drawSymbol(symbol, false);
                } else if (bluetoothConnected) {    // TODO In this case, the text symbol stay displayed due to symbolAvailable false in menu_utils
                    drawSymbol(symbol, true);
                }
            }
        sprite.pushSprite(0,0);
    #else
        const String* const lines[] = {&line1, &line2, &line3, &line4, &line5};

        display.clearDisplay();
        #ifdef ssd1306
            display.setTextColor(WHITE);
            display.drawLine(0, 16, 128, 16, WHITE);
            display.drawLine(0, 17, 128, 17, WHITE);
        #else
            display.setTextColor(SH110X_WHITE);
            display.drawLine(0, 16, 128, 16, SH110X_WHITE);
            display.drawLine(0, 17, 128, 17, SH110X_WHITE);
        #endif
        display.setTextSize(2);
        display.setCursor(0, 0);
        display.println(header);
        display.setTextSize(1);
        for (int i = 0; i < 5; i++) {
            display.setCursor(0, 20 + (9 * i));
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
                if (symbolAvailable) drawSymbol(symbol, false);
            } else if (bluetoothConnected) {    // TODO In this case, the text symbol stay displayed due to symbolAvailable false in menu_utils
                drawSymbol(symbol, true);
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