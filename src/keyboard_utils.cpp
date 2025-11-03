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

#include <Arduino.h>
#include "platform_compat.h"
#include <logger.h>

// 简单的Configuration类前向声明
class Configuration {
public:
    struct Keyboard {
        int pinUp;
        int pinDown;
        int pinSelect;
        int debounceMs;
    };
    Keyboard keyboard;
};

// 全局配置对象外部声明
extern Configuration Config;

// 简单的按键状态结构体
typedef struct {
    bool upPressed;
    bool downPressed;
    bool selectPressed;
    unsigned long upDebounce;
    unsigned long downDebounce;
    unsigned long selectDebounce;
} ButtonState;

// 按键状态变量
ButtonState buttonState = {
    false, false, false,
    0, 0, 0
};

// 简单的KEYBOARD_Utils命名空间
namespace KEYBOARD_Utils {
    
    // 初始化键盘
    void initKeyboard() {
        Serial.println("Initializing keyboard");
        
        // 设置按键引脚为输入
        pinMode(Config.keyboard.pinUp, INPUT_PULLUP);
        pinMode(Config.keyboard.pinDown, INPUT_PULLUP);
        pinMode(Config.keyboard.pinSelect, INPUT_PULLUP);
        
        // 记录初始化日志
        logger.log(LOG_LEVEL_INFO, "KEYBOARD", "Keyboard initialized");
    }
    
    // 检查按键状态
    void checkButtons() {
        unsigned long currentMillis = millis();
        
        // 检查向上按键
        if (digitalRead(Config.keyboard.pinUp) == LOW) {
            if (!buttonState.upPressed && 
                (currentMillis - buttonState.upDebounce > Config.keyboard.debounceMs)) {
                buttonState.upPressed = true;
                buttonState.upDebounce = currentMillis;
                Serial.println("Up button pressed");
                logger.log(LOG_LEVEL_INFO, "KEYBOARD", "Up button pressed");
            }
        } else {
            buttonState.upPressed = false;
        }
        
        // 检查向下按键
        if (digitalRead(Config.keyboard.pinDown) == LOW) {
            if (!buttonState.downPressed && 
                (currentMillis - buttonState.downDebounce > Config.keyboard.debounceMs)) {
                buttonState.downPressed = true;
                buttonState.downDebounce = currentMillis;
                Serial.println("Down button pressed");
                logger.log(LOG_LEVEL_INFO, "KEYBOARD", "Down button pressed");
            }
        } else {
            buttonState.downPressed = false;
        }
        
        // 检查选择按键
        if (digitalRead(Config.keyboard.pinSelect) == LOW) {
            if (!buttonState.selectPressed && 
                (currentMillis - buttonState.selectDebounce > Config.keyboard.debounceMs)) {
                buttonState.selectPressed = true;
                buttonState.selectDebounce = currentMillis;
                Serial.println("Select button pressed");
                logger.log(LOG_LEVEL_INFO, "KEYBOARD", "Select button pressed");
            }
        } else {
            buttonState.selectPressed = false;
        }
    }
    
    // 获取向上按键状态
    bool isUpPressed() {
        return buttonState.upPressed;
    }
    
    // 获取向下按键状态
    bool isDownPressed() {
        return buttonState.downPressed;
    }
    
    // 获取选择按键状态
    bool isSelectPressed() {
        return buttonState.selectPressed;
    }
    
    // 重置所有按键状态
    void resetButtons() {
        buttonState.upPressed = false;
        buttonState.downPressed = false;
        buttonState.selectPressed = false;
    }
}