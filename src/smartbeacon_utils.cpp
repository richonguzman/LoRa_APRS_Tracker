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
    struct SmartBeacon {
        bool active;
        int slowRate;
        int fastRate;
        int fastSpeed;
        int minTxDist;
        int minTxTime;
        int turnSlope;
    };
    SmartBeacon smartBeacon;
    int nonSmartBeaconRate;
};

// 全局配置对象外部声明
extern Configuration Config;

// 全局变量外部声明
extern unsigned long lastTxSmartBeacon;
extern bool sendUpdate;

namespace SMARTBEACON_Utils {
    
    // 简单的距离计算函数（返回模拟距离）
    float calculateDistance(float lat1, float lon1, float lat2, float lon2) {
        // 简化的距离计算，实际项目中应该使用更精确的算法
        return sqrt(pow(lat2 - lat1, 2) + pow(lon2 - lon1, 2)) * 111.0; // 近似距离（公里）
    }
    
    // 简单的速度计算函数（返回模拟速度）
    float calculateSpeed(float distance, unsigned long timeDelta) {
        if (timeDelta == 0) return 0;
        return (distance / timeDelta) * 3600.0; // 转换为公里/小时
    }
    
    // 检查是否应该发送更新（基于智能信标规则）
    void checkInterval(int speed) {
        static struct {
            int slowRate;
            int fastRate;
            int fastSpeed;
        } currentSmartBeaconValues;
        
        // 更新当前智能信标值
        currentSmartBeaconValues.slowRate = Config.smartBeacon.slowRate;
        currentSmartBeaconValues.fastRate = Config.smartBeacon.fastRate;
        currentSmartBeaconValues.fastSpeed = Config.smartBeacon.fastSpeed;
        
        // 确保速度不为负数
        if (speed < 0) speed = 0;
        
        // 使用std::min而不是min宏
        unsigned long txInterval;
        
        if (speed < currentSmartBeaconValues.fastSpeed) {
            txInterval = currentSmartBeaconValues.slowRate * 1000; // 转换为毫秒
        } else {
            // 手动计算最小值，避免min宏冲突
            unsigned long calculatedInterval = static_cast<unsigned long>(currentSmartBeaconValues.fastSpeed) * 
                                              static_cast<unsigned long>(currentSmartBeaconValues.fastRate) / 
                                              static_cast<unsigned long>(speed);
            
            if (static_cast<unsigned long>(currentSmartBeaconValues.slowRate) < calculatedInterval) {
                txInterval = currentSmartBeaconValues.slowRate * 1000;
            } else {
                txInterval = calculatedInterval * 1000;
            }
        }
        
        // 确保最小发射时间
        unsigned long minTxInterval = static_cast<unsigned long>(Config.smartBeacon.minTxTime) * 1000;
        if (txInterval < minTxInterval) {
            txInterval = minTxInterval;
        }
        
        // 检查是否应该发送更新
        unsigned long currentTime = millis();
        if (currentTime - lastTxSmartBeacon >= txInterval) {
            sendUpdate = true;
            logger.log(LOG_LEVEL_DEBUG, "SMARTBEACON", "Time based update triggered");
        }
    }
    
    // 检查固定信标时间
    void checkFixedBeaconTime() {
        if (!Config.smartBeacon.active) {
            // 如果智能信标未激活，使用固定时间间隔
            unsigned long currentTime = millis();
            if (currentTime - lastTxSmartBeacon >= static_cast<unsigned long>(Config.nonSmartBeaconRate) * 60 * 1000) {
                sendUpdate = true;
                logger.log(LOG_LEVEL_DEBUG, "SMARTBEACON", "Fixed interval update triggered");
            }
        }
    }
    
    // 检查转弯触发
    void checkTurn(float heading1, float heading2) {
        if (!Config.smartBeacon.active) return;
        
        // 计算航向变化
        float headingDiff = fabs(heading2 - heading1);
        if (headingDiff > 180) {
            headingDiff = 360 - headingDiff;
        }
        
        // 如果航向变化超过阈值，触发更新
        if (headingDiff > Config.smartBeacon.turnSlope) {
            sendUpdate = true;
            logger.log(LOG_LEVEL_DEBUG, "SMARTBEACON", "Turn based update triggered: %.1f degrees", headingDiff);
        }
    }
    
    // 检查距离触发
    void checkDistance(float distance) {
        if (!Config.smartBeacon.active) return;
        
        // 如果距离超过阈值，触发更新
        if (distance >= Config.smartBeacon.minTxDist) {
            sendUpdate = true;
            logger.log(LOG_LEVEL_DEBUG, "SMARTBEACON", "Distance based update triggered: %.1f km", distance);
        }
    }
    
    // 重置上次发射时间
    void resetLastTxTime() {
        lastTxSmartBeacon = millis();
    }
    
    // 获取智能信标状态
    bool isSmartBeaconActive() {
        return Config.smartBeacon.active;
    }
    
    // 初始化智能信标系统
    void init() {
        logger.log(LOG_LEVEL_INFO, "SMARTBEACON", "Initializing smart beacon system");
        resetLastTxTime();
        
        if (Config.smartBeacon.active) {
            logger.log(LOG_LEVEL_INFO, "SMARTBEACON", "Smart beacon enabled");
            logger.log(LOG_LEVEL_INFO, "SMARTBEACON", "Slow rate: %d seconds", Config.smartBeacon.slowRate);
            logger.log(LOG_LEVEL_INFO, "SMARTBEACON", "Fast rate: %d seconds", Config.smartBeacon.fastRate);
            logger.log(LOG_LEVEL_INFO, "SMARTBEACON", "Fast speed: %d km/h", Config.smartBeacon.fastSpeed);
            logger.log(LOG_LEVEL_INFO, "SMARTBEACON", "Min tx distance: %d km", Config.smartBeacon.minTxDist);
            logger.log(LOG_LEVEL_INFO, "SMARTBEACON", "Min tx time: %d seconds", Config.smartBeacon.minTxTime);
            logger.log(LOG_LEVEL_INFO, "SMARTBEACON", "Turn slope: %d degrees", Config.smartBeacon.turnSlope);
        } else {
            logger.log(LOG_LEVEL_INFO, "SMARTBEACON", "Smart beacon disabled");
            logger.log(LOG_LEVEL_INFO, "SMARTBEACON", "Fixed rate: %d minutes", Config.nonSmartBeaconRate);
        }
    }
    
    // 主智能信标处理函数
    void process() {
        // 检查固定信标时间
        checkFixedBeaconTime();
        
        // 注意：其他检查（速度、转弯、距离）应该在有新位置数据时调用
    }
} // namespace SMARTBEACON_Utils