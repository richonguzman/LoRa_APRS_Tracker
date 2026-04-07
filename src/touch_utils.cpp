/* Copyright (C) 2025 Ricardo Guzman - CA2RXU
 * 
 * This file is part of LoRa APRS Tracker.
 * 
 * LoRa APRS Tracker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or 
 * (at your option) any later version.
 * 
 * LoRa APRS Tracker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with LoRa APRS Tracker. If not, see <https://www.gnu.org/licenses/>.
 */

#include <esp_log.h>
#include "configuration.h"
#include "board_pinout.h"
#include "button_utils.h"
#include "touch_utils.h"
#include "display.h"  // for tft (LovyanGFX)

static const char *TAG = "Touch";

#ifdef HAS_TOUCHSCREEN

    extern Configuration    Config;

    void (*lastCalledAction)() = nullptr;       // keep track of last calledAction from Touch

    extern      bool            sendUpdate;

    int         touchDebounce   = 300;
    uint32_t    lastTouchTime   = 0;

    extern int menuDisplay;


    namespace TOUCH_Utils {

        void sendBeaconFromTouch() { sendUpdate = true;}

        void enterMenuFromTouch() { BUTTON_Utils::doublePress1();}

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
                        ESP_LOGD(TAG, "%s pressed", touchButtons_0[i].label.c_str());
                        touchButtons_0[i].action();                     // Call the function pointer
                        lastCalledAction = touchButtons_0[i].action;    // Update the last called action
                    } else {
                        ESP_LOGW(TAG, "No action assigned to this button");
                    }
                }
            }
        }

        void loop() {
            uint16_t x, y;
            if (tft.getTouch(&x, &y) && (millis() - lastTouchTime > touchDebounce)) {
                lastTouchTime = millis();
                checkLiveButtons(x, y);
            }
            if (millis() - lastTouchTime > 1000) lastCalledAction = nullptr;    // reset touchButton when staying in same menu (like Tx/Send)
        }

        void setup() {
            // Touch is initialized natively by LovyanGFX (GT911 configured in LGFX classes)
            ESP_LOGI(TAG, "Touch managed by LovyanGFX");
        }

    }

#endif