/* SD Logger - Persistent logging to SD card for debugging reboots */

#include "sd_logger.h"
#include "storage_utils.h"
#include <SD.h>
#include <esp_system.h>
#include <rom/rtc.h>
#include <sys/time.h>

#define SD_LOG_FILE "/LoRa_Tracker/system.log"
#define SD_LOG_MAX_SIZE 102400  // 100KB
#define SD_LOG_MAX_LINES 1000

namespace SD_Logger {

    static bool initialized = false;
    static SemaphoreHandle_t sdLogMutex = nullptr;

    // Get reset reason as string
    static const char* getResetReasonString(esp_reset_reason_t reason) {
        switch (reason) {
            case ESP_RST_UNKNOWN:    return "UNKNOWN";
            case ESP_RST_POWERON:    return "POWER_ON";
            case ESP_RST_EXT:        return "EXTERNAL";
            case ESP_RST_SW:         return "SOFTWARE";
            case ESP_RST_PANIC:      return "PANIC";
            case ESP_RST_INT_WDT:    return "INT_WATCHDOG";
            case ESP_RST_TASK_WDT:   return "TASK_WATCHDOG";
            case ESP_RST_WDT:        return "WDT";
            case ESP_RST_DEEPSLEEP:  return "DEEP_SLEEP";
            case ESP_RST_BROWNOUT:   return "BROWNOUT";
            case ESP_RST_SDIO:       return "SDIO";
            default:                 return "UNKNOWN";
        }
    }

    // Get wakeup reason as string
    static const char* getWakeupReasonString(esp_sleep_wakeup_cause_t reason) {
        switch (reason) {
            case ESP_SLEEP_WAKEUP_UNDEFINED:    return "UNDEFINED";
            case ESP_SLEEP_WAKEUP_EXT0:         return "EXT0";
            case ESP_SLEEP_WAKEUP_EXT1:         return "EXT1";
            case ESP_SLEEP_WAKEUP_TIMER:        return "TIMER";
            case ESP_SLEEP_WAKEUP_TOUCHPAD:     return "TOUCHPAD";
            case ESP_SLEEP_WAKEUP_ULP:          return "ULP";
            case ESP_SLEEP_WAKEUP_GPIO:         return "GPIO";
            case ESP_SLEEP_WAKEUP_UART:         return "UART";
            default:                            return "OTHER";
        }
    }

    void init() {
        if (!STORAGE_Utils::isSDAvailable()) {
            Serial.println("[SD_LOG] SD not available, logging disabled");
            return;
        }

        // Create mutex for thread-safe SD access
        if (!sdLogMutex) {
            sdLogMutex = xSemaphoreCreateMutex();
        }

        // Create log directory if needed
        if (!SD.exists("/LoRa_Tracker")) {
            SD.mkdir("/LoRa_Tracker");
        }

        initialized = true;
        Serial.println("[SD_LOG] Initialized");
    }

    void log(LogLevel level, const char* module, const char* message) {
        if (!initialized || !STORAGE_Utils::isSDAvailable()) return;

        // Take mutex
        if (sdLogMutex && xSemaphoreTake(sdLogMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            Serial.println("[SD_LOG] Failed to get mutex");
            return;
        }

        File logFile = SD.open(SD_LOG_FILE, FILE_APPEND);
        if (!logFile) {
            Serial.println("[SD_LOG] Failed to open log file");
            if (sdLogMutex) xSemaphoreGive(sdLogMutex);
            return;
        }

        // Format: [uptime_ms] LEVEL MODULE: message
        const char* levelStr;
        switch (level) {
            case INFO:     levelStr = "INFO "; break;
            case WARN:     levelStr = "WARN "; break;
            case ERROR:    levelStr = "ERROR"; break;
            case CRITICAL: levelStr = "CRIT "; break;
            default:       levelStr = "???? "; break;
        }

        char logLine[256];
        snprintf(logLine, sizeof(logLine), "[%010lu] %s %s: %s\n",
                 millis(), levelStr, module, message);

        logFile.print(logLine);
        logFile.close();

        // Also print to Serial
        Serial.print("[SD_LOG] ");
        Serial.print(logLine);

        if (sdLogMutex) xSemaphoreGive(sdLogMutex);

        // Check if rotation needed
        if (logFile.size() > SD_LOG_MAX_SIZE) {
            rotateLogs();
        }
    }

    void logf(LogLevel level, const char* module, const char* format, ...) {
        char buffer[200];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        log(level, module, buffer);
    }

    void logBootInfo() {
        if (!initialized) return;

        log(INFO, "BOOT", "==================== BOOT START ====================");

        // Reset reason
        esp_reset_reason_t resetReason = esp_reset_reason();
        logf(INFO, "BOOT", "Reset reason: %s", getResetReasonString(resetReason));

        // Wakeup cause (if from sleep)
        if (resetReason == ESP_RST_DEEPSLEEP) {
            esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();
            logf(INFO, "BOOT", "Wakeup cause: %s", getWakeupReasonString(wakeupReason));
        }

        // Memory info
        logf(INFO, "BOOT", "Free heap: %u KB", ESP.getFreeHeap() / 1024);
        #ifdef BOARD_HAS_PSRAM
            logf(INFO, "BOOT", "Free PSRAM: %u KB", ESP.getFreePsram() / 1024);
        #endif

        // CPU frequency
        logf(INFO, "BOOT", "CPU freq: %u MHz", getCpuFrequencyMhz());

        log(INFO, "BOOT", "====================================================");
    }

    void logWatchdogReset() {
        log(CRITICAL, "WDT", "Watchdog reset in loop() - possible freeze detected");
    }

    void logScreenState(bool dimmed) {
        if (dimmed) {
            log(INFO, "SCREEN", "Screen dimmed (eco mode)");
        } else {
            log(INFO, "SCREEN", "Screen active");
        }
    }

    void rotateLogs() {
        if (!initialized || !STORAGE_Utils::isSDAvailable()) return;

        log(INFO, "SD_LOG", "Rotating log file");

        // Take mutex
        if (sdLogMutex && xSemaphoreTake(sdLogMutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
            return;
        }

        // Backup old log
        if (SD.exists(SD_LOG_FILE)) {
            SD.remove("/LoRa_Tracker/system.log.old");
            SD.rename(SD_LOG_FILE, "/LoRa_Tracker/system.log.old");
        }

        if (sdLogMutex) xSemaphoreGive(sdLogMutex);

        log(INFO, "SD_LOG", "Log rotation complete");
    }

    String getLogFilePath() {
        return String(SD_LOG_FILE);
    }

    void clearLogs() {
        if (!initialized || !STORAGE_Utils::isSDAvailable()) return;

        if (sdLogMutex && xSemaphoreTake(sdLogMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            SD.remove(SD_LOG_FILE);
            SD.remove("/LoRa_Tracker/system.log.old");
            xSemaphoreGive(sdLogMutex);
            log(INFO, "SD_LOG", "Logs cleared");
        }
    }
}
