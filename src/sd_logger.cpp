/* SD Logger - Persistent logging to SD card for debugging reboots */

#include "sd_logger.h"
#include "storage_utils.h"
#include <SD.h>
#include <esp_log.h>
#include <esp_system.h>
#include <rom/rtc.h>
#include <sys/time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

static const char *TAG = "SD_Log";

#define SD_LOG_FILE "/LoRa_Tracker/system.log"
#define SD_LOG_MAX_SIZE 102400  // 100KB
#define SD_LOG_MAX_LINES 1000

namespace SD_Logger {

    static bool initialized = false;
    static SemaphoreHandle_t sdLogMutex = nullptr;

    // GPS wall-clock time state
    static bool     gpsTimeSet       = false;
    static uint8_t  gpsMonth         = 0;
    static uint8_t  gpsDay           = 0;
    static uint32_t gpsSecondsOfDay  = 0;   // seconds since midnight UTC at fix
    static uint32_t millisAtGpsFix   = 0;   // millis() at the time of the fix

    // vprintf hook state
    static vprintf_like_t originalVprintf = nullptr;
    static volatile bool  inSdHook        = false;

    // Format timestamp: [MM-DD HH:MM:SS] with GPS, [+HHH:MM:SS.mmm] without
    static void formatTimestamp(char* buf, size_t size) {
        uint32_t now = millis();
        if (gpsTimeSet) {
            uint32_t elapsed   = (now - millisAtGpsFix) / 1000;
            uint32_t totalSec  = gpsSecondsOfDay + elapsed;
            uint8_t  h = (totalSec / 3600) % 24;
            uint8_t  m = (totalSec / 60) % 60;
            uint8_t  s = totalSec % 60;
            snprintf(buf, size, "[%02u-%02u %02u:%02u:%02u]", gpsMonth, gpsDay, h, m, s);
        } else {
            uint32_t sec = now / 1000;
            uint32_t ms  = now % 1000;
            uint32_t hh  = sec / 3600;
            uint8_t  mm  = (sec / 60) % 60;
            uint8_t  ss  = sec % 60;
            snprintf(buf, size, "[+%03u:%02u:%02u.%03u]", hh, mm, ss, ms);
        }
    }

    // Strip ANSI escape codes in-place (e.g. "\033[0;32m" -> "")
    static void stripAnsi(char* str) {
        char *src = str, *dst = str;
        while (*src) {
            if (*src == '\033' && *(src + 1) == '[') {
                src += 2;
                while (*src && *src != 'm') src++;
                if (*src == 'm') src++;
            } else {
                *dst++ = *src++;
            }
        }
        *dst = '\0';
    }

    // vprintf hook: captures ESP_LOGW / ESP_LOGE to SD
    static int sdLogVprintf(const char* format, va_list args) {
        va_list args_copy;
        va_copy(args_copy, args);
        int ret = originalVprintf(format, args);  // serial output (consumes args)

        if (initialized && !inSdHook && !xPortInIsrContext()) {
            char buf[300];
            vsnprintf(buf, sizeof(buf), format, args_copy);
            stripAnsi(buf);
            // Only persist W (warn) and E (error) levels — I/D/V are too noisy
            if (buf[0] == 'W' || buf[0] == 'E') {
                inSdHook = true;
                if (sdLogMutex && xSemaphoreTake(sdLogMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                    File f = SD.open(SD_LOG_FILE, FILE_APPEND);
                    if (f) {
                        char ts[20];
                        formatTimestamp(ts, sizeof(ts));
                        f.print(ts);
                        f.print(' ');
                        f.print(buf);
                        f.close();
                    }
                    xSemaphoreGive(sdLogMutex);
                }
                inSdHook = false;
            }
        }
        va_end(args_copy);
        return ret;
    }

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

    void setGpsTime(uint8_t hour, uint8_t minute, uint8_t second,
                    uint8_t day, uint8_t month, uint16_t year) {
        gpsSecondsOfDay = (uint32_t)hour * 3600 + minute * 60 + second;
        gpsDay          = day;
        gpsMonth        = month;
        millisAtGpsFix  = millis();
        gpsTimeSet      = true;
    }

    void init() {
        // Install vprintf hook early — it guards itself with `initialized`
        if (!originalVprintf) {
            originalVprintf = esp_log_set_vprintf(sdLogVprintf);
        }

        if (!STORAGE_Utils::isSDAvailable()) {
            ESP_LOGW(TAG, "SD not available, logging disabled");
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
        ESP_LOGI(TAG, "Initialized");
    }

    void log(LogLevel level, const char* module, const char* message) {
        if (!initialized || !STORAGE_Utils::isSDAvailable()) return;

        // Take mutex
        if (sdLogMutex && xSemaphoreTake(sdLogMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGW(TAG, "Failed to get mutex");
            return;
        }

        File logFile = SD.open(SD_LOG_FILE, FILE_APPEND);
        if (!logFile) {
            ESP_LOGE(TAG, "Failed to open log file");
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

        char ts[20];
        formatTimestamp(ts, sizeof(ts));
        char logLine[256];
        snprintf(logLine, sizeof(logLine), "%s %s %s: %s\n",
                 ts, levelStr, module, message);

        logFile.print(logLine);
        size_t currentSize = logFile.size();
        logFile.close();

        if (sdLogMutex) xSemaphoreGive(sdLogMutex);

        if (currentSize > SD_LOG_MAX_SIZE) {
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

        ESP_LOGI(TAG, "Rotating log file");

        if (sdLogMutex && xSemaphoreTake(sdLogMutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
            return;
        }

        if (SD.exists(SD_LOG_FILE)) {
            SD.remove("/LoRa_Tracker/system.log.old");
            SD.rename(SD_LOG_FILE, "/LoRa_Tracker/system.log.old");
        }

        if (sdLogMutex) xSemaphoreGive(sdLogMutex);

        ESP_LOGI(TAG, "Log rotation complete");
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
