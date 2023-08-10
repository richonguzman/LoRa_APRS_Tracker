#include <RadioLib.h>
#include <logger.h>
#include <LoRa.h>
#include <SPI.h>
#include "configuration.h"
#include "pins_config.h"
#include "msg_utils.h"
#include "display.h"

extern logging::Logger logger;
extern Configuration Config;

#if defined(TTGO_T_Beam_V1_0_SX1268)
SX1268 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
#endif

namespace LoRa_Utils {

  void setup() {
    #if defined(TTGO_T_Beam_V1_0_SX1268)
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "Set SPI pins!");
    SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN);
    float freq = (float)Config.loramodule.frequency/1000000.0;
    int state = radio.begin(freq);
    if (state == RADIOLIB_ERR_NONE)
    {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "Initializing SX1268");
    } else {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "LoRa", "Starting LoRa failed!");
      while (true);
    }
    radio.setSpreadingFactor(Config.loramodule.spreadingFactor);
    radio.setBandwidth(Config.loramodule.signalBandwidth);
    radio.setCodingRate(Config.loramodule.codingRate4);
    state = radio.setOutputPower(Config.loramodule.power + 2);

    if (state == RADIOLIB_ERR_NONE) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "LoRa init done!");
    } else {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "LoRa", "Starting LoRa failed!");
      while (true);
    }
    #endif
    #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_LORA_V2_1) || defined(TTGO_T_Beam_V1_2)
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "Set SPI pins!");
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "Set LoRa pins!");
    LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);

    long freq = Config.loramodule.frequency;
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "frequency: %d", freq);
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
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa","Send data: %s", newPacket.c_str());

    if (Config.ptt.active) {
      digitalWrite(Config.ptt.io_pin, Config.ptt.reverse ? LOW : HIGH);
      delay(Config.ptt.preDelay);
    }
    #if defined(TTGO_T_Beam_V1_0_SX1268)
    Serial.print("Transmiting... ");
    int state = radio.transmit("\x3c\xff\x01" + newPacket);
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println(F("success!"));

      // print measured data rate
      Serial.print(F("[SX1268] Datarate:\t"));
      Serial.print(radio.getDataRate());
      Serial.println(F(" bps"));

    } else if (state == RADIOLIB_ERR_PACKET_TOO_LONG) {
      Serial.println(F("too long!"));

    } else if (state == RADIOLIB_ERR_TX_TIMEOUT) {
      Serial.println(F("timeout!"));

    } else {
      Serial.print(F("failed, code "));
      Serial.println(state);
    }
    #endif
    #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_LORA_V2_1) || defined(TTGO_T_Beam_V1_2)
    LoRa.beginPacket();
    LoRa.write('<');
    LoRa.write(0xFF);
    LoRa.write(0x01);
    LoRa.write((const uint8_t *)newPacket.c_str(), newPacket.length());
    LoRa.endPacket();
    #endif
    if (Config.ptt.active) {
      delay(Config.ptt.postDelay);
      digitalWrite(Config.ptt.io_pin, Config.ptt.reverse ? HIGH : LOW);
    }
  }

  String receivePacket() {
    String loraPacket = "";
    #if defined(TTGO_T_Beam_V1_0) || defined(TTGO_T_LORA_V2_1) || defined(TTGO_T_Beam_V1_2)
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      while (LoRa.available()) {
        int inChar = LoRa.read();
        loraPacket += (char)inChar;
      }

      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa","Receive data: %s", loraPacket.c_str());
    }
     #endif
    #if defined(TTGO_T_Beam_V1_0_SX1268)
    int state = radio.receive(loraPacket);
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println(F("success!"));
      Serial.print(F("[SX1268] Data:\t\t")); Serial.println(loraPacket);
    } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
      // timeout occurred while waiting for a packet
      //Serial.println(F("timeout!"));
    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
      Serial.println(F("CRC error!"));
    } else {
      Serial.print(F("failed, code "));
      Serial.println(state);
    }
    #endif
    return loraPacket;
  }

}