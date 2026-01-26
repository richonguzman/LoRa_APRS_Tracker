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

#include <SPIFFS.h>
#include <SD.h>
#include <SPI.h>
#include <vector>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include "board_pinout.h"
#include "storage_utils.h"

static bool sdAvailable = false;

// In-memory contacts cache
static std::vector<Contact> contactsCache;
static bool contactsLoaded = false;

// Root directory for all tracker data on SD card
static const char* ROOT_DIR = "/LoRa_Tracker";
static const char* MESSAGES_DIR = "/LoRa_Tracker/Messages";
static const char* INBOX_DIR = "/LoRa_Tracker/Messages/inbox";
static const char* OUTBOX_DIR = "/LoRa_Tracker/Messages/outbox";
static const char* CONTACTS_DIR = "/LoRa_Tracker/Contacts";
static const char* CONTACTS_FILE = "/LoRa_Tracker/Contacts/contacts.json";
static const char* MAPS_DIR = "/LoRa_Tracker/Maps";
static const char* SYMBOLS_DIR = "/LoRa_Tracker/Symbols";

namespace STORAGE_Utils {

    void createDirectoryStructure() {
        if (!sdAvailable) return;

        // Create root directory
        if (!SD.exists(ROOT_DIR)) {
            SD.mkdir(ROOT_DIR);
            Serial.printf("[Storage] Created %s\n", ROOT_DIR);
        }

        // Create Messages directories
        if (!SD.exists(MESSAGES_DIR)) {
            SD.mkdir(MESSAGES_DIR);
            Serial.printf("[Storage] Created %s\n", MESSAGES_DIR);
        }
        if (!SD.exists(INBOX_DIR)) {
            SD.mkdir(INBOX_DIR);
            Serial.printf("[Storage] Created %s\n", INBOX_DIR);
        }
        if (!SD.exists(OUTBOX_DIR)) {
            SD.mkdir(OUTBOX_DIR);
            Serial.printf("[Storage] Created %s\n", OUTBOX_DIR);
        }

        // Create Contacts directory
        if (!SD.exists(CONTACTS_DIR)) {
            SD.mkdir(CONTACTS_DIR);
            Serial.printf("[Storage] Created %s\n", CONTACTS_DIR);
        }

        // Create Maps directory for offline tiles
        if (!SD.exists(MAPS_DIR)) {
            SD.mkdir(MAPS_DIR);
            Serial.printf("[Storage] Created %s\n", MAPS_DIR);
        }

        // Create Symbols directory for APRS symbols
        if (!SD.exists(SYMBOLS_DIR)) {
            SD.mkdir(SYMBOLS_DIR);
            Serial.printf("[Storage] Created %s\n", SYMBOLS_DIR);
        }
    }

    void setup() {
        // Always init SPIFFS as fallback (format on fail for first boot)
        if (!SPIFFS.begin(true)) {
            Serial.println("[Storage] SPIFFS mount failed");
        } else {
            Serial.println("[Storage] SPIFFS mounted");
        }

        #ifdef BOARD_SDCARD_CS
            // Try to init SD card on shared SPI bus
            // SD card uses the same SPI as display/LoRa on T-Deck Plus
            SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN);

