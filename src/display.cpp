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
#include <Wire.h>

// 简单的Configuration类前向声明
class Configuration {
public:
    struct Display {
        bool turn180;
        int brightness;
    };
    Display display;
};

// 全局配置对象外部声明
extern Configuration Config;

// 简单的Station_Utils命名空间前向声明
namespace STATION_Utils {
    void loadIndex(int index);
}

// 显示相关常量
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// 屏幕亮度变量
uint8_t screenBrightness = 1;    //from 1 to 255 to regulate brightness of screens

// 显示设备前向声明
class DisplayDevice {
public:
    virtual void init() = 0;
    virtual void clear() = 0;
    virtual void setTextSize(int size) = 0;
    virtual void setTextColor(uint16_t color) = 0;
    virtual void setCursor(int x, int y) = 0;
    virtual void println(const String &text) = 0;
    virtual void display() = 0;
    virtual void setRotation(int rotation) = 0;
    virtual void setBrightness(uint8_t brightness) = 0;
    virtual void powerOn() = 0;
    virtual void powerOff() = 0;
    virtual bool isInitialized() = 0;
    virtual ~DisplayDevice() {}
};

// 简单的空显示设备实现
class NullDisplay : public DisplayDevice {
private:
    bool initialized = false;
public:
    void init() override {
        logger.log(LOG_LEVEL_INFO, "DISPLAY", "Initializing null display");
        initialized = true;
    }
    
    void clear() override {
        logger.log(LOG_LEVEL_DEBUG, "DISPLAY", "Clear display");
    }
    
    void setTextSize(int size) override {
        logger.log(LOG_LEVEL_DEBUG, "DISPLAY", "Set text size: %d", size);
    }
    
    void setTextColor(uint16_t color) override {
        logger.log(LOG_LEVEL_DEBUG, "DISPLAY", "Set text color: 0x%04X", color);
    }
    
    void setCursor(int x, int y) override {
        logger.log(LOG_LEVEL_DEBUG, "DISPLAY", "Set cursor: (%d,%d)", x, y);
    }
    
    void println(const String &text) override {
        logger.log(LOG_LEVEL_DEBUG, "DISPLAY", "Print: %s", text.c_str());
    }
    
    void display() override {
        logger.log(LOG_LEVEL_DEBUG, "DISPLAY", "Refresh display");
    }
    
    void setRotation(int rotation) override {
        logger.log(LOG_LEVEL_DEBUG, "DISPLAY", "Set rotation: %d", rotation);
    }
    
    void setBrightness(uint8_t brightness) override {
        logger.log(LOG_LEVEL_DEBUG, "DISPLAY", "Set brightness: %d", brightness);
    }
    
    void powerOn() override {
        logger.log(LOG_LEVEL_INFO, "DISPLAY", "Power on display");
    }
    
    void powerOff() override {
        logger.log(LOG_LEVEL_INFO, "DISPLAY", "Power off display");
    }
    
    bool isInitialized() override {
        return initialized;
    }
};

// 全局显示设备实例
DisplayDevice *display = nullptr;

// 初始化显示设备
void displaySetup() {
    delay(500);
    
    // 尝试加载屏幕亮度设置
    #ifndef NRF52840_PLATFORM
    try {
        STATION_Utils::loadIndex(2);    // Screen Brightness value
    } catch (...) {
        logger.log(LOG_LEVEL_WARN, "DISPLAY", "Failed to load screen brightness");
    }
    #else
    STATION_Utils::loadIndex(2);    // Screen Brightness value - NRF52840不支持异常处理
    #endif
    
    // 为NRF52840平台初始化I2C
    logger.log(LOG_LEVEL_INFO, "DISPLAY", "Initializing I2C for display");
    
    // 对于NRF52840，我们使用不带参数的Wire.begin()
    Wire.begin();
    
    // 创建并初始化空显示设备
    display = new NullDisplay();
    display->init();
    
    // 应用配置设置
    if (Config.display.turn180) {
        display->setRotation(2);
    } else {
        display->setRotation(0);
    }
    
    // 设置显示参数
    display->setTextSize(1);
    display->setTextColor(0xFFFF); // 白色
    display->setCursor(0, 0);
    display->setBrightness(screenBrightness);
    
    // 清除显示并显示初始内容
    display->clear();
    display->println("NRF52840 Display");
    display->println("Initialized");
    display->display();
    
    logger.log(LOG_LEVEL_INFO, "DISPLAY", "Display setup completed");
}

// 切换显示电源状态
void displayToggle(bool toggle) {
    if (display == nullptr) {
        logger.log(LOG_LEVEL_ERROR, "DISPLAY", "Display not initialized");
        return;
    }
    
    if (toggle) {
        display->powerOn();
    } else {
        display->powerOff();
    }
}

// 更新显示亮度
void displayBrightnessAdjust(int adjust) {
    if (display == nullptr) {
        logger.log(LOG_LEVEL_ERROR, "DISPLAY", "Display not initialized");
        return;
    }
    
    // 调整亮度值
    screenBrightness += adjust;
    
    // 确保亮度值在有效范围内
    if (screenBrightness < 1) screenBrightness = 1;
    if (screenBrightness > 255) screenBrightness = 255;
    
    // 应用新的亮度
    display->setBrightness(screenBrightness);
    
    logger.log(LOG_LEVEL_INFO, "DISPLAY", "Brightness adjusted to: %d", screenBrightness);
}

// 显示文本消息
void displayMessage(const String &message, int line) {
    if (display == nullptr || !display->isInitialized()) {
        logger.log(LOG_LEVEL_ERROR, "DISPLAY", "Display not initialized");
        return;
    }
    
    // 简单的消息显示实现
    display->setCursor(0, line * 10); // 假设每行10像素高
    display->println(message);
    display->display();
}

// 清除显示
void clearDisplay() {
    if (display == nullptr) {
        logger.log(LOG_LEVEL_ERROR, "DISPLAY", "Display not initialized");
        return;
    }
    
    display->clear();
    display->display();
}

// 获取屏幕宽度
int getScreenWidth() {
    return SCREEN_WIDTH;
}

// 获取屏幕高度
int getScreenHeight() {
    return SCREEN_HEIGHT;
}

// 检查显示是否初始化
bool isDisplayInitialized() {
    return (display != nullptr && display->isInitialized());
}

// 析构显示设备（在程序结束时调用）
void cleanupDisplay() {
    if (display != nullptr) {
        delete display;
        display = nullptr;
        logger.log(LOG_LEVEL_INFO, "DISPLAY", "Display resources cleaned up");
    }
}