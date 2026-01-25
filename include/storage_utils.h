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

#ifndef STORAGE_UTILS_H_
#define STORAGE_UTILS_H_

#include <Arduino.h>
#include <FS.h>
#include <vector>

// Contact structure
struct Contact {
    String callsign;
    String name;
    String comment;
};

// Link statistics structure
struct LinkStats {
    uint32_t rxCount;       // Total frames received
    uint32_t txCount;       // Total frames transmitted
    uint32_t ackCount;      // ACKs received
    int rssiMin;
    int rssiMax;
    int32_t rssiTotal;      // For average calculation
    float snrMin;
    float snrMax;
    float snrTotal;         // For average calculation
};

// Digipeater statistics
struct DigiStats {
    String callsign;
    uint32_t count;         // Times seen in path
};

// Per-station statistics
struct StationStats {
    String callsign;
    uint32_t count;         // Total packets received
    int lastRssi;
    float lastSnr;
    uint32_t lastHeard;     // Unix timestamp
    bool lastIsDirect;
};

namespace STORAGE_Utils {

    void    setup();
    bool    isSDAvailable();

    // Path getters
    String  getRootPath();      // /LoRa_Tracker
    String  getMessagesPath();  // /LoRa_Tracker/Messages
    String  getInboxPath();     // /LoRa_Tracker/Messages/inbox
    String  getOutboxPath();    // /LoRa_Tracker/Messages/outbox
    String  getContactsPath();  // /LoRa_Tracker/Contacts
    String  getMapsPath();      // /LoRa_Tracker/Maps

    // File operations
    bool    fileExists(const String& path);
    File    openFile(const String& path, const char* mode);
    bool    removeFile(const String& path);
    bool    mkdir(const String& path);

    // Directory listing (SD only)
    std::vector<String> listFiles(const String& dirPath);
    std::vector<String> listDirs(const String& dirPath);

    // Storage info
    String  getStorageType();
    uint64_t getUsedBytes();
    uint64_t getTotalBytes();

    // Contacts management
    std::vector<Contact> loadContacts();
    bool saveContacts(const std::vector<Contact>& contacts);
    bool addContact(const Contact& contact);
    bool removeContact(const String& callsign);
    bool updateContact(const String& callsign, const Contact& newData);
    Contact* findContact(const String& callsign);
    int getContactCount();

    // Raw frames logging
    bool logRawFrame(const String& frame, int rssi, float snr, bool isDirect);
    void updateStationStats(const String& callsign, int rssi, float snr, bool isDirect);
    const std::vector<String>& getLastFrames(int count);
    void checkFramesLogRotation();

    // Link statistics
    void resetStats();
    void updateRxStats(int rssi, float snr);
    void updateTxStats();
    void updateAckStats();
    void updateDigiStats(const String& path);
    LinkStats getStats();
    const std::vector<DigiStats>& getDigiStats();

    // Per-station statistics
    void updateStationStats(const String& callsign, int rssi, float snr);
    const std::vector<StationStats>& getStationStats();

    // History for charts (last 50 values)
    const int HISTORY_SIZE = 50;
    const std::vector<int>& getRssiHistory();
    const std::vector<float>& getSnrHistory();

}

#endif
