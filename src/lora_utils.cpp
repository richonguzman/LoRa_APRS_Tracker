#include <RadioLib.h>
#include <logger.h>
#include <SPI.h>
#include "notification_utils.h"
#include "configuration.h"
#include "board_pinout.h"
#include "lora_utils.h"
#include "display.h"

extern logging::Logger  logger;
extern Configuration    Config;
extern LoraType         *currentLoRaType;
extern uint8_t          loraIndex;
extern int              loraIndexSize;

bool operationDone   = true;
bool transmitFlag    = true;

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

    void changeFreq() {
        if(loraIndex >= (loraIndexSize - 1)) {
            loraIndex = 0;
        } else {
            loraIndex++;
        }
        currentLoRaType = &Config.loraTypes[loraIndex];

        float freq = (float)currentLoRaType->frequency/1000000;
        radio.setFrequency(freq);
        radio.setSpreadingFactor(currentLoRaType->spreadingFactor);
        float signalBandwidth = currentLoRaType->signalBandwidth/1000;
        radio.setBandwidth(signalBandwidth);
        radio.setCodingRate(currentLoRaType->codingRate4);
        #if (defined(HAS_SX1268) || defined(HAS_SX1262)) && !defined(HAS_1W_LORA)
            radio.setOutputPower(currentLoRaType->power + 2); // values available: 10, 17, 22 --> if 20 in tracker_conf.json it will be updated to 22.
        #endif
        #if defined(HAS_SX1278) || defined(HAS_SX1276) || defined(HAS_1W_LORA)
            radio.setOutputPower(currentLoRaType->power);
        #endif

        String loraCountryFreq;
        switch (loraIndex) {
            case 0: loraCountryFreq = "EU/WORLD"; break;
            case 1: loraCountryFreq = "POLAND"; break;
            case 2: loraCountryFreq = "UK"; break;
        }
        String currentLoRainfo = "LoRa ";
        currentLoRainfo += loraCountryFreq;
        currentLoRainfo += " / Freq: ";
        currentLoRainfo += String(currentLoRaType->frequency);
        currentLoRainfo += " / SF:";
        currentLoRainfo += String(currentLoRaType->spreadingFactor);
        currentLoRainfo += " / CR: ";
        currentLoRainfo += String(currentLoRaType->codingRate4);
        
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "LoRa", currentLoRainfo.c_str());
        displayShow("LORA FREQ>", "", "CHANGED TO: " + loraCountryFreq, "", "", "", 2000);
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
            Serial.print(F("failed, code "));
            Serial.println(state);
        }
        
        if (Config.notification.ledTx) digitalWrite(Config.notification.ledTxPin, LOW);
        if (Config.ptt.active) {
            delay(Config.ptt.postDelay);
            digitalWrite(Config.ptt.io_pin, Config.ptt.reverse ? HIGH : LOW);
        }
        /*#ifdef HAS_TFT
            cleanTFT();
        #endif*/
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
                    Serial.print(F("failed, code "));   // 7 = CRC mismatch
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