            if (SD.begin(BOARD_SDCARD_CS, SPI, 20000000)) {  // 20 MHz (was 4 MHz)
                sdAvailable = true;
                uint8_t cardType = SD.cardType();

                if (cardType == CARD_NONE) {
                    Serial.println("[Storage] No SD card inserted");
                    sdAvailable = false;
                } else {
                    const char* typeStr = "UNKNOWN";
                    if (cardType == CARD_MMC) typeStr = "MMC";
                    else if (cardType == CARD_SD) typeStr = "SDSC";
                    else if (cardType == CARD_SDHC) typeStr = "SDHC";

                    Serial.printf("[Storage] SD card mounted (%s, %lluMB)\n",
                        typeStr, SD.cardSize() / (1024 * 1024));

                    // Create directory structure
                    createDirectoryStructure();
                }
            } else {
                Serial.println("[Storage] SD card init failed, using SPIFFS");
                sdAvailable = false;
            }
        #else
            Serial.println("[Storage] No SD card support, using SPIFFS");
        #endif
    }

    bool isSDAvailable() {
        return sdAvailable;
    }

    // Get paths to different directories
    String getRootPath() {
        return sdAvailable ? String(ROOT_DIR) : "";
    }

    String getMessagesPath() {
        return sdAvailable ? String(MESSAGES_DIR) : "";
    }

    String getInboxPath() {
        return sdAvailable ? String(INBOX_DIR) : "";
    }

    String getOutboxPath() {
        return sdAvailable ? String(OUTBOX_DIR) : "";
    }

    String getContactsPath() {
        return sdAvailable ? String(CONTACTS_DIR) : "";
    }

    String getMapsPath() {
        return sdAvailable ? String(MAPS_DIR) : "";
    }

    bool fileExists(const String& path) {
        if (sdAvailable) {
            // If path starts with /, use it as-is for SD
            if (path.startsWith("/LoRa_Tracker")) {
                return SD.exists(path);
            }
            // Legacy: prepend messages path
            String sdPath = String(MESSAGES_DIR) + path;
            return SD.exists(sdPath);
        }
        return SPIFFS.exists(path);
    }

    File openFile(const String& path, const char* mode) {
        if (sdAvailable) {
            String sdPath;
            // If path starts with /, use it as-is for SD
            if (path.startsWith("/LoRa_Tracker")) {
                sdPath = path;
            } else {
                // Legacy: prepend messages path
                sdPath = String(MESSAGES_DIR) + path;
            }
            // For read mode, check if file exists first to avoid ESP32 error spam
            if (strcmp(mode, "r") == 0 && !SD.exists(sdPath)) {
                return File();  // Return empty File object
            }
            return SD.open(sdPath, mode);
        }
        // For SPIFFS read mode, check existence first
        if (strcmp(mode, "r") == 0 && !SPIFFS.exists(path)) {
            return File();
        }
        return SPIFFS.open(path, mode);
    }

    bool removeFile(const String& path) {
        if (sdAvailable) {
            if (path.startsWith("/LoRa_Tracker")) {
                return SD.remove(path);
            }
            String sdPath = String(MESSAGES_DIR) + path;
            return SD.remove(sdPath);
        }
        return SPIFFS.remove(path);
    }

    bool mkdir(const String& path) {
        if (sdAvailable) {
            String sdPath;
            // If path starts with /LoRa_Tracker, use it as-is
            if (path.startsWith("/LoRa_Tracker")) {
                sdPath = path;
            } else {
                // Prepend messages path for relative paths
                sdPath = String(MESSAGES_DIR) + path;
            }
            return SD.mkdir(sdPath);
        }
        return true;  // SPIFFS doesn't need directories
    }

    // List files in a directory (SD only)
    std::vector<String> listFiles(const String& dirPath) {
        std::vector<String> files;
        if (!sdAvailable) return files;

        File dir = SD.open(dirPath);
        if (!dir || !dir.isDirectory()) {
            return files;
        }

        File file = dir.openNextFile();
        while (file) {
            if (!file.isDirectory()) {
                files.push_back(String(file.name()));
            }
            file = dir.openNextFile();
        }
        dir.close();
        return files;
    }

    // List subdirectories in a directory (SD only)
    std::vector<String> listDirs(const String& dirPath) {
        std::vector<String> dirs;
        if (!sdAvailable) return dirs;

        File dir = SD.open(dirPath);
        if (!dir || !dir.isDirectory()) {
            return dirs;
        }

        File file = dir.openNextFile();
        while (file) {
            if (file.isDirectory()) {
                dirs.push_back(String(file.name()));
            }
            file = dir.openNextFile();
        }
        dir.close();
        return dirs;
    }

    String getStorageType() {
        return sdAvailable ? "SD" : "SPIFFS";
    }

    uint64_t getUsedBytes() {
        if (sdAvailable) {
            return SD.usedBytes();
        }
        return SPIFFS.usedBytes();
    }

    uint64_t getTotalBytes() {
        if (sdAvailable) {
            return SD.totalBytes();
        }
        return SPIFFS.totalBytes();
    }

    // ========== Contacts Management ==========

    std::vector<Contact> loadContacts() {
        if (contactsLoaded) {
            return contactsCache;
        }

        contactsCache.clear();

        if (!sdAvailable) {
            Serial.println("[Storage] No SD card, contacts not available");
            return contactsCache;
        }

        File file = SD.open(CONTACTS_FILE, FILE_READ);
        if (!file) {
            Serial.println("[Storage] No contacts file, starting fresh");
            contactsLoaded = true;
            return contactsCache;
        }

        // Parse JSON
        DynamicJsonDocument doc(4096);
        DeserializationError error = deserializeJson(doc, file);
        file.close();

        if (error) {
            Serial.printf("[Storage] JSON parse error: %s\n", error.c_str());
            contactsLoaded = true;
            return contactsCache;
        }

        // Load contacts from JSON array
        JsonArray array = doc.as<JsonArray>();
        for (JsonObject obj : array) {
            Contact c;
            c.callsign = obj["callsign"].as<String>();
            c.name = obj["name"].as<String>();
            c.comment = obj["comment"].as<String>();
            if (c.callsign.length() > 0) {
                contactsCache.push_back(c);
            }
        }

        Serial.printf("[Storage] Loaded %d contacts\n", contactsCache.size());
        contactsLoaded = true;
        return contactsCache;
    }

    bool saveContacts(const std::vector<Contact>& contacts) {
        if (!sdAvailable) {
            Serial.println("[Storage] No SD card, cannot save contacts");
            return false;
        }

        // Build JSON
        DynamicJsonDocument doc(4096);
        JsonArray array = doc.to<JsonArray>();

        for (const Contact& c : contacts) {
            JsonObject obj = array.createNestedObject();
            obj["callsign"] = c.callsign;
            obj["name"] = c.name;
            obj["comment"] = c.comment;
        }

        // Write to file
        File file = SD.open(CONTACTS_FILE, FILE_WRITE);
        if (!file) {
            Serial.println("[Storage] Failed to open contacts file for writing");
            return false;
        }

        serializeJsonPretty(doc, file);
        file.close();

        // Update cache
        contactsCache = contacts;
        contactsLoaded = true;

        Serial.printf("[Storage] Saved %d contacts\n", contacts.size());
        return true;
    }

    bool addContact(const Contact& contact) {
        // Load existing contacts if not loaded
        if (!contactsLoaded) {
            loadContacts();
        }

        // Check if callsign already exists
        for (const Contact& c : contactsCache) {
            if (c.callsign.equalsIgnoreCase(contact.callsign)) {
                Serial.printf("[Storage] Contact %s already exists\n", contact.callsign.c_str());
                return false;
            }
        }

        // Add to cache
        contactsCache.push_back(contact);

        // Save to file
        return saveContacts(contactsCache);
    }

    bool removeContact(const String& callsign) {
        if (!contactsLoaded) {
            loadContacts();
        }

        for (auto it = contactsCache.begin(); it != contactsCache.end(); ++it) {
            if (it->callsign.equalsIgnoreCase(callsign)) {
                contactsCache.erase(it);
                Serial.printf("[Storage] Removed contact %s\n", callsign.c_str());
                return saveContacts(contactsCache);
            }
        }

        Serial.printf("[Storage] Contact %s not found\n", callsign.c_str());
        return false;
    }

    bool updateContact(const String& callsign, const Contact& newData) {
        if (!contactsLoaded) {
            loadContacts();
        }

        for (Contact& c : contactsCache) {
            if (c.callsign.equalsIgnoreCase(callsign)) {
                c.callsign = newData.callsign;
                c.name = newData.name;
                c.comment = newData.comment;
                Serial.printf("[Storage] Updated contact %s\n", callsign.c_str());
                return saveContacts(contactsCache);
            }
        }

        Serial.printf("[Storage] Contact %s not found for update\n", callsign.c_str());
        return false;
    }

    Contact* findContact(const String& callsign) {
        if (!contactsLoaded) {
            loadContacts();
        }

        for (Contact& c : contactsCache) {
            if (c.callsign.equalsIgnoreCase(callsign)) {
                return &c;
            }
        }
        return nullptr;
    }

    int getContactCount() {
        if (!contactsLoaded) {
            loadContacts();
        }
        return contactsCache.size();
    }

    // ========== Raw Frames Logging ==========

    static const char* FRAMES_FILE = "/LoRa_Tracker/frames.log";
    static const char* FRAMES_OLD_FILE = "/LoRa_Tracker/frames.old";
    static const uint32_t MAX_FRAMES_SIZE = 100 * 1024;  // 100 KB

    void checkFramesLogRotation() {
        if (!sdAvailable) return;

        File file = SD.open(FRAMES_FILE, FILE_READ);
        if (!file) return;

        size_t fileSize = file.size();
        file.close();

        if (fileSize >= MAX_FRAMES_SIZE) {
            Serial.printf("[Storage] Rotating frames log (%u bytes)...\n", fileSize);
            // Remove old backup if exists
            if (SD.exists(FRAMES_OLD_FILE)) {
                SD.remove(FRAMES_OLD_FILE);
            }
            // Rename current to old
            SD.rename(FRAMES_FILE, FRAMES_OLD_FILE);
            Serial.println("[Storage] Frames log rotated");
        }
    }

    // Memory cache for frames - fixed circular buffer
    static const int FRAMES_CACHE_SIZE = 50;
    static String framesCache[FRAMES_CACHE_SIZE];
    static int framesCacheHead = 0;
    static int framesCacheCount = 0;
    static std::vector<String> framesCacheOrdered;  // For returning ordered list

    bool logRawFrame(const String& frame, int rssi, float snr, bool isDirect) {
        // 1. Préparation du timestamp
        char timestamp[64];
        if (year() > 2000) {
            snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d",
                year(), month(), day(), hour(), minute(), second());
        } else {
            snprintf(timestamp, sizeof(timestamp), "----/--/-- --:--:--");
        }

        // 2. Création de la ligne avec le marqueur [D]irect ou [R]elayé
        String statusMarker = isDirect ? "[D]" : "[R]";
        String logLine = statusMarker + String(timestamp) + " GMT: " + frame;

        // 3. Stockage dans le buffer de la mémoire vive (pour l'écran)
        framesCache[framesCacheHead] = logLine;
        framesCacheHead = (framesCacheHead + 1) % FRAMES_CACHE_SIZE;
        if (framesCacheCount < FRAMES_CACHE_SIZE) framesCacheCount++;

        // 4. Écriture sur la carte SD
        if (sdAvailable) {
            checkFramesLogRotation();
            File file = SD.open(FRAMES_FILE, FILE_APPEND);
            if (file) {
                file.println(logLine);
                file.close();
            }
        }
        return true;
    }
    
