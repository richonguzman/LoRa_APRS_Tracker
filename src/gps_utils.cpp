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

// 简单的TinyGPS++类前向声明，仅包含编译所需的最小接口
class TinyGPSPlus {
public:
    class Location {
    public:
        double lat() const { return 0.0; }
        double lng() const { return 0.0; }
        bool isUpdated() const { return false; }
        bool isValid() const { return false; }
    };
    
    class Date {
    public:
        int year() const { return 2024; }
        int month() const { return 1; }
        int day() const { return 1; }
    };
    
    class Time {
    public:
        int hour() const { return 0; }
        int minute() const { return 0; }
        int second() const { return 0; }
    };
    
    class Course {
    public:
        double deg() const { return 0.0; }
    };
    
    class Speed {
    public:
        double knots() const { return 0.0; }
    };
    
    class Altitude {
    public:
        double feet() const { return 0.0; }
    };
    
    class Satellites {
    public:
        uint32_t value() const { return 0; }
    };
    
    class HDOP {
    public:
        double value() const { return 0.0; }
    };
    
    Location location;
    Date date;
    Time time;
    Course course;
    Speed speed;
    Altitude altitude;
    Satellites satellites;
    HDOP hdop;
    
    void encode(char c) {}
};

// 简单的Configuration类前向声明
class Configuration {
public:
    struct GPS {
        int rxPin;
        int txPin;
        int baudRate;
        int powerPin;
        int updateRate;
        int timeoutMs;
        bool active;
    };
    GPS gps;
};

// 全局对象外部声明
extern TinyGPSPlus gps;
extern Configuration Config;

// 简单的GPS_Utils命名空间
namespace GPS_Utils {
    
    // GPS状态枚举
    enum GPSStatus {
        GPS_NOT_INITIALIZED,
        GPS_INITIALIZING,
        GPS_READY,
        GPS_NO_FIX,
        GPS_FIX
    };
    
    // GPS状态变量
    GPSStatus gpsStatus = GPS_NOT_INITIALIZED;
    unsigned long lastFixTime = 0;
    unsigned long lastUpdateTime = 0;
    bool gpsInitialized = false;
    
    // 初始化GPS
    void initGPS() {
        Serial.println("Initializing GPS");
        
        // 设置GPS电源引脚（如果有）
        if (Config.gps.powerPin != -1) {
            pinMode(Config.gps.powerPin, OUTPUT);
            digitalWrite(Config.gps.powerPin, HIGH);
            delay(100);
        }
        
        // 初始化串口通信
        #ifdef NRF52840_PLATFORM
            // NRF52840平台上的串口处理
            Serial.begin(Config.gps.baudRate);
        #else
            Serial1.begin(Config.gps.baudRate, SERIAL_8N1, Config.gps.rxPin, Config.gps.txPin);
        #endif
        
        // 更新GPS状态
        gpsStatus = GPS_INITIALIZING;
        gpsInitialized = true;
        
        // 记录初始化日志
        logger.log(LOG_LEVEL_INFO, "GPS", "GPS initialized");
    }
    
    // 关闭GPS
    void closeGPS() {
        Serial.println("Closing GPS");
        
        // 关闭串口通信
        #ifdef NRF52840_PLATFORM
            // NRF52840平台上的串口处理
            Serial.end();
        #else
            Serial1.end();
        #endif
        
        // 关闭GPS电源（如果有）
        if (Config.gps.powerPin != -1) {
            digitalWrite(Config.gps.powerPin, LOW);
        }
        
        // 更新GPS状态
        gpsStatus = GPS_NOT_INITIALIZED;
        gpsInitialized = false;
        
        // 记录关闭日志
        logger.log(LOG_LEVEL_INFO, "GPS", "GPS closed");
    }
    
    // 更新GPS数据
    void updateGPS() {
        if (!gpsInitialized || gpsStatus == GPS_NOT_INITIALIZED) {
            return;
        }
        
        // 检查是否需要更新
        unsigned long currentTime = millis();
        if (currentTime - lastUpdateTime < Config.gps.updateRate) {
            return;
        }
        
        lastUpdateTime = currentTime;
        
        // 从串口读取GPS数据
        #ifdef NRF52840_PLATFORM
            // NRF52840平台上的串口处理
            while (Serial.available() > 0) {
                char c = Serial.read();
                gps.encode(c);
            }
        #else
            while (Serial1.available() > 0) {
                char c = Serial1.read();
                gps.encode(c);
            }
        #endif
        
        // 检查GPS定位状态
        if (gps.location.isValid()) {
            lastFixTime = currentTime;
            gpsStatus = GPS_FIX;
            logger.log(LOG_LEVEL_INFO, "GPS", "GPS fix acquired");
        } else {
            // 如果长时间没有定位，标记为无信号
            if (currentTime - lastFixTime > Config.gps.timeoutMs) {
                gpsStatus = GPS_NO_FIX;
            }
        }
    }
    
    // 获取GPS状态
    GPSStatus getGPSStatus() {
        return gpsStatus;
    }
    
    // 检查GPS是否有定位
    bool hasFix() {
        return gpsStatus == GPS_FIX;
    }
    
    // 获取自上次定位以来的时间
    unsigned long getTimeSinceFix() {
        if (!hasFix()) {
            return millis() - lastFixTime;
        }
        return 0;
    }
    
    // 检查GPS是否初始化
    bool isInitialized() {
        return gpsInitialized;
    }
    
    // 唤醒GPS
    void wakeUpGPS() {
        if (!gpsInitialized || gpsStatus == GPS_NOT_INITIALIZED) {
            initGPS();
        } else {
            logger.log(LOG_LEVEL_INFO, "GPS", "GPS already active");
        }
    }
    
    // 休眠GPS
    void sleepGPS() {
        closeGPS();
    }
} // namespace GPS_Utils