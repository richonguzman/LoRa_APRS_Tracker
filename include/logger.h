#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <stdarg.h>
#include "platform_compat.h"

// 简化的日志级别定义
#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_ERROR 3
#define LOG_LEVEL_WARN LOG_LEVEL_WARNING // 保持向后兼容

class Logger {
private:
    int currentLevel;

public:
    Logger() {
        currentLevel = LOG_LEVEL_INFO;
    }

    void setLogLevel(int level) {
        currentLevel = level;
    }

    void log(int level, const char* module, const char* format, ...) {
        if (level < currentLevel) return;

        switch (level) {
            case LOG_LEVEL_DEBUG:
                Serial.print("[DEBUG] ");
                break;
            case LOG_LEVEL_INFO:
                Serial.print("[INFO] ");
                break;
            case LOG_LEVEL_WARNING:
                Serial.print("[WARN] ");
                break;
            case LOG_LEVEL_ERROR:
                Serial.print("[ERROR] ");
                break;
        }

        Serial.print(module);
        Serial.print(": ");

        va_list args;
        va_start(args, format);
        char buffer[256];
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        Serial.println(buffer);
    }
};

extern Logger logger;

#endif // LOGGER_H