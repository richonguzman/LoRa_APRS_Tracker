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
    struct Notification {
        bool buzzerActive;
        int buzzerPinTone;
        int buzzerPinVcc;
        bool bootUpBeep;
        bool txBeep;
        bool messageRxBeep;
        bool stationBeep;
        bool lowBatteryBeep;
        bool shutDownBeep;
        bool ledTx;
        int ledTxPin;
    };
    Notification notification;
};

// 全局配置对象外部声明
extern Configuration Config;

// 简单的NOTIFICATION_Utils命名空间，只包含最基本的功能
namespace NOTIFICATION_Utils {
    
    // 为NRF52840平台提供一个简单的tone函数实现
    void tone(int pin, unsigned int frequency, unsigned long duration = 0) {
        #ifdef NRF52840_PLATFORM
            // NRF52840平台使用简单的脉冲宽度调制模拟tone函数
            unsigned long startTime = millis();
            unsigned int period = 1000000 / frequency / 2; // 微秒
            
            if (duration == 0) {
                // 无限播放，这里我们只播放一小段时间
                for (unsigned int i = 0; i < 100; i++) {
                    digitalWrite(pin, HIGH);
                    delayMicroseconds(period);
                    digitalWrite(pin, LOW);
                    delayMicroseconds(period);
                }
            } else {
                // 播放指定时长
                while (millis() - startTime < duration) {
                    digitalWrite(pin, HIGH);
                    delayMicroseconds(period);
                    digitalWrite(pin, LOW);
                    delayMicroseconds(period);
                }
            }
        #else
            // 其他平台可以使用标准的tone函数
            ::tone(pin, frequency, duration);
        #endif
    }
    
    // 为NRF52840平台提供一个简单的noTone函数实现
    void noTone(int pin) {
        #ifdef NRF52840_PLATFORM
            // 简单地将引脚设置为低电平
            digitalWrite(pin, LOW);
        #else
            // 其他平台可以使用标准的noTone函数
            ::noTone(pin);
        #endif
    }
    
    // 简单的playTone函数，不使用ledc相关函数
    void playTone(int frequency, uint8_t duration) {
        Serial.print("Playing tone: ");
        Serial.print(frequency);
        Serial.print(" Hz for ");
        Serial.print(duration);
        Serial.println(" ms");
        
        if (Config.notification.buzzerActive) {
            // 设置蜂鸣器VCC引脚（如果有）
            if (Config.notification.buzzerPinVcc != -1) {
                digitalWrite(Config.notification.buzzerPinVcc, HIGH);
            }
            
            // 使用我们的tone函数
            tone(Config.notification.buzzerPinTone, frequency, duration);
            
            // 短暂延迟
            delay(duration);
            
            // 停止发声
            noTone(Config.notification.buzzerPinTone);
            
            // 关闭蜂鸣器VCC引脚（如果有）
            if (Config.notification.buzzerPinVcc != -1) {
                digitalWrite(Config.notification.buzzerPinVcc, LOW);
            }
        }
    }
    
    // 简单的beaconTxBeep函数
    void beaconTxBeep() {
        Serial.println("Beacon TX beep");
        if (Config.notification.buzzerActive && Config.notification.txBeep) {
            playTone(1000, 100);
        }
    }
    
    // 简单的startUpBeep函数
    void startUpBeep() {
        Serial.println("Start-up beep");
        if (Config.notification.buzzerActive && Config.notification.bootUpBeep) {
            playTone(1000, 100);
            delay(50);
            playTone(1500, 100);
        }
    }
    
    // 简单的shutDownBeep函数
    void shutDownBeep() {
        Serial.println("Shut-down beep");
        if (Config.notification.buzzerActive && Config.notification.shutDownBeep) {
            playTone(1500, 100);
            delay(50);
            playTone(1000, 100);
        }
    }
    
    // 简单的stationBeep函数
    void stationBeep() {
        Serial.println("Station beep");
        if (Config.notification.buzzerActive && Config.notification.stationBeep) {
            playTone(800, 100);
        }
    }
    
    // 简单的start函数
    void start() {
        Serial.println("Initializing notifications");
        
        // 初始化LED引脚
        if (Config.notification.ledTx) {
            pinMode(Config.notification.ledTxPin, OUTPUT);
            digitalWrite(Config.notification.ledTxPin, LOW);
        }
        
        // 初始化蜂鸣器引脚
        if (Config.notification.buzzerActive) {
            pinMode(Config.notification.buzzerPinTone, OUTPUT);
            digitalWrite(Config.notification.buzzerPinTone, LOW);
            
            if (Config.notification.buzzerPinVcc != -1) {
                pinMode(Config.notification.buzzerPinVcc, OUTPUT);
                digitalWrite(Config.notification.buzzerPinVcc, LOW);
            }
        }
        
        // 播放启动提示音
        startUpBeep();
    }
    
    // 简单的stop函数
    void stop() {
        Serial.println("Stopping notifications");
        
        // 播放关闭提示音
        shutDownBeep();
        
        // 关闭LED
        if (Config.notification.ledTx) {
            digitalWrite(Config.notification.ledTxPin, LOW);
        }
        
        // 关闭蜂鸣器
        if (Config.notification.buzzerActive) {
            digitalWrite(Config.notification.buzzerPinTone, LOW);
            
            if (Config.notification.buzzerPinVcc != -1) {
                digitalWrite(Config.notification.buzzerPinVcc, LOW);
            }
        }
    }
} // namespace NOTIFICATION_Utils