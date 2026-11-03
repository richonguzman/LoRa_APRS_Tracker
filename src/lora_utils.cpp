/* Copyright (C) 2026 Ricardo Guzman - CA2RXU
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

#include <APRSPacketLib.h>
#include <RadioLib.h>
#include <logger.h>
#include <SPI.h>

#include "configuration.h"
#include "thermal_utils.h"
#include "board_pinout.h"
#include "lora_utils.h"
#include "display.h"
#include "led_utils.h"
#include "log_buffer.h"
#ifdef FAN_CTRL_PIN
#include "thermal_utils.h"
#endif

extern logging::Logger  logger;
extern Configuration    Config;
extern LoraType         *currentLoRaType;

bool operationDone   = true;
bool transmitFlag    = true;
// millis() of the last time an RX-done interrupt was actually consumed
// (successful read or CRC/decode failure) — see STATION_Utils::processOutputPacketBuffer()
// for why a queued TX waits for a short quiet window since this timestamp.
uint32_t lastRxActivityMs = 0;

#define DIFS_SLOTS      2       // Number of secuential CAD slots to consider a free channel to Tx
int     backoffMax      = 4;    // Max Backoff value (number of CAD slots to wait before Tx)
#define CAD_MAX_WAIT_MS 10000   // Max total time waiting for a free channel before Tx anyway
unsigned long cadStartTime = 0;
bool cadNoticeShown = false;

#if defined(HAS_SX1262)
    // On HELTEC_T114, variants_bsp/heltec_t114/variant.h sets PIN_SPI_MISO/MOSI/SCK
    // to the LoRa pins (P0.23/22/19), so the BSP's default `SPI` global is already
    // the LoRa bus — no custom SPIClass needed, and SPIM2 stays free for SPI1 (TFT).
    // LORANGER_V1 and TTGO_T_BEAM_1W have the SX1262 on FSPI (IO11/12/13); use a
    // dedicated SPIClass so the bus is configured to the correct pins.
    #if defined(LORANGER_V1) || defined(TTGO_T_BEAM_1W)
        SPIClass loraSPI(FSPI);
        SX1262 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN, loraSPI);
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

#if defined(HAS_SX1278) || defined(HAS_SX1276)
    #define CHIP_MIN_POWER  2       // PA_BOOST (below 2 RadioLib switches to RFO, not wired on most modules)
    #define CHIP_MAX_POWER  20
#else                               // SX1262 / SX1268 / LLCC68
    #define CHIP_MIN_POWER  -9
    #define CHIP_MAX_POWER  22
#endif

#ifndef RADIO_MAX_POWER             // optional per board in board_pinout.h (e.g. 1W PA modules)
    #define RADIO_MAX_POWER CHIP_MAX_POWER
#endif

namespace LoRa_Utils {

    void setFlag(void) {
        operationDone = true;
    }

    // Applies currentLoRaType->power to the radio for the active chip family,
    // clamping per-family to the driver's valid range (bare SX126x) or the
    // external PA's documented-safe input level (HAS_1W_LORA). Shared by
    // setup() and changeFreq() so the two call sites can't drift out of sync.
    int applyOutputPower() {
        int state = RADIOLIB_ERR_NONE;
        #ifdef HAS_1W_LORA
            // Ebyte E22 400M30S (SX1262) / 900M30S / E220 400M30S (LLCC68) driving
            // an external 30 dBm (1 W) PA. Chip-level drive is only validated up
            // to 20 dBm (-> ~30 dBm/1W rated PA output via the module's internal
            // LNA). RadioLib's own range check won't catch an over-drive here
            // (22 dBm is still chip-legal), so clamp defensively — a stale/corrupt
            // config or a user-typed CLI value must not push the PA past its
            // documented operating point.
            int8_t clampedPower = constrain(currentLoRaType->power, 2, 20);
            state = radio.setOutputPower(clampedPower);
            radio.setCurrentLimit(140); // to be validated (100 , 120, 140)?
        #elif defined(HAS_SX1268) || defined(HAS_SX1262)
            // SX1262 valid range is -9..+22 dBm. The +2 offset is upstream's
            // user-friendly bump (config 20 -> chip 22). If the on-flash
            // config has a value outside the safe band, the call returns
            // -13 (INVALID_OUTPUT_POWER) and aborts the whole init. Clamp
            // here so a stale or corrupt config can't brick startup.
            int8_t clampedPower = constrain((int)currentLoRaType->power + 2, -9, 22);
            state = radio.setOutputPower(clampedPower);
            radio.setCurrentLimit(140);
        #elif defined(HAS_SX1278) || defined(HAS_SX1276)
            state = radio.setOutputPower(currentLoRaType->power);
            radio.setCurrentLimit(100); // to be validated (80 , 100)?
        #endif
        return state;
    }

    void changeFreq() {
        // Single LoRa profile — nothing to cycle.
        // Apply current profile settings (in case they changed via CLI/web).
        float freq = (float)currentLoRaType->frequency/1000000;
        radio.setFrequency(freq);
        radio.setSpreadingFactor(currentLoRaType->spreadingFactor);
        float signalBandwidth = currentLoRaType->signalBandwidth / 1000.0f;
        radio.setBandwidth(signalBandwidth);
        radio.setCodingRate(currentLoRaType->codingRate4);
        applyOutputPower();
        #if defined(TTGO_T_BEAM_1W)
            radio.setPaRampTime(RADIOLIB_SX126X_PA_RAMP_800U);  // setOutputPower() resets the PA ramp time; restore it
        #endif

        String currentLoRainfo = "LoRa / Freq: ";
        currentLoRainfo += String(currentLoRaType->frequency);
        currentLoRainfo += " / SF:";
        currentLoRainfo += String(currentLoRaType->spreadingFactor);
        currentLoRainfo += " / CR: ";
        currentLoRainfo += String(currentLoRaType->codingRate4);

        logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "LoRa", currentLoRainfo.c_str());
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "Freq: %ld", currentLoRaType->frequency);
    }

    void setup() {
        #if defined(LIGHTTRACKER_PLUS_1_0) || defined(TTGO_T_BEAM_1W)
            pinMode(RADIO_VCC_PIN,OUTPUT);
            digitalWrite(RADIO_VCC_PIN,HIGH);
        #endif
        #if defined(TTGO_T_BEAM_1W)
            pinMode(RADIO_RXEN, OUTPUT);
            digitalWrite(RADIO_RXEN, LOW);  // start setup in Tx mode
        #endif

        logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "LoRa", "Set SPI pins!");
        #if defined(LIGHTTRACKER_PLUS_1_0) || defined(LORANGER_V1) || defined(TTGO_T_BEAM_1W)
            loraSPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN, RADIO_CS_PIN);
        #else
            #ifdef ARDUINO_ARCH_NRF52
                SPI.begin();   // Adafruit nRF52 BSP's SPI.begin() takes no args; pins are fixed by the BSP variant (PIN_SPI_*)
            #else
                SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN);
            #endif
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
        float signalBandwidth = currentLoRaType->signalBandwidth / 1000.0f;
        radio.setBandwidth(signalBandwidth);
        radio.setCodingRate(currentLoRaType->codingRate4);
        radio.setCRC(true);

        #if defined(RADIO_RXEN) && defined(RADIO_TXEN)
            radio.setRfSwitchPins(RADIO_RXEN, RADIO_TXEN);
        #endif
        #if defined(TTGO_T_BEAM_1W)
            radio.setRfSwitchPins(RADIO_RXEN, RADIOLIB_NC);
        #endif

        state = applyOutputPower();

        #if defined(HAS_SX1262) || defined(HAS_SX1268) || defined(HAS_LLCC68)
            radio.setRxBoostedGainMode(true);
        #endif

        #if defined(HAS_TCXO) && !defined(HAS_1W_LORA)
            radio.setDio2AsRfSwitch();
        #endif
        #if defined(TTGO_T_BEAM_1W)
            radio.setDio2AsRfSwitch(true);                          // DIO2 drives the 1W PA
            radio.setPaRampTime(RADIOLIB_SX126X_PA_RAMP_800U);      // PA needs >800us to settle (default 200us)
        #endif
        #ifdef HAS_TCXO
            radio.setTCXO(1.8);
        #endif

        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "LoRa init done!");
    }

    bool doCAD() {      // CAD (Channel Activity Detection)
        return radio.scanChannel() != RADIOLIB_CHANNEL_FREE;    // false=channel free | true=RADIOLIB_LORA_DETECTED or CAD failed
    }

    bool doDIFS() {
        for (uint8_t i = DIFS_SLOTS; i > 0; i--) {
            if (doCAD()) return false;
        }
        return true;
    }

    bool cadTimedOut() {
        return millis() - cadStartTime > CAD_MAX_WAIT_MS;
    }

    void showCadNotice() {
        if (cadNoticeShown) return;
        cadNoticeShown = true;
        bootStatus("CAD: waiting for free channel...");
    }

    void waitForDIFS() {
        while (!doDIFS()) {
            if (cadTimedOut()) return;
            showCadNotice();
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "LoRa Tx", "CAD/DIFS failed, retry...");
        }
    }

    void doBEB() {
        int backoffCounter = random(1, backoffMax + 1);
        while (backoffCounter > 0) {
            if (cadTimedOut()) return;
            if (doCAD()) {
                showCadNotice();
                waitForDIFS();  // busy channel: freeze backoff and restart DIFS
            } else {
                backoffCounter--;
            }
        }
    }

    void sendNewPacket(const String& newPacket) {
        if (Config.cadActive) {
            cadStartTime = millis();
            cadNoticeShown = false;
            waitForDIFS();  // DIFS (Distributed Inter-Frame Space)
            doBEB();        // BEB  (Binary Exponential Backoff)
            if (cadTimedOut()) {
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "LoRa Tx", "CAD timeout, transmitting anyway");
            }
            // Note: our fork's displayTx(newPacket) below handles the TX overlay;
            // upstream's redundant displayShow() here would double-render.
        }
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa Tx","---> %s", newPacket.c_str());
        /*logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "LoRa","Send data: %s", newPacket.c_str());
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "LoRa","Send data: %s", newPacket.c_str());
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "LoRa","Send data: %s", newPacket.c_str());*/

        // FCC TX-gate: refuse to key the carrier if the source callsign field
        // looks like a placeholder (NOCALL / N0CALL). Catches misconfigured
        // local beacons and any externally-injected packets via BLE/BT-Classic
        // KISS paths. Load-bearing now that nRF52 first-boot writes a default
        // config including NOCALL-7 — without this gate, a fresh device would
        // TX before the operator can configure a real callsign.
        int sepPos = newPacket.indexOf('>');
        String fromCall = (sepPos > 0) ? newPacket.substring(0, sepPos) : newPacket;
        if (APRSPacketLib::checkNocall(fromCall)) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "LoRa Tx",
                       "TX BLOCKED: source callsign \"%s\" is a placeholder; configure a valid callsign before transmitting",
                       fromCall.c_str());
            return;
        }

        LogBuffer::pushf(LogBuffer::TYPE_TX, "%s", newPacket.c_str());
        displayTx(newPacket);

        if (Config.ptt.active) {
            digitalWrite(Config.ptt.io_pin, Config.ptt.reverse ? LOW : HIGH);
            delay(Config.ptt.preDelay);
        }
        LED_Utils::txRxFlash();

        #if defined(TTGO_T_BEAM_1W)
            digitalWrite(RADIO_RXEN, LOW);
        #endif
        #ifdef FAN_CTRL_PIN
            THERMAL_Utils::onTxStart();
        #endif
        // Diagnostic bracket: confirms execution actually reaches radio.transmit()
        // rather than hanging in the display/LED/PTT calls above it.
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "LoRa Tx", "calling transmit(), len %d", newPacket.length());
        int state = radio.transmit("\x3c\xff\x01" + newPacket);
        // Diagnostic bracket for an unresolved digipeat-only hang: confirms
        // radio.transmit() actually returned rather than being stuck inside
        // RadioLib. Pair with the "RX re-armed" log in receivePacket() below —
        // whichever of the two never prints next pinpoints the hang location.
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "LoRa Tx", "transmit() returned, code %d", state);
        #ifdef FAN_CTRL_PIN
            THERMAL_Utils::onTxEnd();
        #endif
        transmitFlag = true;
        if (state == RADIOLIB_ERR_NONE) {
            //Serial.println(F("success!"));
        } else {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "LoRa Tx", "Tx failed, code %d", state);
        }

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
        #if defined(TTGO_T_BEAM_1W)
            digitalWrite(RADIO_RXEN, HIGH);
        #endif
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
                #if defined(TTGO_T_BEAM_1W)
                    digitalWrite(RADIO_RXEN, HIGH);
                #endif
                radio.startReceive();
                // Diagnostic bracket, pairs with the "transmit() returned" log
                // above — confirms the post-TX RX re-arm call itself returned.
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "LoRa Rx", "RX re-armed after TX");
                transmitFlag = false;
            } else {
                // Mark the radio as "just touched by RX" regardless of outcome
                // (clean read or CRC/decode failure) — a queued digipeat/ack TX
                // waits for this to go quiet before keying. Confirmed on-hardware:
                // transmitting while the chip is still settling from a just-consumed
                // RX-done interrupt triggers RadioLib's SX126x::launchMode() unbounded
                // BUSY-wait hang (see STATION_Utils::processOutputPacketBuffer()).
                lastRxActivityMs = millis();
                int state = radio.readData(packet);
                if (state == RADIOLIB_ERR_NONE) {
                    if(packet.length() != 0) {
                        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa Rx","---> %s", packet.substring(3).c_str());
                        LogBuffer::pushf(LogBuffer::TYPE_RX, "%s  [RSSI:%d SNR:%.0f]",
                            packet.substring(3).c_str(), (int)radio.getRSSI(), (double)radio.getSNR());
                        receivedLoraPacket.text       = packet;
                        receivedLoraPacket.rssi       = radio.getRSSI();
                        receivedLoraPacket.snr        = radio.getSNR();
                        receivedLoraPacket.freqError  = radio.getFrequencyError();
                    }
                } else {
                    // 7 = CRC mismatch
                    logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "LoRa Rx", "Rx failed, code %d", state);
                }
            }
        }
        return receivedLoraPacket;
    }

    void sleepRadio() {
        radio.sleep();
    }

}