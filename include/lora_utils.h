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

#ifndef LORA_UTILS_H_
#define LORA_UTILS_H_

#include <Arduino.h>

struct ReceivedLoRaPacket {
    String  text;
    int     rssi;
    float   snr;
    int     freqError;
};

struct DataRateConfig {
    int dataRate;
    int spreadingFactor;
    int codingRate4;
    long signalBandwidth;
};

// Flags externes pour les changements de configuration pendants
extern bool pendingFrequencyChange;
extern int pendingLoraIndex;
extern bool pendingDataRateChange;
extern int pendingDataRate;

namespace LoRa_Utils {

    void setFlag();
    void requestFrequencyChange(int newLoraIndex);
    void requestDataRateChange(int newDataRate);
    void processPendingChanges();
    int calculateDataRate(int sf, int cr, int bw);
    void changeFreq();
    void applyLoraConfig();
    void changeDataRate();
    void setDataRate(int dataRate);
    DataRateConfig getDataRateConfig(int dataRate);
    int getNextDataRate(int currentDataRate);
    void setup();
    void sendNewPacket(const String& newPacket);
    void wakeRadio();
    ReceivedLoRaPacket receiveFromSleep();
    ReceivedLoRaPacket receivePacket();
    void sleepRadio();

}

#endif