#ifndef BLUETOOTH_UTILS_H
#define BLUETOOTH_UTILS_H

#include <BluetoothSerial.h>

namespace BLUETOOTH_Utils {

  void setup();
  void bluetoothCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param);
  void getData(const uint8_t *buffer, size_t size);
  void sendToLoRa();
  void sendPacket(const String& packet);
  
}

#endif
