#include "board_pinout.h"
#include "button_utils.h"
#include "touch_utils.h"

#ifdef HAS_TOUCHSCREEN

    #define TOUCH_MODULES_GT911
    #include <TouchLib.h>

    extern uint8_t touchModuleAddress;

    TouchLib    touch(Wire, BOARD_I2C_SDA, BOARD_I2C_SCL, 0x00);

    void (*lastCalledAction)() = nullptr;       // keep track of last calledAction from Touch

    extern      bool            sendUpdate;

    int16_t     xCalibratedMin  = 5;
    int16_t     xCalibratedMax  = 314;
    int16_t     yCalibratedMin  = 6;
    int16_t     yCalibratedMax  = 233;

    int16_t     xValueMax       = 320;
    int16_t     yValueMax       = 240;

    int         touchDebounce   = 300;
    uint32_t    lastTouchTime   = 0;

    int16_t     xlastValue      = 0;
    int16_t     ylastValue      = 0;

    extern int menuDisplay;


    namespace TOUCH_Utils {

        void sendBeaconFromTouch() { sendUpdate = true;}

        void enterMenuFromTouch() { BUTTON_Utils::doublePress();}

        void exitFromTouch() {
            menuDisplay = 0;
            //Serial.println("CANCEL BUTTON PRESSED");
        }

        TouchButton touchButtons_0[] = {
            {30,  110,   0,  28, "Send",    1, sendBeaconFromTouch},    // Button Send  //drawButton(30,  210, 80, 28, "Send", 1);
            {125, 205,   0,  28, "Menu",    0, enterMenuFromTouch},     // Button Menu  //drawButton(125, 210, 80, 28, "Menu", 0);
            {210, 305,   0,  28, "Exit",    2, exitFromTouch}           // Button Exit  //drawButton(210, 210, 95, 28, "Exit", 2);
        };
        

        bool touchButtonPressed(int touchX, int touchY, int Xmin, int Xmax, int Ymin, int Ymax) {
            return (touchX >= (Xmin - 5) && touchX <= (Xmax + 5) && touchY >= (Ymin - 5) && touchY <= (Ymax + 5));
        }
        
        void checkLiveButtons(uint16_t x, uint16_t y) {
            for (int i = 0; i < sizeof(touchButtons_0) / sizeof(touchButtons_0[0]); i++) {
                if (touchButtonPressed(x, y, touchButtons_0[i].Xmin, touchButtons_0[i].Xmax, touchButtons_0[i].Ymin, touchButtons_0[i].Ymax)) {

                    if (touchButtons_0[i].action != nullptr && touchButtons_0[i].action != lastCalledAction) {                      // Call the action function associated with the button
                        Serial.println(touchButtons_0[i].label + " pressed");
                        touchButtons_0[i].action();                     // Call the function pointer
                        lastCalledAction = touchButtons_0[i].action;    // Update the last called action
                    } else {
                        Serial.println("No action assigned to this button!");
                    }
                }
            }
        }

        void loop() {
            if (touch.read() && (millis() - lastTouchTime > touchDebounce)) {
                TP_Point touchPoint = touch.getPoint(0);
                uint16_t xValueTouched = map(touchPoint.y, xCalibratedMin, xCalibratedMax, 0, xValueMax);   // x and y values are inverted because
                uint16_t yValueTouched = map(touchPoint.x, yCalibratedMin, yCalibratedMax, 0, yValueMax);   // TFT screen is rotated!!!!
                lastTouchTime = millis();
                //Serial.print(" X="); Serial.print(xValueTouched); Serial.print("  Y="); Serial.println(yValueTouched);
                checkLiveButtons(xValueTouched, yValueTouched);
            }
            if (millis() - lastTouchTime > 1000) lastCalledAction = nullptr;    // reset touchButton when staying in same menu (like Tx/Send)
        }

        void setup() {
            if (touchModuleAddress != 0x00) {
                if (touchModuleAddress == 0x14) {
                    touch = TouchLib(Wire, BOARD_I2C_SDA, BOARD_I2C_SCL, GT911_SLAVE_ADDRESS2);
                    touch.init();
                } else if (touchModuleAddress == 0x5d) {
                    touch = TouchLib(Wire, BOARD_I2C_SDA, BOARD_I2C_SCL, GT911_SLAVE_ADDRESS1);
                    touch.init();
                } else {
                    Serial.println("No Touch Module Address found");
                }
            }
        }  

    }

#endif