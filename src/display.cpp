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

    unsigned short      grays[13];      // ready to delete this?
    
    #ifdef HELTEC_WIRELESS_TRACKER
        #define bigSizeFont     2
        #define smallSizeFont   1
        #define lineSpacing     12
    #endif
    #if defined(TTGO_T_DECK_GPS) || defined(TTGO_T_DECK_PLUS)
        #define color1  TFT_BLACK
        #define color2  0x0249
        #define green   0x1B08

        #define bigSizeFont     4
        #define normalSizeFont  2
        #define smallSizeFont   1
        #define lineSpacing     22

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

    void draw_T_DECK_Top() {//const String& header, const String& datetime, const String& location) {  
        sprite.fillSprite(TFT_BLACK); 
        sprite.fillRect(0, 0, 320, 38, redColor);
        sprite.setTextFont(0);
        sprite.setTextSize(bigSizeFont);
        sprite.setTextColor(TFT_WHITE, redColor);
        sprite.drawString(topHeader1, 3, 5);
        
        sprite.setTextSize(smallSizeFont);
        sprite.setTextColor(TFT_WHITE, redColor);
        //String date = datetime.substring(0, datetime.indexOf("   "));
        sprite.drawString(topHeader1_1, 258, 5);
        //String time = datetime.substring(datetime.indexOf("   ") + 3);
        sprite.drawString("UTC:" + topHeader1_2, 246, 15);

        sprite.fillRect(0, 38, 320, 2, redColorDark);//TFT_ORANGE);

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

    void draw_T_DECK_Body(const String& line1, const String& line2, const String& line3, const String& line4, const String& line5, const String& line6) {
        
        sprite.setTextSize(normalSizeFont);
        sprite.setTextColor(TFT_WHITE, TFT_BLACK);

        int lineLength  = 22;
        int line3Length = line3.length();

        String line3Temp, line4Temp, line5Temp;
        if (line3.length() > 0 && line4 == "" && line5 == "") {
            line3Temp = line3.substring(0, lineLength);
            if (line3Length > lineLength) {
                line4Temp = line3.substring(lineLength, min(2 * lineLength, line3Length));
                if (line3Length > 2 * lineLength) {
                    line5Temp = line3.substring(2 * lineLength);
                }
            }
        } else {
            line3Temp = line3;
            line4Temp = line4;
            line5Temp = line5;
        }

        const String* const lines[] = {&line1, &line2, &line3Temp, &line4Temp, &line5Temp, &line6};
        for (int i = 0; i < 6; i++) {
            sprite.drawString(*lines[i], 35, 70 + (i * 20));
        }

        //drawButton(125, 210, 80, 28, "Menu", 0);

        drawButton(30,  210, 80, 28, "Send", 1);
        drawButton(125, 210, 80, 28, "Menu", 0);
        drawButton(220, 210, 80, 28, "Exit", 2);
        //}
    }

#endif
    
    //sprite.fillRect(0, 38, 320, 2, TFT_DARKGREY);
    //sprite.fillRect(0, 20,  320, 2,  color2);                                           // linea bajo techo
    //sprite.fillRect(0, 202, 320, 2,  0xBC81);                                           // linea abajo amarilla

    //sprite.fillSmoothRoundRect(  0, 218, 320, 22 , 2, redColor, TFT_BLACK);                  // piso
    
    //sprite.fillSmoothRoundRect(  4,   2,  56, 14 , 2, grays[6],     grays[9]);          // cuadrado gris izquierda arriba
    //sprite.fillSmoothRoundRect(  2,   2,  16, 14 , 2, TFT_BLUE,          grays[9]);     // cuadrado rojo izquierda arriba

    //sprite.fillSmoothRoundRect(272,   2,  40, 16 , 2, green,        grays[9]);          // bateria
    //sprite.fillSmoothRoundRect(308,   6,   8,  8 , 2, green,        grays[9]);          // bateria
    //sprite.fillSmoothRoundRect(275,   4,  34, 12 , 2, TFT_BLACK,    green);             // centro bateria

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

String fillStringLength(const String& line, uint8_t length) {
    String outputLine = line;
    for (int a = line.length(); a < length; a++) {
        outputLine += " ";
    }
    return outputLine;
}

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
        int co = 210;
        for (int i = 0; i < 13; i++) {
            grays[i] = tft.color565(co, co, co);
            co = co - 20;
        }
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
            String tftLine1, tftLine2, tftLine3, tftLine4;
            if (line1.length() > 22 && line2.length() > 22) {
                tftLine1 = line1.substring(0,22);
                tftLine2 = line1.substring(22);
                tftLine3 = line2.substring(0,22);
                tftLine4 = line2.substring(22);
            } else if (line1.length() > 22) {
                tftLine1 = line1.substring(0,22);
                tftLine2 = line1.substring(22);
                tftLine3 = line2;
                tftLine4 = "";
            } else if (line2.length() > 22) {
                tftLine1 = line1;
                tftLine2 = line2.substring(0,22);
                tftLine3 = line2.substring(22);
                tftLine4 = "";
            } else {
                tftLine1 = line1;
                tftLine2 = line2;
                tftLine3 = "";
                tftLine4 = "";
            }
            draw_T_DECK_Body(header, tftLine1, tftLine2, tftLine3, tftLine4, "");
        #endif
        #if defined(HELTEC_WIRELESS_TRACKER)
            sprite.fillSprite(TFT_BLACK); 
            sprite.fillRect(0, 0, 160, 19, redColor);
            sprite.setTextFont(0);
            sprite.setTextSize(bigSizeFont);
            sprite.setTextColor(TFT_WHITE, redColor);
            sprite.drawString(header, 3, 3);

            const String* const lines[] = {&line1, &line2};

            sprite.setTextSize(smallSizeFont);
            sprite.setTextColor(TFT_WHITE, TFT_BLACK);

            for (int i = 0; i < 2; i++) {
                sprite.drawString(*lines[i], 3, (lineSpacing * (2 + i)) - 2);
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
            draw_T_DECK_Body(header, line1, line2, line3, line4, line5);
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

            for (int i = 0; i < 5; i++) {
                sprite.drawString(*lines[i], 3, (lineSpacing * (2 + i)) - 2);
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

void displayMessage(const String& sender, const String& message, bool next, int wait) {
    String messageLine1, messageLine2, messageLine3;

    int messageLength   = message.length();
    int lineLength      = 0;
    #if defined(TTGO_T_DECK_GPS) || defined(TTGO_T_DECK_PLUS)
        lineLength = 22;
    #else   // Heltec Wireless Tracker
        lineLength = 26;
    #endif

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