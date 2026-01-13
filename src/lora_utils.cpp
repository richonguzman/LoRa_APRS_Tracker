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

#include <RadioLib.h>
#include <logger.h>
#include <SPI.h>
#include "notification_utils.h"
#include "configuration.h"
#include "board_pinout.h"
#include "lora_utils.h"
#include "display.h"
#include "station_utils.h"

extern logging::Logger  logger;
extern Configuration    Config;
extern LoraType         *currentLoRaType;
extern uint8_t          loraIndex;
extern int              loraIndexSize;

bool operationDone   = true;
bool transmitFlag    = true;

// Flags pour les changements de configuration à appliquer hors ISR
bool pendingFrequencyChange = false;
int pendingLoraIndex = -1;
bool pendingDataRateChange = false;
int pendingDataRate = -1;

#if defined(HAS_SX1262)
    SX1262 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
#endif
#if defined(HAS_SX1268)
    #if defined(LIGHTTRACKER_PLUS_1_0)
        SPIClass loraSPI(FSPI);
        SX1268 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN, loraSPI); 
    #else
        SX1268 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
    #endif
#endif
#if defined(HAS_SX1278)
    SX1278 radio = new Module(RADIO_CS_PIN, RADIO_BUSY_PIN, RADIO_RST_PIN);
#endif
#if defined(HAS_SX1276)
    SX1276 radio = new Module(RADIO_CS_PIN, RADIO_BUSY_PIN, RADIO_RST_PIN);
#endif
#if defined(HAS_LLCC68) //  LLCC68 supports spreading factor only in range of 5-11!
    LLCC68 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
#endif

namespace LoRa_Utils {

    void setFlag(void) {
        operationDone = true;
    }

    void requestFrequencyChange(int newLoraIndex) {
        // Fonction sûre à appeler depuis ISR - positionne juste un flag
        pendingLoraIndex = newLoraIndex;
        pendingFrequencyChange = true;
    }

    void requestDataRateChange(int newDataRate) {
        // Fonction sûre à appeler depuis ISR - positionne juste un flag
        pendingDataRate = newDataRate;
        pendingDataRateChange = true;
    }

    void processPendingChanges() {
        // À appeler depuis la boucle principale (loop), pas depuis ISR
        if (pendingFrequencyChange) {
            pendingFrequencyChange = false;
            if (pendingLoraIndex != loraIndex && pendingLoraIndex >= 0 && pendingLoraIndex < loraIndexSize) {
                loraIndex = pendingLoraIndex;
                applyLoraConfig();
                STATION_Utils::saveIndex(1, loraIndex);
            }
        }

        if (pendingDataRateChange) {
            pendingDataRateChange = false;
            if (pendingDataRate > 0) {
                setDataRate(pendingDataRate);
                STATION_Utils::saveIndex(1, loraIndex);
            }
        }
    }

    int calculateDataRate(int sf, int cr, int bw) {
        // Simplified lookup table for BW=125kHz (most common)
        // Based on actual LoRa specifications
        if (bw == 125000) {
            // Lookup table: [SF][CR-5] (CR stored as 5,6,7,8)
            const int dataRates[13][4] = {
                {0, 0, 0, 0},      // SF 0 (unused)
                {0, 0, 0, 0},      // SF 1 (unused)
                {0, 0, 0, 0},      // SF 2 (unused)
                {0, 0, 0, 0},      // SF 3 (unused)
                {0, 0, 0, 0},      // SF 4 (unused)
                {0, 0, 0, 0},      // SF 5 (unused)
                {0, 0, 0, 0},      // SF 6 (unused)
                {5470, 4440, 3810, 3330},   // SF 7
                {3125, 2540, 2180, 1910},   // SF 8
                {1760, 1430, 1200, 1070},   // SF 9
                {980, 800, 680, 610},       // SF 10
                {540, 440, 380, 330},       // SF 11
                {300, 244, 209, 183}        // SF 12
            };

            if (sf >= 7 && sf <= 12 && cr >= 5 && cr <= 8) {
                return dataRates[sf][cr - 5];
            }
        }

        // Fallback for other bandwidths or invalid values
        return 0;
    }

    DataRateConfig getDataRateConfig(int dataRate) {
        // Mapping des 6 vitesses vers leurs paramètres LoRa
        const DataRateConfig configs[] = {
            {300,  12, 5, 125000},  // SF12, CR4/5
            {244,  12, 6, 125000},  // SF12, CR4/6
            {209,  12, 7, 125000},  // SF12, CR4/7
            {183,  12, 8, 125000},  // SF12, CR4/8
            {610,  10, 8, 125000},  // SF10, CR4/8
            {1200,  9, 7, 125000}   // SF9, CR4/7
        };

        for (int i = 0; i < 6; i++) {
            if (configs[i].dataRate == dataRate) {
                return configs[i];
            }
        }

        // Valeur par défaut si non trouvé
        return configs[0];  // 300 bps
    }

