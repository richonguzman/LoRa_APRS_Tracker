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

#include <esp_log.h>
#include <RadioLib.h>
#include <SPI.h>
#if defined(CROWPANEL_ADVANCE_35)
#include "esp32-hal-matrix.h"   // pinMatrixOutAttach/InAttach for GPIO matrix remapping
#include "soc/gpio_sig_map.h"   // SPI3_CLK_OUT_IDX, SPI3_D_IN_IDX, SPI3_Q_OUT_IDX
#endif
#include <freertos/semphr.h>
#include "notification_utils.h"
#include "configuration.h"
#include "board_pinout.h"
#include "lora_utils.h"
#include "display.h"
#include "station_utils.h"
#include "storage_utils.h"
#ifdef USE_LVGL_UI
#include "lvgl_ui.h"
#endif

extern Configuration    Config;
extern LoraType         *currentLoRaType;
extern uint8_t          loraIndex;
extern int              loraIndexSize;
extern SemaphoreHandle_t spiMutex;

static const char *TAG = "LoRa";

// CrowPanel: display owns FSPI (SPI2_HOST), SD owns HSPI (SPI3_HOST) at boot.
// LoRa gets a dedicated SPIClass on HSPI — pin reconfiguration via loraSpiBegin()
// before each LoRa operation (spiMutex protects against SD contention).
#if defined(CROWPANEL_ADVANCE_35)
    SPIClass loraSPI(HSPI);
#endif
static inline void loraSpiBegin() {
    // DIAGNOSTIC: no-op — HSPI stays on LoRa pins from setup
}
static inline void loraSpiEnd() {
    // DIAGNOSTIC: no-op — SD will be broken but LoRa TX/RX should work
}

bool operationDone   = true;
bool transmitFlag    = true;
bool loraInitOk      = false;  // Set true only after successful radio.begin()

// Flags for configuration changes to apply outside ISR
bool pendingFrequencyChange = false;
int pendingLoraIndex = -1;
bool pendingDataRateChange = false;
int pendingDataRate = -1;

