// 平台兼容性头文件 - 用于处理不同平台之间的差异
// 特别针对ESP32和NRF52840平台

#ifndef PLATFORM_COMPAT_H
#define PLATFORM_COMPAT_H

#include <Arduino.h>

// 检测平台
#ifdef ARDUINO_ARCH_ESP32
#define PLATFORM_ESP32
#elif defined(ARDUINO_ARCH_NRF52)
#define PLATFORM_NRF52840
#define NRF52840_PLATFORM  // 保持向后兼容性
#else
#define PLATFORM_UNKNOWN
#endif

// 处理min/max宏冲突
#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

// 引入必要的头文件
#include <algorithm> // 为了使用std::min和std::max

// 串口定义 - NRF52840上可能没有Serial1
#ifdef PLATFORM_NRF52840
// 在NRF52840上，我们可以使用Serial作为主串口，或者定义一个包装器
#define Serial1 Serial
#endif

// SPI定义
#ifdef PLATFORM_NRF52840
// 确保SPI在NRF52840上可用
#include <SPI.h>
#endif

// 日志级别常量已在logger.h中定义

// ESP32特有功能的NRF52840替代实现
#ifdef PLATFORM_NRF52840

// 替代ESP32的电源管理函数
inline void setCpuFrequencyMhz(int mhz) {
  // NRF52840的CPU频率管理
  // 这里只是记录日志，实际实现需要根据ArduinoCore-nRF52库的API
}

// 替代ESP32的深度睡眠函数
inline void esp_sleep_enable_timer_wakeup(unsigned long time_us) {
  // NRF52840的睡眠模式替代
}

inline void esp_deep_sleep_start() {
  // NRF52840的深度睡眠替代
}

// 替代ESP32的ledc函数
inline void ledcSetup(int channel, int freq, int resolution_bits) {
  // NRF52840上使用简单的PWM实现
}

inline void ledcAttachPin(int pin, int channel) {
  // 配置引脚为PWM输出
  pinMode(pin, OUTPUT);
}

inline void ledcWrite(int channel, int value) {
  // 在NRF52840上，我们可以使用analogWrite来替代
}

// ESP命名空间的兼容层
namespace ESP {
  inline void restart() {
    // NRF52840的重启实现
    Serial.println("[Platform] Restarting...");
    NVIC_SystemReset();
  }
}

// 额外的重启相关函数，提供更广泛的兼容性
inline void ESP_restart() {
  // 调用ESP命名空间中的restart方法
  ESP::restart();
}

#endif // PLATFORM_NRF52840

// String类扩展 - 为不支持isEmpty()的平台提供兼容
#if !defined(PLATFORM_ESP32)
inline bool isEmpty(const String& str) {
  return str.length() == 0;
}
#endif

#endif // PLATFORM_COMPAT_H