    int getNextDataRate(int currentDataRate) {
        // Les 6 vitesses disponibles
        const int dataRates[] = {300, 244, 209, 183, 610, 1200};

        for (int i = 0; i < 6; i++) {
            if (dataRates[i] == currentDataRate) {
                return dataRates[(i + 1) % 6];  // Cycle à travers les 6 options
            }
        }

        return 300;  // Valeur par défaut
    }

    void changeDataRate() {
        int currentDataRate = Config.loraTypes[loraIndex].dataRate;
        int nextDataRate = getNextDataRate(currentDataRate);
        setDataRate(nextDataRate);
    }

    void setDataRate(int dataRate) {
        DataRateConfig config = getDataRateConfig(dataRate);

        // Mise à jour de la configuration
        Config.loraTypes[loraIndex].dataRate = config.dataRate;
        Config.loraTypes[loraIndex].spreadingFactor = config.spreadingFactor;
        Config.loraTypes[loraIndex].codingRate4 = config.codingRate4;
        Config.loraTypes[loraIndex].signalBandwidth = config.signalBandwidth;

        currentLoRaType = &Config.loraTypes[loraIndex];

        // Reconfigurer la radio avec les nouveaux paramètres
        radio.setSpreadingFactor(config.spreadingFactor);
        radio.setCodingRate(config.codingRate4);
        float signalBandwidth = config.signalBandwidth / 1000;
        radio.setBandwidth(signalBandwidth);

        displayShow("LoRa", "Data Rate", String(dataRate) + " bps", 1000);
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "Data Rate changed to %d bps (SF%d, CR4/%d)",
                   dataRate, config.spreadingFactor, config.codingRate4);
        Config.writeFile();
    }

    void applyLoraConfig() {
        // Apply current loraIndex configuration to radio
        // Safety check: ensure loraIndex is valid
        if (loraIndex < 0 || loraIndex >= loraIndexSize) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "LoRa", "Invalid loraIndex: %d (max: %d)", loraIndex, loraIndexSize);
            loraIndex = 0;  // Reset to safe default
        }

        currentLoRaType = &Config.loraTypes[loraIndex];

        // Validate frequency and bandwidth values
        if (currentLoRaType->frequency < 100000000 || currentLoRaType->frequency > 1000000000) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "LoRa", "Invalid frequency value: %ld", currentLoRaType->frequency);
            return;
        }
        if (currentLoRaType->signalBandwidth < 1000 || currentLoRaType->signalBandwidth > 1000000) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "LoRa", "Invalid bandwidth value: %ld", currentLoRaType->signalBandwidth);
            return;
        }

        float freq = (float)currentLoRaType->frequency/1000000;
        radio.setFrequency(freq);
        radio.setSpreadingFactor(currentLoRaType->spreadingFactor);
        float signalBandwidth = currentLoRaType->signalBandwidth/1000;
        radio.setBandwidth(signalBandwidth);
        radio.setCodingRate(currentLoRaType->codingRate4);
        #if (defined(HAS_SX1268) || defined(HAS_SX1262)) && !defined(HAS_1W_LORA)
            radio.setOutputPower(currentLoRaType->power + 2);
        #endif
        #if defined(HAS_SX1278) || defined(HAS_SX1276) || defined(HAS_1W_LORA)
            radio.setOutputPower(currentLoRaType->power);
        #endif

        String loraCountryFreq;
        switch (loraIndex) {
            case 0: loraCountryFreq = "EU/WORLD"; break;
            case 1: loraCountryFreq = "POLAND"; break;
            case 2: loraCountryFreq = "UK"; break;
            case 3: loraCountryFreq = "US"; break;
        }
        String currentLoRainfo = "LoRa ";
        currentLoRainfo += loraCountryFreq;
        currentLoRainfo += " / Freq: ";
        currentLoRainfo += String(currentLoRaType->frequency);
        currentLoRainfo += " / SF:";
        currentLoRainfo += String(currentLoRaType->spreadingFactor);
        currentLoRainfo += " / CR: ";
        currentLoRainfo += String(currentLoRaType->codingRate4);

        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", currentLoRainfo.c_str());
        displayShow("LORA FREQ>", "", "CHANGED TO: " + loraCountryFreq, "", "", "", 2000);
    }

    void changeFreq() {
        // Cycle to next frequency
        if(loraIndex >= (loraIndexSize - 1)) {
            loraIndex = 0;
        } else {
            loraIndex++;
        }
        applyLoraConfig();
    }

    void setup() {
        #ifdef LIGHTTRACKER_PLUS_1_0
            pinMode(RADIO_VCC_PIN,OUTPUT);
            digitalWrite(RADIO_VCC_PIN,HIGH);
        #endif
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "LoRa", "Set SPI pins!");
        #if defined(LIGHTTRACKER_PLUS_1_0)
            loraSPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN, RADIO_CS_PIN);
        #else
            SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN);
        #endif
        float freq = (float)currentLoRaType->frequency/1000000;
        #if defined(RADIO_HAS_XTAL)
            radio.XTAL = true;
        #endif
        int state = radio.begin(freq);
        if (state == RADIOLIB_ERR_NONE) {
            #if defined(HAS_SX1262) || defined(HAS_SX1268)
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "Initializing SX126X ...");
            #else
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "Initializing SX127X ...");
            #endif
        } else {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "LoRa", "Starting LoRa failed! State: %d", state);
            while (true);
        }
        #if defined(HAS_SX1262) || defined(HAS_SX1268) || defined(HAS_LLCC68)
            radio.setDio1Action(setFlag);
        #endif
        #if defined(HAS_SX1278) || defined(HAS_SX1276)
            radio.setDio0Action(setFlag, RISING);
        #endif
        radio.setSpreadingFactor(currentLoRaType->spreadingFactor);
        float signalBandwidth = currentLoRaType->signalBandwidth/1000;
        radio.setBandwidth(signalBandwidth);
        radio.setCodingRate(currentLoRaType->codingRate4);
        radio.setCRC(true);
        
        #if defined(RADIO_RXEN) && defined(RADIO_TXEN)
            radio.setRfSwitchPins(RADIO_RXEN, RADIO_TXEN);
        #endif

        #ifdef HAS_1W_LORA  // Ebyte E22 400M30S (SX1268) / 900M30S (SX1262) / Ebyte E220 400M30S (LLCC68)
            state = radio.setOutputPower(currentLoRaType->power); // max value 20 (when 20dB in setup 30dB in output as 400M30S has Low Noise Amp)
            radio.setCurrentLimit(140); // to be validated (100 , 120, 140)?
        #endif

        #if (defined(HAS_SX1268) || defined(HAS_SX1262)) && !defined(HAS_1W_LORA)
            state = radio.setOutputPower(currentLoRaType->power + 2); // values available: 10, 17, 22 --> if 20 in tracker_conf.json it will be updated to 22.
            radio.setCurrentLimit(140);
        #endif
        
        #if defined(HAS_SX1278) || defined(HAS_SX1276)
            state = radio.setOutputPower(currentLoRaType->power);
            radio.setCurrentLimit(100); // to be validated (80 , 100)?
        #endif

        #if defined(HAS_SX1262) || defined(HAS_SX1268) || defined(HAS_LLCC68)
        radio.setRxBoostedGainMode(true);
        #endif

        if (state == RADIOLIB_ERR_NONE) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "LoRa init done!");
        } else {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "LoRa", "Starting LoRa failed! State: %d", state);
            while (true);
        }        
    }

    void sendNewPacket(const String& newPacket) {
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa Tx","---> %s", newPacket.c_str());
        /*logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "LoRa","Send data: %s", newPacket.c_str());
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "LoRa","Send data: %s", newPacket.c_str());
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "LoRa","Send data: %s", newPacket.c_str());*/

        if (Config.ptt.active) {
            digitalWrite(Config.ptt.io_pin, Config.ptt.reverse ? LOW : HIGH);
            delay(Config.ptt.preDelay);
        }
        if (Config.notification.ledTx) digitalWrite(Config.notification.ledTxPin, HIGH);
        if (Config.notification.buzzerActive && Config.notification.txBeep) NOTIFICATION_Utils::beaconTxBeep();
        
        int state = radio.transmit("\x3c\xff\x01" + newPacket);
        transmitFlag = true;
        if (state == RADIOLIB_ERR_NONE) {
            //Serial.println(F("success!"));
        } else {
            Serial.print(F("Tx failed, code "));
            Serial.println(state);
        }
        
        if (Config.notification.ledTx) digitalWrite(Config.notification.ledTxPin, LOW);
        if (Config.ptt.active) {
            delay(Config.ptt.postDelay);
            digitalWrite(Config.ptt.io_pin, Config.ptt.reverse ? HIGH : LOW);
        }
    }

    void wakeRadio() {
        radio.startReceive();
    }

    ReceivedLoRaPacket receiveFromSleep() {
        ReceivedLoRaPacket receivedLoraPacket;
        String packet = "";
        int state = radio.readData(packet);
        if (state == RADIOLIB_ERR_NONE) {
            receivedLoraPacket.text       = packet;
            receivedLoraPacket.rssi       = radio.getRSSI();
            receivedLoraPacket.snr        = radio.getSNR();
            receivedLoraPacket.freqError  = radio.getFrequencyError();
        } else {
            //
        }
        return receivedLoraPacket;
    }

    ReceivedLoRaPacket receivePacket() {
        ReceivedLoRaPacket receivedLoraPacket;
        String packet = "";
        if (operationDone) {
            operationDone = false;
            if (transmitFlag) {
                radio.startReceive();
                transmitFlag = false;
            } else {
                int state = radio.readData(packet);
                if (state == RADIOLIB_ERR_NONE) {
                    if(!packet.isEmpty()) {
                        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa Rx","---> %s", packet.substring(3).c_str());
                        receivedLoraPacket.text       = packet;
                        receivedLoraPacket.rssi       = radio.getRSSI();
                        receivedLoraPacket.snr        = radio.getSNR();
                        receivedLoraPacket.freqError  = radio.getFrequencyError();
                    }
                } else {
                    Serial.print(F("Rx failed, code "));   // 7 = CRC mismatch
                    Serial.println(state);
                }
            }
        }
        return receivedLoraPacket;
    }

    void sleepRadio() {
        radio.sleep();
    }

}