#if defined(HAS_SX1262)
    #if defined(CROWPANEL_ADVANCE_35)
        // CrowPanel: LoRa on HSPI, display on FSPI. IO2 shared with TFT RST — managed manually.
        SX1262 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIOLIB_NC, RADIO_BUSY_PIN, loraSPI);
    #else
        SX1262 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
    #endif
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
        // ISR-safe function - just sets a flag
        pendingLoraIndex = newLoraIndex;
        pendingFrequencyChange = true;
    }

    void requestDataRateChange(int newDataRate) {
        // ISR-safe function - just sets a flag
        pendingDataRate = newDataRate;
        pendingDataRateChange = true;
    }

    void processPendingChanges() {
        if (!loraInitOk) return;
        // Call from main loop, not from ISR
        if (pendingFrequencyChange) {
            pendingFrequencyChange = false;
            if (pendingLoraIndex >= 0 && pendingLoraIndex < loraIndexSize) {
                String loraCountryFreq;
                switch (pendingLoraIndex) {
                    case 0: loraCountryFreq = "EU/WORLD"; break;
                    case 1: loraCountryFreq = "POLAND"; break;
                    case 2: loraCountryFreq = "UK"; break;
                    case 3: loraCountryFreq = "US"; break;
                }

                if (pendingLoraIndex != loraIndex) {
                    loraIndex = pendingLoraIndex;
                    #ifndef USE_LVGL_UI
                        displayShow("LORA FREQ>", "", "CHANGED TO: " + loraCountryFreq, "", "", "", 2000);
                    #else
                        LVGL_UI::refreshLoRaInfo();
                    #endif
                    applyLoraConfig();
                    STATION_Utils::saveIndex(1, loraIndex);
                } else {
                    // Already on this frequency, just show confirmation
                    #ifndef USE_LVGL_UI
                        displayShow("LORA FREQ>", "", "ALREADY ON: " + loraCountryFreq, "", "", "", 2000);
                    #endif
                }
            }
        }

        if (pendingDataRateChange) {
            pendingDataRateChange = false;
            if (pendingDataRate > 0) {
                #ifndef USE_LVGL_UI
                    displayShow("DATA RATE>", "", "CHANGED TO: " + String(pendingDataRate) + " bps", "", "", "", 2000);
                #endif
                setDataRate(pendingDataRate);
                STATION_Utils::saveIndex(1, loraIndex);
                #ifdef USE_LVGL_UI
                    LVGL_UI::refreshLoRaInfo();
                #endif
            }
        }
    }

    int calculateDataRate(int sf, int cr, int bw) {
        // Match the 6 standard presets first (keeps compatibility with speed selector UI)
        if (bw == 125000) {
            const struct { int sf; int cr; int rate; } presets[] = {
                {12, 5, 300}, {12, 6, 244}, {12, 7, 209}, {12, 8, 183},
                {10, 8, 610}, {9, 7, 1200}
            };
            for (const auto& p : presets) {
                if (p.sf == sf && p.cr == cr) return p.rate;
            }
        }
        // Compute exact rate for all other SF/CR/BW combinations
        if (sf < 5 || sf > 12 || cr < 5 || cr > 8 || bw <= 0) return 0;
        double rate = (double)sf * ((double)bw / (1 << sf)) * (4.0 / cr);
        return (int)(rate + 0.5);
    }

    DataRateConfig getDataRateConfig(int dataRate) {
        // Map the 6 speeds to their LoRa parameters
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

        // Default value if not found
        return configs[0];  // 300 bps
    }

    int getNextDataRate(int currentDataRate) {
        // The 6 available speeds
        const int dataRates[] = {300, 244, 209, 183, 610, 1200};

        for (int i = 0; i < 6; i++) {
            if (dataRates[i] == currentDataRate) {
                return dataRates[(i + 1) % 6];  // Cycle through the 6 options
            }
        }

        return 300;  // Default value
    }

    void changeDataRate() {
        int currentDataRate = Config.loraTypes[loraIndex].dataRate;
        int nextDataRate = getNextDataRate(currentDataRate);
        setDataRate(nextDataRate);
    }

    void setDataRate(int dataRate) {
        DataRateConfig config = getDataRateConfig(dataRate);

        // Update configuration
        Config.loraTypes[loraIndex].dataRate = config.dataRate;
        Config.loraTypes[loraIndex].spreadingFactor = config.spreadingFactor;
        Config.loraTypes[loraIndex].codingRate4 = config.codingRate4;
        Config.loraTypes[loraIndex].signalBandwidth = config.signalBandwidth;

        currentLoRaType = &Config.loraTypes[loraIndex];

        // Reconfigure radio with new parameters
        if (spiMutex) xSemaphoreTakeRecursive(spiMutex, portMAX_DELAY);
        loraSpiBegin();
        radio.setSpreadingFactor(config.spreadingFactor);
        radio.setCodingRate(config.codingRate4);
        float signalBandwidth = config.signalBandwidth / 1000;
        radio.setBandwidth(signalBandwidth);
        loraSpiEnd();
        if (spiMutex) xSemaphoreGiveRecursive(spiMutex);

        ESP_LOGI(TAG, "Data Rate changed to %d bps (SF%d, CR4/%d)",
                   dataRate, config.spreadingFactor, config.codingRate4);
        Config.writeFile();
    }

    void applyLoraConfig() {
        // Apply current loraIndex configuration to radio
        // Safety check: ensure loraIndex is valid
        if (loraIndex < 0 || loraIndex >= loraIndexSize) {
            ESP_LOGE(TAG, "Invalid loraIndex: %d (max: %d)", loraIndex, loraIndexSize);
            loraIndex = 0;  // Reset to safe default
        }

        currentLoRaType = &Config.loraTypes[loraIndex];

        // Validate frequency and bandwidth values
        #if defined(LORA_FREQ_MIN) && defined(LORA_FREQ_MAX)
            // Board-specific frequency validation
            if (currentLoRaType->frequency < LORA_FREQ_MIN || currentLoRaType->frequency > LORA_FREQ_MAX) {
                ESP_LOGE(TAG, "Frequency %ld Hz out of range (%ld-%ld)",
                    currentLoRaType->frequency, (long)LORA_FREQ_MIN, (long)LORA_FREQ_MAX);
                return;
            }
        #else
            // Generic frequency validation
            if (currentLoRaType->frequency < 100000000 || currentLoRaType->frequency > 1000000000) {
                ESP_LOGE(TAG, "Invalid frequency value: %ld", currentLoRaType->frequency);
                return;
            }
        #endif
        if (currentLoRaType->signalBandwidth < 1000 || currentLoRaType->signalBandwidth > 1000000) {
            ESP_LOGE(TAG, "Invalid bandwidth value: %ld", currentLoRaType->signalBandwidth);
            return;
        }

        float freq = (float)currentLoRaType->frequency/1000000;
        if (spiMutex) xSemaphoreTakeRecursive(spiMutex, portMAX_DELAY);
        loraSpiBegin();
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
        loraSpiEnd();
        if (spiMutex) xSemaphoreGiveRecursive(spiMutex);

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

        ESP_LOGI(TAG, "%s", currentLoRainfo.c_str());
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
        #ifdef CROWPANEL_ADVANCE_35
            // Switch IO45 to LOW to activate Wireless Module header instead of internal MIC
            // IO9 and IO10 are shared between I2S Mic and SPI Radio
            pinMode(45, OUTPUT);
            digitalWrite(45, LOW);
            delay(50); // Small delay for power stabilization
        #endif
        #ifdef LIGHTTRACKER_PLUS_1_0
            pinMode(RADIO_VCC_PIN,OUTPUT);
            digitalWrite(RADIO_VCC_PIN,HIGH);
        #endif
        ESP_LOGD(TAG, "Set SPI pins!");
        #if defined(LIGHTTRACKER_PLUS_1_0) || defined(CROWPANEL_ADVANCE_35)
            loraSPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN, RADIO_CS_PIN);
        #else
            SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN);
        #endif
        float freq = (float)currentLoRaType->frequency/1000000;
        #if defined(CROWPANEL_ADVANCE_35)
            // Manual hardware reset — RadioLib can't use IO2 (shared with TFT_RST)
            pinMode(RADIO_RST_PIN, OUTPUT);
            digitalWrite(RADIO_RST_PIN, LOW);
            delay(1);
            digitalWrite(RADIO_RST_PIN, HIGH);
            delay(5);
        #endif
        #if defined(RADIO_HAS_XTAL)
            radio.XTAL = true;
        #endif
        #if defined(CROWPANEL_ADVANCE_35)
            int state = radio.begin(freq, 125.0, 9, 7, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 10, 8, 3.3);
        #else
            int state = radio.begin(freq);
        #endif
        if (state == RADIOLIB_ERR_NONE) {
            #if defined(HAS_SX1262) || defined(HAS_SX1268)
            ESP_LOGI(TAG, "Initializing SX126X ...");
            #if defined(CROWPANEL_ADVANCE_35)
                radio.setDio2AsRfSwitch(true);
            #endif
            #else
            ESP_LOGI(TAG, "Initializing SX127X ...");
            #endif
        } else {
            ESP_LOGE(TAG, "Starting LoRa failed! State: %d", state);
            #if defined(LIGHTTRACKER_PLUS_1_0) || defined(CROWPANEL_ADVANCE_35)
                loraSpiEnd();
            #else
                SPI.end();
            #endif
            return;
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
            loraInitOk = true;

            #if defined(CROWPANEL_ADVANCE_35)
            // DIAGNOSTIC: probe SPI health over time
            for (int i = 0; i < 6; i++) {
                int16_t st = radio.standby();
                ESP_LOGW(TAG, "DIAG[%d] standby=%d BUSY=%d IO45=%d IO2=%d",
                    i, st, digitalRead(RADIO_BUSY_PIN), digitalRead(45), digitalRead(RADIO_RST_PIN));
                if (i < 5) delay(500);
            }

            // DIAGNOSTIC: test XOSC standby mode before TX
            {
                int16_t xoscState = radio.standby(RADIOLIB_SX126X_STANDBY_XOSC);
                delay(10);
                ESP_LOGW(TAG, "DIAG-XOSC: standby(XOSC)=%d BUSY=%d", xoscState, digitalRead(RADIO_BUSY_PIN));
            }

            // DIAGNOSTIC: TX direct depuis STANDBY (pas de startReceive avant)
            ESP_LOGW(TAG, "DIAG-TX-TEST: startTransmit from STANDBY...");
            uint8_t testData[] = "TEST1234";
            int16_t txStart = radio.startTransmit(testData, 8);
            ESP_LOGW(TAG, "DIAG-TX-TEST: startTransmit()=%d BUSY=%d DIO1=%d", txStart, digitalRead(RADIO_BUSY_PIN), digitalRead(RADIO_DIO1_PIN));

            if (txStart == RADIOLIB_ERR_NONE) {
                // Poll IRQ flags via SPI for up to 5 seconds
                uint32_t txStartMs = millis();
                uint32_t irq = 0;
                while (millis() - txStartMs < 5000) {
                    irq = radio.getIrqFlags();
                    if (irq != 0) {
                        ESP_LOGW(TAG, "DIAG-TX-TEST: IRQ=0x%04X after %lums DIO1=%d", irq, millis() - txStartMs, digitalRead(RADIO_DIO1_PIN));
                        break;
                    }
                    delay(10);
                }
                if (irq == 0) {
                    ESP_LOGE(TAG, "DIAG-TX-TEST: timeout 5s, IRQ still 0x0000 DIO1=%d BUSY=%d", digitalRead(RADIO_DIO1_PIN), digitalRead(RADIO_BUSY_PIN));
                    // Read device errors via raw SPI (getDeviceErrors() is protected)
                    // SX1262 SPI frame: [CMD] [NOP→status] [NOP→errHi] [NOP→errLo]
                    {
                        while (digitalRead(RADIO_BUSY_PIN)) { delay(1); }
                        loraSPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
                        digitalWrite(RADIO_CS_PIN, LOW);
                        loraSPI.transfer(0x17);  // CMD_GET_DEVICE_ERRORS → RFU
                        uint8_t status = loraSPI.transfer(0x00);  // NOP → status
                        uint8_t err_hi = loraSPI.transfer(0x00);  // NOP → errors[15:8]
                        uint8_t err_lo = loraSPI.transfer(0x00);  // NOP → errors[7:0]
                        digitalWrite(RADIO_CS_PIN, HIGH);
                        loraSPI.endTransaction();
                        uint16_t errors = ((uint16_t)err_hi << 8) | err_lo;
                        ESP_LOGE(TAG, "DIAG-TX-TEST: deviceErrors=0x%04X status=0x%02X (RC64K=%d RC13M=%d PLL_CAL=%d ADC=%d IMG=%d XOSC=%d PLL_LOCK=%d PA_RAMP=%d)",
                            errors, status,
                            (errors >> 0) & 1, (errors >> 1) & 1, (errors >> 2) & 1, (errors >> 3) & 1,
                            (errors >> 4) & 1, (errors >> 5) & 1, (errors >> 6) & 1, (errors >> 7) & 1);
                    }
                }
                radio.clearIrqFlags(0xFFFF);
            }
            // IRQ bits: 0x0001=TX_DONE, 0x0002=RX_DONE, 0x0200=TIMEOUT
            #endif

            radio.startReceive();

            #if defined(LIGHTTRACKER_PLUS_1_0) || defined(CROWPANEL_ADVANCE_35)
            loraSpiEnd();  // Restore SD GPIO mapping — LoRa setup done
            #endif
            operationDone = false;  // No pending operation yet — avoid spurious readData()
            transmitFlag = false;   // We're in RX mode, not post-TX
            ESP_LOGI(TAG, "LoRa init done! BUSY=%d", digitalRead(RADIO_BUSY_PIN));
        } else {
            ESP_LOGE(TAG, "LoRa config failed! State: %d", state);
            loraSpiEnd();
            return;
        }
    }

    void sendNewPacket(const String& newPacket) {
        if (!loraInitOk) return;
        ESP_LOGI(TAG, "Tx ---> %s", newPacket.c_str());
        /*logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "LoRa","Send data: %s", newPacket.c_str());
        ESP_LOGE(TAG,"Send data: %s", newPacket.c_str());
        ESP_LOGD(TAG,"Send data: %s", newPacket.c_str());*/

        ESP_LOGD(TAG, "TX: BUSY=%d at entry", digitalRead(RADIO_BUSY_PIN));

        if (Config.ptt.active && Config.ptt.io_pin >= 0 && Config.ptt.io_pin <= 48) {
            digitalWrite(Config.ptt.io_pin, Config.ptt.reverse ? LOW : HIGH);
            delay(Config.ptt.preDelay);
        }
        if (Config.notification.ledTx && Config.notification.ledTxPin >= 0 && Config.notification.ledTxPin <= 48) {
            digitalWrite(Config.notification.ledTxPin, HIGH);
        }
        if (Config.notification.buzzerActive && Config.notification.txBeep) NOTIFICATION_Utils::beaconTxBeep();

        // Acquire SPI mutex — SD card shares the same SPI bus
        ESP_LOGD(TAG, "TX: BUSY=%d after beep", digitalRead(RADIO_BUSY_PIN));
        if (spiMutex) xSemaphoreTakeRecursive(spiMutex, portMAX_DELAY);
        loraSpiBegin();

        // DIAGNOSTIC: probe SPI health while radio is in RX mode (before transmit calls standby)
        int16_t probeStby = radio.standby();
        ESP_LOGW(TAG, "DIAG-TX: standby()=%d BUSY=%d before transmit()", probeStby, digitalRead(RADIO_BUSY_PIN));

        int state = radio.transmit("\x3c\xff\x01" + newPacket);
        transmitFlag = true;
        loraSpiEnd();
        if (spiMutex) xSemaphoreGiveRecursive(spiMutex);
        if (state == RADIOLIB_ERR_NONE) {
            STORAGE_Utils::updateTxStats();
        } else {
            ESP_LOGE(TAG, "Tx failed, code %d", state);
        }

        if (Config.notification.ledTx && Config.notification.ledTxPin >= 0 && Config.notification.ledTxPin <= 48) {
            digitalWrite(Config.notification.ledTxPin, LOW);
        }
        if (Config.ptt.active && Config.ptt.io_pin >= 0 && Config.ptt.io_pin <= 48) {
            delay(Config.ptt.postDelay);
            digitalWrite(Config.ptt.io_pin, Config.ptt.reverse ? HIGH : LOW);
        }
    }

    void wakeRadio() {
        if (spiMutex) xSemaphoreTakeRecursive(spiMutex, portMAX_DELAY);
        loraSpiBegin();
        radio.startReceive();
        loraSpiEnd();
        if (spiMutex) xSemaphoreGiveRecursive(spiMutex);
    }

    ReceivedLoRaPacket receiveFromSleep() {
        ReceivedLoRaPacket receivedLoraPacket;
        String packet = "";
        if (spiMutex) xSemaphoreTakeRecursive(spiMutex, portMAX_DELAY);
        loraSpiBegin();
        int state = radio.readData(packet);
        if (state == RADIOLIB_ERR_NONE) {
            receivedLoraPacket.text       = packet;
            receivedLoraPacket.rssi       = radio.getRSSI();
            receivedLoraPacket.snr        = radio.getSNR();
            receivedLoraPacket.freqError  = radio.getFrequencyError();
        } else {
            //
        }
        loraSpiEnd();
        if (spiMutex) xSemaphoreGiveRecursive(spiMutex);
        return receivedLoraPacket;
    }

    ReceivedLoRaPacket receivePacket() {
        ReceivedLoRaPacket receivedLoraPacket;
        if (!loraInitOk) return receivedLoraPacket;
        String packet = "";
        if (operationDone) {
            ESP_LOGW(TAG, "operationDone=true, transmitFlag=%d, BUSY=%d", transmitFlag, digitalRead(RADIO_BUSY_PIN));
            operationDone = false;
            if (spiMutex) xSemaphoreTakeRecursive(spiMutex, portMAX_DELAY);
            loraSpiBegin();
            if (transmitFlag) {
                radio.startReceive();
                transmitFlag = false;
            } else {
                int state = radio.readData(packet);
                ESP_LOGD(TAG, "readData state=%d, BUSY=%d", state, digitalRead(RADIO_BUSY_PIN));
                if (state == RADIOLIB_ERR_NONE) {
                    if(!packet.isEmpty()) {
                        ESP_LOGI(TAG, "Rx ---> %s", packet.substring(3).c_str());
                        receivedLoraPacket.text       = packet;
                        receivedLoraPacket.rssi       = radio.getRSSI();
                        receivedLoraPacket.snr        = radio.getSNR();
                        receivedLoraPacket.freqError  = radio.getFrequencyError();
                    }
                } else {
                    ESP_LOGE(TAG, "Rx failed, code %d", state);  // 7 = CRC mismatch
                }
            }
            loraSpiEnd();
            if (spiMutex) xSemaphoreGiveRecursive(spiMutex);
        }
        return receivedLoraPacket;
    }

    void sleepRadio() {
        if (!loraInitOk) return;
        if (spiMutex) xSemaphoreTakeRecursive(spiMutex, portMAX_DELAY);
        loraSpiBegin();
        radio.sleep();
        loraSpiEnd();
        if (spiMutex) xSemaphoreGiveRecursive(spiMutex);
    }

}