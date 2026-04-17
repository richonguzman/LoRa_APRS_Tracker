/* LoRa radio — RadioLib SX1262 with ESP32-C3 native SPI HAL */

#include "lora_radio.h"
#include "esp_c3_hal.h"
#include "pinout.h"

#include "esp_log.h"

static const char *TAG = "lora";

/* HAL + Module + Radio instances */
static EspC3Hal *hal = nullptr;
static Module *mod = nullptr;
static SX1262 *radio = nullptr;

/* RX state */
static volatile bool operationDone = false;
static volatile bool inTxMode = false;
static lora_rx_cb_t rxCallback = nullptr;

/* ISR flag */
static void IRAM_ATTR onDio1Isr(void) {
    operationDone = true;
}

/* Convert bandwidth index (7=125k, 8=250k, 9=500k) to kHz */
static float bwIndexToKhz(uint8_t bw) {
    switch (bw) {
        case 7:  return 125.0f;
        case 8:  return 250.0f;
        case 9:  return 500.0f;
        default: return 125.0f;
    }
}

extern "C" int lora_init(const lora_config_t *cfg) {
    hal = new EspC3Hal(LORA_SCK_PIN, LORA_MISO_PIN, LORA_MOSI_PIN);
    mod = new Module(hal, LORA_CS_PIN, LORA_DIO1_PIN, LORA_RST_PIN, LORA_BUSY_PIN);
    radio = new SX1262(mod);

    float freqMhz = (float)cfg->freq / 1e6f;
    float bwKhz = bwIndexToKhz(cfg->bw);

    ESP_LOGI(TAG, "Init: freq=%.3f MHz, sf=%u, bw=%.0f kHz, cr=%u, pwr=%d dBm",
             freqMhz, cfg->sf, bwKhz, cfg->cr, cfg->tx_power);

    /* TCXO 3.3V on DIO3, 5ms startup */
    int state = radio->begin(freqMhz, bwKhz, cfg->sf, cfg->cr, RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
                             cfg->tx_power, cfg->preamble, 3.3f);
    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGE(TAG, "begin() failed: %d", state);
        return state;
    }

    /* DIO2 as RF switch (HT-RA62 compatible) */
    radio->setDio2AsRfSwitch(true);

    /* CRC on */
    radio->setCRC(2);

    /* Set DIO1 interrupt callback */
    radio->setPacketReceivedAction(onDio1Isr);
    radio->setPacketSentAction(onDio1Isr);

    ESP_LOGI(TAG, "SX1262 init OK");
    return 0;
}

extern "C" int lora_reconfigure(const lora_config_t *cfg) {
    if (!radio) return -1;

    float freqMhz = (float)cfg->freq / 1e6f;
    float bwKhz = bwIndexToKhz(cfg->bw);

    int state = radio->setFrequency(freqMhz);
    if (state == RADIOLIB_ERR_NONE) state = radio->setBandwidth(bwKhz);
    if (state == RADIOLIB_ERR_NONE) state = radio->setSpreadingFactor(cfg->sf);
    if (state == RADIOLIB_ERR_NONE) state = radio->setCodingRate(cfg->cr);
    if (state == RADIOLIB_ERR_NONE) state = radio->setOutputPower(cfg->tx_power);
    if (state == RADIOLIB_ERR_NONE) state = radio->setPreambleLength(cfg->preamble);

    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGE(TAG, "reconfigure failed: %d", state);
    } else {
        ESP_LOGI(TAG, "Reconfigured: %.3f MHz SF%u BW%.0f CR%u %ddBm",
                 freqMhz, cfg->sf, bwKhz, cfg->cr, cfg->tx_power);
    }
    return state;
}

extern "C" int lora_transmit(const uint8_t *data, uint16_t len) {
    if (!radio) return -1;

    inTxMode = true;
    operationDone = false;

    int state = radio->transmit(const_cast<uint8_t*>(data), len);
    inTxMode = false;

    if (state == RADIOLIB_ERR_NONE) {
        ESP_LOGI(TAG, "TX OK (%u bytes)", len);
    } else {
        ESP_LOGE(TAG, "TX failed: %d", state);
    }

    return state;
}

extern "C" void lora_start_rx(lora_rx_cb_t cb) {
    if (!radio) return;
    rxCallback = cb;
    operationDone = false;
    inTxMode = false;
    radio->startReceive(RADIOLIB_SX126X_RX_TIMEOUT_NONE);
    ESP_LOGI(TAG, "RX started (continuous)");
}

extern "C" bool lora_check_rx(void) {
    if (!operationDone || inTxMode) return false;
    operationDone = false;

    uint8_t buf[256];
    size_t len = radio->getPacketLength();
    if (len > sizeof(buf)) len = sizeof(buf);

    int state = radio->readData(buf, len);

    /* Re-arm RX immediately */
    radio->startReceive(RADIOLIB_SX126X_RX_TIMEOUT_NONE);

    if (state == RADIOLIB_ERR_NONE && len > 0 && rxCallback) {
        int16_t rssi = (int16_t)(radio->getRSSI() * 10);  /* dBm x10 */
        int8_t snr = (int8_t)(radio->getSNR() * 4);       /* dB x4 */
        uint16_t freqErr = (uint16_t)abs((int)radio->getFrequencyError());
        rxCallback(buf, (uint16_t)len, rssi, snr, freqErr);
        return true;
    }

    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGW(TAG, "RX read error: %d", state);
    }

    return false;
}

extern "C" uint8_t lora_get_state(void) {
    if (inTxMode) return 2;
    if (rxCallback) return 1;
    return 0;
}
