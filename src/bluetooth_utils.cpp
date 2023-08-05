#include <esp_bt.h>
#include "bluetooth_utils.h"
#include "logger.h"
#include "display.h"
#include "lora_utils.h"
#include "configuration.h"
#include "TinyGPSPlus.h"

extern Configuration    Config;
extern BluetoothSerial  SerialBT;
extern logging::Logger  logger;
extern TinyGPSPlus      gps;

namespace BLUETOOTH_Utils {
  String serialReceived;
  bool shouldSendToLoRa = false;

  void setup() {
    if (!Config.bluetooth) {
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
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Bluetooth", "Bluetooth client connected !");
    } else if (event == ESP_SPP_CLOSE_EVT) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Bluetooth", "Bluetooth client disconnected !");
    }
  }

  void getData(const uint8_t *buffer, size_t size) {
    if (size == 0) {
      return;
    }

    shouldSendToLoRa = false;
    serialReceived.clear();

    bool isNmea = buffer[0] == '$';

    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "bluetooth", "Received buffer size %d. Nmea=%d", size, isNmea);

    for (int i = 0; i < size; i++) {
      char c = (char) buffer[i];

      if (isNmea) {
        gps.encode(c);
      } else {
        serialReceived += c;
      }
    }

    shouldSendToLoRa = !serialReceived.isEmpty(); // because we can't send data here

    if (shouldSendToLoRa) {
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "bluetooth",
                   "Data received should be transmitted to RF => %s", serialReceived.c_str());
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
    if (Config.bluetooth && !packet.isEmpty()) {
      SerialBT.println(packet);
    }
  }

}