const std::vector<String>& getLastFrames(int count) {
    framesCacheOrdered.clear();
    
    if (framesCacheCount == 0) return framesCacheOrdered;

    // Limit count to what we actually have
    int actualCount = (count > framesCacheCount) ? framesCacheCount : count;
    framesCacheOrdered.reserve(actualCount);

    // Get the most recent frames (backwards from head)
    for (int i = 0; i < actualCount; i++) {
        int idx = (framesCacheHead - 1 - i + FRAMES_CACHE_SIZE) % FRAMES_CACHE_SIZE;
        framesCacheOrdered.push_back(framesCache[idx]);
    }
    
    return framesCacheOrdered;
}

    // ========== Link Statistics ==========

    // Initialize with inverted min/max so first value always updates them
    static LinkStats linkStats = {0, 0, 0, 0, -200, 0, 50.0f, -50.0f, 0.0f};
    static std::vector<DigiStats> digiStats;
    static std::vector<StationStats> stationStats;

    // History buffers for charts - fixed size circular buffer
    static int rssiHistory[HISTORY_SIZE];
    static float snrHistory[HISTORY_SIZE];
    static int historyCount = 0;  // Number of valid entries
    static int historyHead = 0;   // Next write position

    // Ordered vectors for returning to UI (rebuilt on request)
    static std::vector<int> rssiHistoryOrdered;
    static std::vector<float> snrHistoryOrdered;

    void resetStats() {
        linkStats = {0, 0, 0, 0, -200, 0, 50.0f, -50.0f, 0.0f};
        digiStats.clear();
        stationStats.clear();
        historyCount = 0;
        historyHead = 0;
        rssiHistoryOrdered.clear();
        snrHistoryOrdered.clear();
        Serial.println("[Storage] Stats reset");
    }

    void updateRxStats(int rssi, float snr) {
        linkStats.rxCount++;
        linkStats.rssiTotal += rssi;
        if (rssi < linkStats.rssiMin) linkStats.rssiMin = rssi;
        if (rssi > linkStats.rssiMax) linkStats.rssiMax = rssi;
        linkStats.snrTotal += snr;
        if (snr < linkStats.snrMin) linkStats.snrMin = snr;
        if (snr > linkStats.snrMax) linkStats.snrMax = snr;

        // Add to circular buffer (no memory allocation)
        rssiHistory[historyHead] = rssi;
        snrHistory[historyHead] = snr;
        historyHead = (historyHead + 1) % HISTORY_SIZE;
        if (historyCount < HISTORY_SIZE) historyCount++;
    }

    const std::vector<int>& getRssiHistory() {
        // Rebuild ordered vector from circular buffer
        rssiHistoryOrdered.clear();
        rssiHistoryOrdered.reserve(historyCount);
        int start = (historyCount < HISTORY_SIZE) ? 0 : historyHead;
        for (int i = 0; i < historyCount; i++) {
            rssiHistoryOrdered.push_back(rssiHistory[(start + i) % HISTORY_SIZE]);
        }
        return rssiHistoryOrdered;
    }

    const std::vector<float>& getSnrHistory() {
        // Rebuild ordered vector from circular buffer
        snrHistoryOrdered.clear();
        snrHistoryOrdered.reserve(historyCount);
        int start = (historyCount < HISTORY_SIZE) ? 0 : historyHead;
        for (int i = 0; i < historyCount; i++) {
            snrHistoryOrdered.push_back(snrHistory[(start + i) % HISTORY_SIZE]);
        }
        return snrHistoryOrdered;
    }

    void updateTxStats() {
        linkStats.txCount++;
    }

    void updateAckStats() {
        linkStats.ackCount++;
    }

    void updateDigiStats(const String& path) {
        // Parse path to extract digipeaters
        // Format: DEST,DIGI1*,DIGI2,qAO,IGATE or similar
        // The * indicates which digi actually relayed

        int start = 0;
        int comma = path.indexOf(',');

        while (comma != -1) {
            String segment = path.substring(start, comma);

            // Check if this digi has relayed (marked with *)
            bool relayed = segment.endsWith("*");
            if (relayed) {
                segment = segment.substring(0, segment.length() - 1);
            }

            // Skip q-constructs (qAO, qAR, etc.), WIDE/TRACE, and known generic/service aliases
            if (!segment.startsWith("q") &&
                !segment.startsWith("WIDE") &&
                !segment.startsWith("TRACE") &&
                !segment.equalsIgnoreCase("APLRT1") && // APRS-IS Relay Tracker 1
                !segment.equalsIgnoreCase("APLRG1") && // APRS-IS Relay Gateway 1
                !segment.equalsIgnoreCase("APLRFD") && // APRS-IS Relay Forwarder
                !segment.equalsIgnoreCase("APLRFA") && // APRS-IS Relay Forwarder (alternate)
                !segment.equalsIgnoreCase("APFD11") && // Generic Forwarder
                !segment.equalsIgnoreCase("APWW11") && // Weather station IGate
                segment.length() > 0) {

                // Find or create digi entry
                bool found = false;
                for (auto& d : digiStats) {
                    if (d.callsign.equalsIgnoreCase(segment)) {
                        d.count++;
                        found = true;
                        break;
                    }
                }
                if (!found && digiStats.size() < 50) {  // Limit to 50 digis
                    DigiStats newDigi;
                    newDigi.callsign = segment;
                    newDigi.count = 1;
                    digiStats.push_back(newDigi);
                }
            }

            start = comma + 1;
            comma = path.indexOf(',', start);
        }

        // Handle last segment
        String lastSeg = path.substring(start);
        if (lastSeg.endsWith("*")) {
            lastSeg = lastSeg.substring(0, lastSeg.length() - 1);
        }
        if (!lastSeg.startsWith("q") &&
            !lastSeg.startsWith("WIDE") &&
            !lastSeg.startsWith("TRACE") &&
            !lastSeg.equalsIgnoreCase("APLRT1") && // APRS-IS Relay Tracker 1
            !lastSeg.equalsIgnoreCase("APLRG1") && // APRS-IS Relay Gateway 1
            !lastSeg.equalsIgnoreCase("APLRFD") && // APRS-IS Relay Forwarder
            !lastSeg.equalsIgnoreCase("APLRFA") && // APRS-IS Relay Forwarder (alternate)
            !lastSeg.equalsIgnoreCase("APFD11") && // Generic Forwarder
            !lastSeg.equalsIgnoreCase("APWW11") && // Weather station IGate
            lastSeg.length() > 0) {
            bool found = false;
            for (auto& d : digiStats) {
                if (d.callsign.equalsIgnoreCase(lastSeg)) {
                    d.count++;
                    found = true;
                    break;
                }
            }
            if (!found && digiStats.size() < 50) {
                DigiStats newDigi;
                newDigi.callsign = lastSeg;
                newDigi.count = 1;
                digiStats.push_back(newDigi);
            }
        }
    }

    LinkStats getStats() {
        return linkStats;
    }

    const std::vector<DigiStats>& getDigiStats() {
        return digiStats;
    }

    // Per-station statistics
    void updateStationStats(const String& callsign, int rssi, float snr, bool isDirect) {
        // Check if station already exists
        for (auto& s : stationStats) {
            if (s.callsign.equalsIgnoreCase(callsign)) {
                s.count++;
                s.lastRssi = rssi;
                s.lastSnr = snr;
                s.lastHeard = now(); // Unix timestamp from GPS/RTC
                s.lastIsDirect = isDirect;
                return;
            }
        }

        // New station - add to list (limit to 20)
        if (stationStats.size() < 20) {
            StationStats newStation;
            newStation.callsign = callsign;
            newStation.count = 1;
            newStation.lastRssi = rssi;
            newStation.lastSnr = snr;
            newStation.lastHeard = now();
            newStation.lastIsDirect = isDirect;
            stationStats.push_back(newStation);
            
        }
    }

    const std::vector<StationStats>& getStationStats() {
        return stationStats;
    }

}
