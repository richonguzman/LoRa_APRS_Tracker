#ifdef USE_LVGL_UI

#include <esp_log.h>
#include "gpx_writer.h"
#include <SD.h>
#include <NMEAGPS.h>
#include <freertos/semphr.h>

static const char *TAG = "GPX";

extern gps_fix gpsFix;
extern SemaphoreHandle_t spiMutex;

namespace GPXWriter {

    static bool recording = false;
    static char currentFilePath[64] = "";

    bool isRecording() {
        return recording;
    }

    bool startRecording() {
        if (recording) return true;

        // Build filename from GPS date/time (UTC)
        char filename[64];
        if (gpsFix.valid.date && gpsFix.valid.time) {
            snprintf(filename, sizeof(filename),
                     "/LoRa_Tracker/gpx/track_%04d-%02d-%02d_%02d%02d.gpx",
                     2000 + gpsFix.dateTime.year, gpsFix.dateTime.month, gpsFix.dateTime.date,
                     gpsFix.dateTime.hours, gpsFix.dateTime.minutes);
        } else {
            snprintf(filename, sizeof(filename),
                     "/LoRa_Tracker/gpx/track_%lu.gpx", millis() / 1000);
        }

        if (spiMutex == NULL || xSemaphoreTakeRecursive(spiMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to acquire SPI mutex");
            return false;
        }

        // Ensure directory exists
        if (!SD.exists("/LoRa_Tracker/gpx")) {
            SD.mkdir("/LoRa_Tracker/gpx");
        }

        File file = SD.open(filename, FILE_WRITE);
        if (!file) {
            xSemaphoreGiveRecursive(spiMutex);
            ESP_LOGE(TAG, "Failed to create file: %s", filename);
            return false;
        }

        // Write GPX header
        file.println("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
        file.println("<gpx version=\"1.1\" creator=\"LoRa_APRS_Tracker\"");
        file.println("  xmlns=\"http://www.topografix.com/GPX/1/1\">");
        file.println("  <trk>");
        file.println("    <name>LoRa APRS Track</name>");
        file.println("    <trkseg>");
        file.close();
        xSemaphoreGiveRecursive(spiMutex);

        strncpy(currentFilePath, filename, sizeof(currentFilePath));
        recording = true;
        ESP_LOGI(TAG, "Recording started: %s", filename);
        return true;
    }

    void stopRecording() {
        if (!recording) return;

        if (spiMutex != NULL && xSemaphoreTakeRecursive(spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            File file = SD.open(currentFilePath, FILE_APPEND);
            if (file) {
                file.println("    </trkseg>");
                file.println("  </trk>");
                file.println("</gpx>");
                file.close();
            }
            xSemaphoreGiveRecursive(spiMutex);
        }

        ESP_LOGI(TAG, "Recording stopped: %s", currentFilePath);
        recording = false;
        currentFilePath[0] = '\0';
    }

    void addPoint(float lat, float lon, float alt, float hdop, float speed) {
        if (!recording) return;

        // Build timestamp from GPS
        char timestamp[32] = "";
        if (gpsFix.valid.date && gpsFix.valid.time) {
            snprintf(timestamp, sizeof(timestamp),
                     "%04d-%02d-%02dT%02d:%02d:%02dZ",
                     2000 + gpsFix.dateTime.year, gpsFix.dateTime.month, gpsFix.dateTime.date,
                     gpsFix.dateTime.hours, gpsFix.dateTime.minutes, gpsFix.dateTime.seconds);
        }

        if (spiMutex == NULL || xSemaphoreTakeRecursive(spiMutex, pdMS_TO_TICKS(500)) != pdTRUE) {
            return;
        }

        File file = SD.open(currentFilePath, FILE_APPEND);
        if (file) {
            file.printf("      <trkpt lat=\"%.6f\" lon=\"%.6f\">\n", lat, lon);
            file.printf("        <ele>%.1f</ele>\n", alt);
            if (timestamp[0])
                file.printf("        <time>%s</time>\n", timestamp);
            file.printf("        <hdop>%.1f</hdop>\n", hdop);
            file.printf("        <speed>%.1f</speed>\n", speed);
            file.println("      </trkpt>");
            file.close();
        }
        xSemaphoreGiveRecursive(spiMutex);
    }

} // namespace GPXWriter

#endif // USE_LVGL_UI
