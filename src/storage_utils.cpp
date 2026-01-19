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

            if (SD.begin(BOARD_SDCARD_CS, SPI, 4000000)) {
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
            return SD.mkdir(path);
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

}
