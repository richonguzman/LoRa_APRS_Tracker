/* SD Logger - Persistent logging to SD card for debugging reboots
 * Logs system events, errors, and reboot causes when USB is not connected
 */

#ifndef SD_LOGGER_H_
#define SD_LOGGER_H_

#include <Arduino.h>

namespace SD_Logger {

    // Initialize SD logger (call after SD card is mounted)
    void init();

    // Log levels
    enum LogLevel {
        INFO,
        WARN,
        ERROR,
        CRITICAL
    };

    // Set GPS wall-clock time (call once when GPS has a valid fix)
    // Subsequent log entries will use HH:MM:SS UTC instead of raw uptime
    void setGpsTime(uint8_t hour, uint8_t minute, uint8_t second,
                    uint8_t day, uint8_t month, uint16_t year);

    // Log a message to SD card
    void log(LogLevel level, const char* module, const char* message);
    void logf(LogLevel level, const char* module, const char* format, ...);

    // Log boot information (reason, free memory, etc)
    void logBootInfo();

    // Log watchdog reset event
    void logWatchdogReset();

    // Log screen state changes
    void logScreenState(bool dimmed);

    // Rotate log file if too large (keeps last N entries)
    void rotateLogs();

    // Get log file path
    String getLogFilePath();

    // Clear old logs
    void clearLogs();

    // Crash context — stored in RTC memory, survives panic/WDT reset
    // Call updateCrashContext() periodically from key code paths.
    // Call logPreviousCrashContext() at boot (before logBootInfo writes BOOT END).
    void updateCrashContext(const char* module, float lat = 0.0f, float lon = 0.0f);
    void logPreviousCrashContext();
}

#endif // SD_LOGGER_H_
