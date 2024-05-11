#include <logger.h>
#include <SPI.h>
#include "notification_utils.h"
#include "configuration.h"
#include "pins_config.h"
#include "msg_utils.h"
#include "display.h"
#ifdef HAS_SX127X
#include <LoRa.h>
#endif
#ifdef HAS_SX126X
#include <RadioLib.h>
#endif


extern logging::Logger  logger;
extern Configuration    Config;
extern LoraType         *currentLoRaType;
extern uint8_t          loraIndex;
extern int              loraIndexSize;


#if defined(TTGO_T_Beam_V1_0_SX1268) || defined(ESP32_DIY_1W_LoRa_GPS) || defined(OE5HWN_MeshCom)
SX1268 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
bool transmissionFlag = true;
bool enableInterrupt = true;
#endif
#if defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3) || defined(HELTEC_V3_GPS) || defined(HELTEC_WIRELESS_TRACKER) || defined(TTGO_T_DECK_GPS)
SX1262 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
bool transmissionFlag = true;
bool enableInterrupt = true;
#endif

namespace LoRa_Utils {

    void setFlag() {
        #ifdef HAS_SX126X
        transmissionFlag = true;
        #endif
    }

    void changeFreq() {
        if(loraIndex >= (loraIndexSize - 1)) {
            loraIndex = 0;
        } else {
            loraIndex++;
        }
        currentLoRaType = &Config.loraTypes[loraIndex];
        #ifdef HAS_SX126X
        float freq = (float)currentLoRaType->frequency/1000000;
        radio.setFrequency(freq);
        radio.setSpreadingFactor(currentLoRaType->spreadingFactor);
        float signalBandwidth = currentLoRaType->signalBandwidth/1000;
        radio.setBandwidth(signalBandwidth);
        radio.setCodingRate(currentLoRaType->codingRate4);
        #if defined(TTGO_T_Beam_V1_0_SX1268) || defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3) || defined(HELTEC_V3_GPS) || defined(HELTEC_WIRELESS_TRACKER) || defined(TTGO_T_DECK_GPS)
        radio.setOutputPower(currentLoRaType->power + 2); // values available: 10, 17, 22 --> if 20 in tracker_conf.json it will be updated to 22.
        radio.setCurrentLimit(140);
        #endif
        #if defined(ESP32_DIY_1W_LoRa_GPS) || defined(OE5HWN_MeshCom)
        radio.setOutputPower(currentLoRaType->power);
        radio.setCurrentLimit(140);     // still needs to be validated
        #endif
        radio.setRxBoostedGainMode(true);
        #endif
        #ifdef HAS_SX127X
        LoRa.setFrequency(currentLoRaType->frequency);
        LoRa.setSpreadingFactor(currentLoRaType->spreadingFactor);
        LoRa.setSignalBandwidth(currentLoRaType->signalBandwidth);
        LoRa.setCodingRate4(currentLoRaType->codingRate4);
        LoRa.setTxPower(currentLoRaType->power);
        #endif
        String loraCountryFreq;
        switch (loraIndex) {
            case 0:
                loraCountryFreq = "EU/WORLD";
                break;
            case 1:
                loraCountryFreq = "POLAND";
                break;
            case 2:
                loraCountryFreq = "UK";
                break;
        }
        String currentLoRainfo = "LoRa " + loraCountryFreq + " / Freq: " + String(currentLoRaType->frequency)  + " / SF:" + String(currentLoRaType->spreadingFactor) + " / CR: " + String(currentLoRaType->codingRate4);
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", currentLoRainfo.c_str());
        show_display("LORA FREQ>", "", "CHANGED TO: " + loraCountryFreq, "", "", "", 2000);
    }

