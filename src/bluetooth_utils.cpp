#include <TinyGPS++.h>
#include <esp_bt.h>
#include "bluetooth_utils.h"
#include "configuration.h"
#include "KISS_TO_TNC2.h"
#include "lora_utils.h"
#include "display.h"
#include "logger.h"


extern Configuration    Config;
extern BluetoothSerial  SerialBT;
extern logging::Logger  logger;
extern TinyGPSPlus      gps;
extern bool             bluetoothConnected;
extern bool             bluetoothActive;

namespace BLUETOOTH_Utils {
  String serialReceived;
  bool shouldSendToLoRa = false;
  bool useKiss = false;

  void setup() {
    if (!bluetoothActive) {
      btStop();
      esp_bt_controller_disable();
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "BT controller disabled");
      return;
    }

    serialReceived.reserve(255);

    SerialBT.register_callback(BLUETOOTH_Utils::bluetoothCallback);
    SerialBT.onData(BLUETOOTH_Utils::getData); // callback instead of while to avoid RX buffer limit when NMEA data received

    uint8_t dmac[6];
    esp_efuse_mac_get_default(dmac);
    char ourId[5];
    snprintf(ourId, sizeof(ourId), "%02x%02x", dmac[4], dmac[5]);

    if (!SerialBT.begin(String("LoRa Tracker " + String(ourId)))) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "Bluetooth", "Starting Bluetooth failed!");
      show_display("ERROR", "Starting Bluetooth failed!");
      while(true) {
        delay(1000);
      }
    }
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Bluetooth", "Bluetooth init done!");
  }

  void bluetoothCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
    if (event == ESP_SPP_SRV_OPEN_EVT) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Bluetooth", "Client connected !");
      bluetoothConnected = true;
      useKiss = false;
    } else if (event == ESP_SPP_CLOSE_EVT) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Bluetooth", "Client disconnected !");
      bluetoothConnected = false;
    } else {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Bluetooth", "Status: %d", event);
    }
  }

  void getData(const uint8_t *buffer, size_t size) {
    if (size == 0) {
      return;
    }

    shouldSendToLoRa = false;
    serialReceived.clear();

    bool isNmea = buffer[0] == '$';

    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "bluetooth", "Received buffer size %d. Nmea=%d. %s", size, isNmea, buffer);

    for (int i = 0; i < size; i++) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "bluetooth", "[%d/%d] %x -> %c", i + 1, size, buffer[i], buffer[i]);
    }

    for (int i = 0; i < size; i++) {
      char c = (char) buffer[i];

      if (isNmea) {
        gps.encode(c);
      } else {
        serialReceived += c;
      }
    }

    // Test if we have to send frame
    isNmea = serialReceived.indexOf("$G") != -1 || serialReceived.indexOf("$B") != -1;

    if (isNmea) {
      useKiss = false;
    }

    if (isNmea || serialReceived.isEmpty()) {
      return;
    }

    if (validateKISSFrame(serialReceived)) {
      bool dataFrame;

      String decodeKiss = decode_kiss(serialReceived, dataFrame);
      serialReceived.clear();
      serialReceived += decodeKiss;

      logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "bluetooth", "It's a kiss frame. dataFrame: %d", dataFrame);

      useKiss = true;
    } else {
      useKiss = false;
    }

    if (validateTNC2Frame(serialReceived)) {
      shouldSendToLoRa = true;

      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "bluetooth",
                 "Data received should be transmitted to RF => %s", serialReceived.c_str());
      // because we can't send data here
    }
  }

  void sendToLoRa() {
    if (!shouldSendToLoRa) {
      return;
    }

    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BT TX", "%s", serialReceived.c_str());
    show_display("BT Tx >>", "", serialReceived, 1000);
    LoRa_Utils::sendNewPacket(serialReceived);
    shouldSendToLoRa = false;
  }

  void sendPacket(const String& packet) {
    if (bluetoothActive && !packet.isEmpty()) {
      if (useKiss) {
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BT RX Kiss", "%s", serialReceived.c_str());
        SerialBT.println(encode_kiss(packet));
      } else {
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BT RX TNC2", "%s", serialReceived.c_str());
        SerialBT.println(packet);
      }
    }
  }
}