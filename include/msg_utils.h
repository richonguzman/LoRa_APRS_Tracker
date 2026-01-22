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

#ifndef MSG_UTILS_H_
#define MSG_UTILS_H_

#include <Arduino.h>
#include <vector>
#include "lora_utils.h"

struct Packet15SegBuffer {
    uint32_t    receivedTime;
    String      station;
    String      payload;
};

namespace MSG_Utils {

    bool    warnNoAPRSMessages();
    bool    warnNoWLNKMails();
    const String getLastHeardTracker();
    int     getNumAPRSMessages();
    int     getNumWLNKMails();
    void    loadNumMessages();
    std::vector<String>& getLoadedAPRSMessages();
    std::vector<String>& getLoadedWLNKMails();
    std::vector<String> getMessagesForContact(const String& callsign);
    std::vector<String> getConversationsList();  // Get list of callsigns with conversations
    void    loadMessagesFromMemory(uint8_t typeOfMessage);
    void    ledNotification();
    void    deleteFile(uint8_t typeOfFile);
    bool    deleteMessageByIndex(uint8_t typeOfMessage, int index);
    bool    deleteMessageFromConversation(const String& callsign, int index);
    void    saveNewMessage(uint8_t typeMessage, const String& station, const String& newMessage);
    void    sendMessage(const String& station, const String& textMessage);
    void    addToOutputBuffer(uint8_t typeOfMessage, const String& station, const String& textMessage);
    void    processOutputBuffer();
    void    clean15SegBuffer();
    bool    check15SegBuffer(const String& station, const String& textMessage);
    void    checkReceivedMessage(ReceivedLoRaPacket packetReceived);

}

#endif