    void setup() {
        #ifdef HAS_SX126X
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "Set SPI pins!");
        SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN);
        float freq = (float)currentLoRaType->frequency/1000000;
        int state = radio.begin(freq);
        if (state == RADIOLIB_ERR_NONE) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "Initializing SX126X");
        } else {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "LoRa", "Starting LoRa failed!");
            while (true);
        }
        radio.setDio1Action(setFlag);
        radio.setSpreadingFactor(currentLoRaType->spreadingFactor);
        float signalBandwidth = currentLoRaType->signalBandwidth/1000;
        radio.setBandwidth(signalBandwidth);
        radio.setCodingRate(currentLoRaType->codingRate4);
        radio.setCRC(true);
        #if defined(ESP32_DIY_1W_LoRa_GPS) || defined(OE5HWN_MeshCom)
        radio.setRfSwitchPins(RADIO_RXEN, RADIO_TXEN);
        #endif
        #if defined(TTGO_T_Beam_V1_0_SX1268) || defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3) || defined(HELTEC_V3_GPS) || defined(HELTEC_WIRELESS_TRACKER) || defined(TTGO_T_DECK_GPS)
        state = radio.setOutputPower(currentLoRaType->power + 2); // values available: 10, 17, 22 --> if 20 in tracker_conf.json it will be updated to 22.
        radio.setCurrentLimit(140);
        #endif
        #if defined(ESP32_DIY_1W_LoRa_GPS) || defined(OE5HWN_MeshCom)
        state = radio.setOutputPower(currentLoRaType->power); // max value 20 (when 20dB in setup 30dB in output as 400M30S has Low Noise Amp)
        radio.setCurrentLimit(140); // still needs to be validated
        #endif
        radio.setRxBoostedGainMode(true);
        if (state == RADIOLIB_ERR_NONE) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "LoRa init done!");
        } else {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "LoRa", "Starting LoRa failed!");
            while (true);
        }
        #endif
        #ifdef HAS_SX127X
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "Set SPI pins!");
        SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
        LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);
        long freq = currentLoRaType->frequency;
        if (!LoRa.begin(freq)) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "LoRa", "Starting LoRa failed!");
            show_display("ERROR", "Starting LoRa failed!");
            while (true) {
                delay(1000);
            }
        }
        LoRa.setSpreadingFactor(currentLoRaType->spreadingFactor);
        LoRa.setSignalBandwidth(currentLoRaType->signalBandwidth);
        LoRa.setCodingRate4(currentLoRaType->codingRate4);
        LoRa.enableCrc();
        LoRa.setTxPower(currentLoRaType->power);
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "LoRa init done!");
        String currentLoRainfo = "LoRa Freq: " + String(currentLoRaType->frequency)  + " / SF:" + String(currentLoRaType->spreadingFactor) + " / CR: " + String(currentLoRaType->codingRate4);
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", currentLoRainfo.c_str());
        #endif
    }

    void sendNewPacket(const String &newPacket) {
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
        #ifdef HAS_SX126X
        int state = radio.transmit("\x3c\xff\x01" + newPacket);
        if (state == RADIOLIB_ERR_NONE) {
            //Serial.println(F("success!"));
        } else if (state == RADIOLIB_ERR_PACKET_TOO_LONG) {
            Serial.println(F("too long!"));
        } else if (state == RADIOLIB_ERR_TX_TIMEOUT) {
            Serial.println(F("timeout!"));
        } else {
            Serial.print(F("failed, code "));
            Serial.println(state);
        }
        #endif
        #ifdef HAS_SX127X
        LoRa.beginPacket();
        LoRa.write('<');
        LoRa.write(0xFF);
        LoRa.write(0x01);
        LoRa.write((const uint8_t *)newPacket.c_str(), newPacket.length());
        LoRa.endPacket();
        #endif
        if (Config.notification.ledTx) digitalWrite(Config.notification.ledTxPin, LOW);
        if (Config.ptt.active) {
            delay(Config.ptt.postDelay);
            digitalWrite(Config.ptt.io_pin, Config.ptt.reverse ? HIGH : LOW);
        }
        #ifdef HAS_TFT
        cleanTFT();
        #endif
    }

    ReceivedLoRaPacket receivePacket() {
        ReceivedLoRaPacket receivedLoraPacket;
        String packet = "";
        #ifdef HAS_SX127X
        int packetSize = LoRa.parsePacket();
        if (packetSize) {
            while (LoRa.available()) {
                int inChar = LoRa.read();
                packet += (char)inChar;
            }
            receivedLoraPacket.text       = packet;
            receivedLoraPacket.rssi       = LoRa.packetRssi();
            receivedLoraPacket.snr        = LoRa.packetSnr();
            receivedLoraPacket.freqError  = LoRa.packetFrequencyError();
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa Rx", "---> %s", packet.c_str());
        }
        #endif
        #ifdef HAS_SX126X
        if (transmissionFlag) {
            transmissionFlag = false;
            radio.startReceive();
            int state = radio.readData(packet);
            if (state == RADIOLIB_ERR_NONE) {
                if(!packet.isEmpty()) {
                    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa Rx","---> %s", packet.c_str());
                }
                receivedLoraPacket.text       = packet;
                receivedLoraPacket.rssi       = radio.getRSSI();
                receivedLoraPacket.snr        = radio.getSNR();
                receivedLoraPacket.freqError  = radio.getFrequencyError();
            } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
                // timeout occurred while waiting for a packet
            } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
                Serial.println(F("CRC error!"));
            } else {
                Serial.print(F("failed, code "));
                Serial.println(state);
            }
        }
        #endif
        return receivedLoraPacket;
    }

}