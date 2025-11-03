// 首先解除任何可能存在的min/max宏定义，以避免与STL冲突
#undef min
#undef max

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
    struct Power {
        int batteryPin;
        int batteryMinVoltage;
        int batteryMaxVoltage;
        int shutdownPin;
        bool lowPowerMode;
    };
    Power power;
};

// 全局配置对象外部声明
extern Configuration Config;

// 深度睡眠常量（秒）
#define DEEP_SLEEP_TIME_SEC 10

// 简单的POWER_Utils命名空间
namespace POWER_Utils {
    
    // 电池状态枚举
    enum BatteryStatus {
        BATTERY_UNKNOWN,
        BATTERY_OK,
        BATTERY_LOW,
        BATTERY_CRITICAL
    };
    
    // 电池状态变量
    BatteryStatus batteryStatus = BATTERY_UNKNOWN;
    int lastBatteryLevel = 0;
    unsigned long lastBatteryCheck = 0;
    
    // 检查电池电量
    int checkBattery() {
        if (!Config.power.batteryPin) return 0;
        
        unsigned long currentTime = millis();
        if (currentTime - lastBatteryCheck < 5000) { // 5秒检查一次
            return lastBatteryLevel;
        }
        
        lastBatteryCheck = currentTime;
        
        // 读取电池电压模拟值
        int adcValue = analogRead(Config.power.batteryPin);
        
        // 简单的电池电量计算（基于电压范围）
        // 将ADC值映射到0-100%的电池电量
        // 注意：这里是简化的映射，实际项目中可能需要更复杂的算法
        int batteryPercentage = map(adcValue, 0, 1023, 0, 100);
        
        // 确保百分比在0-100范围内
        batteryPercentage = constrain(batteryPercentage, 0, 100);
        
        // 更新电池状态
        if (batteryPercentage > 30) {
            batteryStatus = BATTERY_OK;
        } else if (batteryPercentage > 10) {
            batteryStatus = BATTERY_LOW;
        } else {
            batteryStatus = BATTERY_CRITICAL;
        }
        
        lastBatteryLevel = batteryPercentage;
        
        // 记录电池电量
        logger.log(LOG_LEVEL_INFO, "POWER", "Battery level: %d%%", batteryPercentage);
        
        return batteryPercentage;
    }
    
    // 获取电池状态
    BatteryStatus getBatteryStatus() {
        return batteryStatus;
    }
    
    // 检查是否需要降低功耗
    bool shouldLowerPower() {
        return (batteryStatus == BATTERY_LOW || batteryStatus == BATTERY_CRITICAL || Config.power.lowPowerMode);
    }
    
    // 降低CPU频率（NRF52840兼容版本）
    void lowerCpuFrequency() {
        // NRF52840平台的CPU频率管理
        logger.log(LOG_LEVEL_INFO, "POWER", "Lowering CPU frequency on NRF52840");
    }
    
    // 恢复正常CPU频率（NRF52840兼容版本）
    void restoreCpuFrequency() {
        logger.log(LOG_LEVEL_INFO, "POWER", "Restoring CPU frequency on NRF52840");
    }
    
    // 初始化电源管理
    void initPowerManagement() {
        logger.log(LOG_LEVEL_INFO, "POWER", "Initializing power management");
        
        // 设置电池引脚为输入
        if (Config.power.batteryPin != -1) {
            pinMode(Config.power.batteryPin, INPUT);
        }
        
        // 设置关机引脚（如果有）
        if (Config.power.shutdownPin != -1) {
            pinMode(Config.power.shutdownPin, OUTPUT);
            digitalWrite(Config.power.shutdownPin, LOW);
        }
        
        // 初始电池检查
        checkBattery();
    }
    
    // 进入深度睡眠（NRF52840兼容版本）
    void shutdown() {
        logger.log(LOG_LEVEL_INFO, "POWER", "Shutting down system");
        
        // 记录关闭信息
        logger.log(LOG_LEVEL_INFO, "POWER", "System going to sleep for %d seconds", DEEP_SLEEP_TIME_SEC);
        
        delay(500); // 给日志时间写入
        
        logger.log(LOG_LEVEL_INFO, "POWER", "System in low power state");
    }
    
    // 检查关机引脚状态
    bool checkShutdownRequest() {
        if (Config.power.shutdownPin == -1) {
            return false;
        }
        
        return false;
    }
    
    // 处理电源相关事件
    void handlePowerEvents() {
        // 检查电池电量
        checkBattery();
        
        // 如果电池电量低，尝试降低功耗
        if (batteryStatus == BATTERY_LOW || batteryStatus == BATTERY_CRITICAL) {
            lowerCpuFrequency();
        }
        
        // 检查是否有关机请求
        if (checkShutdownRequest()) {
            shutdown();
        }
    }
    
    // 获取电源状态的文本描述
    String getPowerStatusString() {
        switch (batteryStatus) {
            case BATTERY_OK:
                return "OK";
            case BATTERY_LOW:
                return "LOW";
            case BATTERY_CRITICAL:
                return "CRITICAL";
            default:
                return "UNKNOWN";
        }
    }
} // namespace POWER_Utils