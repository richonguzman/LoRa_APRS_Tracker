#include <RadioLib.h>
#include <logger.h>
#include <LoRa.h>
#include <SPI.h>
#include "notification_utils.h"
#include "configuration.h"
#include "pins_config.h"
#include "msg_utils.h"
#include "display.h"

extern logging::Logger logger;
extern Configuration Config;

#if defined(TTGO_T_Beam_V1_0_SX1268) || defined(ESP32_DIY_1W_LoRa_GPS)
SX1268 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
bool transmissionFlag = true;
bool enableInterrupt = true;
#endif
#if defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3) || defined(HELTEC_V3_GPS)
SX1262 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
bool transmissionFlag = true;
bool enableInterrupt = true;
#endif

namespace LoRa_Utils {

  void setFlag() {
    #if defined(TTGO_T_Beam_V1_0_SX1268) || defined(ESP32_DIY_1W_LoRa_GPS) || defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3) || defined(HELTEC_V3_GPS)
    transmissionFlag = true;
    #endif
  }

  void setup() {
    #if defined(TTGO_T_Beam_V1_0_SX1268) || defined(ESP32_DIY_1W_LoRa_GPS) || defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3) || defined(HELTEC_V3_GPS)
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "Set SPI pins!");
    SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN);
    float freq = (float)Config.loramodule.frequency/1000000;
    int state = radio.begin(freq);
    if (state == RADIOLIB_ERR_NONE) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "Initializing SX1268");
    } else {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "LoRa", "Starting LoRa failed!");
      while (true);
    }
    radio.setDio1Action(setFlag);
    radio.setSpreadingFactor(Config.loramodule.spreadingFactor);
    radio.setBandwidth(Config.loramodule.signalBandwidth);
    radio.setCodingRate(Config.loramodule.codingRate4);
    #if defined(TTGO_T_Beam_V1_0_SX1268) || defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3) || defined(HELTEC_V3_GPS)
    state = radio.setOutputPower(Config.loramodule.power + 2); // values available: 10, 17, 22 --> if 20 in tracker_conf.json it will be updated to 22.
    #endif
    #ifdef ESP32_DIY_1W_LoRa_GPS
    state = radio.setOutputPower(Config.loramodule.power); // max value 20 (when 20dB in setup 30dB in output as 400M30S has Low Noise Amp) 
    #endif
    if (state == RADIOLIB_ERR_NONE) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "LoRa init done!");
    } else {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "LoRa", "Starting LoRa failed!");
      while (true);
    }
    #endif
    #if defined(TTGO_T_Beam_V0_7) || defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_LORA32_V2_1_TNC) || defined(TTGO_T_Beam_V1_2) || defined(ESP32_DIY_LoRa_GPS) || defined(TTGO_T_LORA32_V2_1_GPS)
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "Set SPI pins!");
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
    LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);

    long freq = Config.loramodule.frequency;
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "Frequency: %d", freq);
    if (!LoRa.begin(freq)) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "LoRa", "Starting LoRa failed!");
      show_display("ERROR", "Starting LoRa failed!");
      while (true) {
        delay(1000);
      }
    }
    LoRa.setSpreadingFactor(Config.loramodule.spreadingFactor);
    LoRa.setSignalBandwidth(Config.loramodule.signalBandwidth);
    LoRa.setCodingRate4(Config.loramodule.codingRate4);
    LoRa.enableCrc();

    LoRa.setTxPower(Config.loramodule.power);
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "LoRa init done!");
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
    if (Config.notification.ledTx){
      digitalWrite(Config.notification.ledTxPin, HIGH);
    }
    if (Config.notification.buzzerActive && Config.notification.txBeep) {
      NOTIFICATION_Utils::beaconTxBeep();
    }
    #if defined(TTGO_T_Beam_V1_0_SX1268) || defined(ESP32_DIY_1W_LoRa_GPS) || defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3) || defined(HELTEC_V3_GPS)
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
    #if defined(TTGO_T_Beam_V0_7) || defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_LORA32_V2_1_TNC) || defined(TTGO_T_Beam_V1_2) || defined(ESP32_DIY_LoRa_GPS) || defined(TTGO_T_LORA32_V2_1_GPS)
    LoRa.beginPacket();
    LoRa.write('<');
    LoRa.write(0xFF);
    LoRa.write(0x01);
    LoRa.write((const uint8_t *)newPacket.c_str(), newPacket.length());
    LoRa.endPacket();
    #endif
    if (Config.notification.ledTx){
      digitalWrite(Config.notification.ledTxPin, LOW);
    }
    if (Config.ptt.active) {
      delay(Config.ptt.postDelay);
      digitalWrite(Config.ptt.io_pin, Config.ptt.reverse ? HIGH : LOW);
    }
  }

  ReceivedLoRaPacket receivePacket() {
    ReceivedLoRaPacket receivedLoraPacket;
    String packet = "";
    #if defined(TTGO_T_Beam_V0_7) || defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_LORA32_V2_1_TNC) || defined(TTGO_T_Beam_V1_2) || defined(ESP32_DIY_LoRa_GPS) || defined(TTGO_T_LORA32_V2_1_GPS)
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
    #if defined(TTGO_T_Beam_V1_0_SX1268) || defined(ESP32_DIY_1W_LoRa_GPS) || defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3) || defined(HELTEC_V3_GPS)
    if (transmissionFlag) {
      transmissionFlag = false;
      radio.startReceive();
      int state = radio.readData(packet);
      if (state == RADIOLIB_ERR_NONE) {
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa Rx","---> %s", packet.c_str());
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