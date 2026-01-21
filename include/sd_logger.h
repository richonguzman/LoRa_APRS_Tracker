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
}

#endif // SD_LOGGER_H_
